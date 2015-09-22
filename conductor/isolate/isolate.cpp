/*
    Copyright (c) 2011-2013 Dmitry Unkovsky <diunko@yandex-team.ru>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "isolate.hpp"

#include <cocaine/context.hpp>
#include <cocaine/logging.hpp>
#include <cocaine/memory.hpp>

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <system_error>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/lexical_cast.hpp>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cocaine/rpc/actor.hpp>

#include "cocaine/conductor.hpp"

using namespace cocaine;
using namespace cocaine::isolate;

namespace fs = boost::filesystem;

namespace {

struct container_handle_t:
    public api::handle_t
{
    COCAINE_DECLARE_NONCOPYABLE(container_handle_t)

    container_handle_t(std::shared_ptr<rpc_isolate_t> parent):
        m_parent(parent),
        m_terminated(false),
        m_container_id(1234567890),
        m_fd(0)
    { }

    virtual
   ~container_handle_t() {
        std::cout << "container_handle_t is being destroyed" << std::endl;
        terminate();
    }

    virtual
    void
    terminate() {
        if(!m_terminated) {
            m_terminated = true;
            
            m_parent->terminate(m_container_id);
        }
    }

    virtual
    int
    stdout() const {
        return m_fd;
    }

private:
    std::shared_ptr<rpc_isolate_t> m_parent;
    bool m_terminated;
    uint64_t m_container_id;
    int m_fd;
};

} // namespace

rpc_isolate_t::rpc_isolate_t(context_t& context,  asio::io_service& io_context, const std::string& name, const dynamic_t& args):
    category_type(context, io_context, name, args),
    m_log(context.log(name, {{"isolate", "rpc"}})),
    m_do_pool(false),
    m_context(context),
    m_args(args)
{
    try {
        const auto& config = args.as_object();
        m_runtime_path = config.at("runtime-path", "/run/cocaine").as_string();

        if(config.count("registry") > 0) {
            m_do_pool = true;

            // <default> means default docker's registry (the one owned by dotCloud). User must specify it explicitly.
            // I think that if there is no registry in the config then the plugin must not pull images from foreign registry by default.
            if(config["registry"].as_string() != "<default>") {
                m_image += config["registry"].as_string() + "/";
            }
        } else {
            COCAINE_LOG_WARNING(m_log, "docker registry is not specified - only local images will be used");
        }

        if(config.count("repository") > 0) {
            m_image += config["repository"].as_string() + "/";
        }
        m_image += name;
        m_tag = ""; // empty for now

    } catch(const std::exception& e) {
        throw cocaine::error_t("%s", e.what());
    }
}

rpc_isolate_t::~rpc_isolate_t() {
    COCAINE_LOG_DEBUG(m_log, "isolate is being destroyed");
}

std::unique_ptr<api::cancellation_t>
rpc_isolate_t::async_spool(std::function<void(const std::error_code&)> handler) {
    if(m_do_pool) {
        COCAINE_LOG_INFO(m_log, "isolate::spool(%s)", m_image);

        auto actor = m_context.locate("conductor");

        if (!actor) {
            COCAINE_LOG_ERROR(m_log, "unable to spool container: conductor is not available");
            throw cocaine::error_t("unable to spool container: conductor is not available");
        }

        auto conductor = dynamic_cast<const service::conductor_t*>(&actor.get().prototype());
        
        const_cast<service::conductor_t*>(conductor)->spool(m_image, [&] (const std::error_code& ec) {

            COCAINE_LOG_DEBUG(m_log, "isolate: spool done");
            
            get_io_service().post(std::bind(handler, std::error_code(errno, std::system_category())));

        });
    }
    return std::make_unique<api::cancellation_t>();
}

void
rpc_isolate_t::async_spawn(const std::string& path_,
                           const api::string_map_t& args_,
                           const api::string_map_t& environment_,
                           api::spawn_handler_t handler)
{
    try {
        COCAINE_LOG_INFO(m_log, "isolate::spawn(%s/%s)", m_image, path_);

        auto actor = m_context.locate("conductor");

        if (!actor) {
            COCAINE_LOG_ERROR(m_log, "unable to spawn container: conductor is not available");
            throw cocaine::error_t("unable to spawn container: conductor is not available");
        }

        auto conductor = dynamic_cast<const service::conductor_t*>(&actor.get().prototype());

        api::string_map_t isolate_params {{"isolate", "porto"}, {"slave", path_}};
        api::string_map_t args (args_);
        api::string_map_t environment (environment_);
        
        // void spawn(std::string image,
        //            std::string path,
        //            api::string_map_t&& isolate_params,
        //            api::string_map_t&& args,
        //            api::string_map_t&& environment,
        //            std::function<void(const std::error_code&)> handler);

        auto self = shared_from_this();

        const_cast<service::conductor_t*>(conductor)->spawn(
            m_image,
            path_,
            std::move(isolate_params),
            std::move(args),
            std::move(environment),
            [this, self, handler] (const std::error_code& ec) {

                if (ec){
                    COCAINE_LOG_ERROR(m_log, "spawn error");
                } else {
                    COCAINE_LOG_DEBUG(m_log, "spawn reported ok");
                }

                auto lambda = [this, self, handler, &ec] {
                    std::unique_ptr<api::handle_t> handle;

                    std::cout << "inner lambda" << std::endl;

                    if (!ec){
                        COCAINE_LOG_DEBUG(m_log, "creating container handle");
                        handle.reset(new container_handle_t(self));
                    }

                    handler(ec, handle);
                };

                get_io_service().post(lambda);

            }
        );

    } catch(const std::exception& e) {
        throw cocaine::error_t("%s", e.what());
    }
}

