/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

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

#include <boost/lexical_cast.hpp>
#include <libcgroup.h>
#include <sys/wait.h>
#include <unistd.h>

#include "isolate.hpp"

#include <cocaine/context.hpp>
#include <cocaine/logging.hpp>

using namespace cocaine;
using namespace cocaine::api;
using namespace cocaine::isolate;

namespace {
    class process_handle_t:
        public api::handle_t
    {
        public:
            process_handle_t(pid_t pid):
                m_pid(pid)
            { }

            virtual
            ~process_handle_t() {
                terminate();
            }

            virtual
            void
            terminate() {
                int status = 0;

                if(::waitpid(m_pid, &status, WNOHANG) == 0) {
                    ::kill(m_pid, SIGTERM);
                }
            }

        private:
            pid_t m_pid;
    };
}

cgroups_t::cgroups_t(context_t& context, const std::string& name, const Json::Value& args):
    category_type(context, name, args),
    m_log(context.log(name))
{
    int rv = 0;

    if((rv = cgroup_init()) != 0) {
        boost::format message("unable to initialize the cgroups isolate - %s");
        throw configuration_error_t((message % cgroup_strerror(rv)).str());
    }
    
    m_cgroup = cgroup_new_cgroup(name.c_str());

    // TODO: Check if it changes anything.
    cgroup_set_uid_gid(m_cgroup, getuid(), getgid(), getuid(), getgid());
    
    Json::Value::Members controllers(args.getMemberNames());

    for(Json::Value::Members::iterator c = controllers.begin();
        c != controllers.end();
        ++c)
    {
        Json::Value cfg(args[*c]);

        if(!cfg.isObject() || cfg.empty()) {
            continue;
        }
        
        cgroup_controller * ctl = cgroup_add_controller(m_cgroup, c->c_str());
        
        Json::Value::Members parameters(cfg.getMemberNames());

        for(Json::Value::Members::iterator p = parameters.begin();
            p != parameters.end();
            ++p)
        {
            switch(cfg[*p].type()) {
                case Json::stringValue: {
                    cgroup_add_value_string(ctl, p->c_str(), cfg[*p].asCString());
                    break;
                } case Json::intValue: {
                    cgroup_add_value_int64(ctl, p->c_str(), cfg[*p].asInt());
                    break;
                } case Json::uintValue: {
                    cgroup_add_value_uint64(ctl, p->c_str(), cfg[*p].asUInt());
                    break;
                } case Json::booleanValue: {
                    cgroup_add_value_bool(ctl, p->c_str(), cfg[*p].asBool());
                    break;
                } default: {
                    COCAINE_LOG_WARNING(m_log, "cgroup controller '%s' parameter '%s' type is not supported", *c, *p);
                    continue;
                }
            }
            
            COCAINE_LOG_DEBUG(
                m_log,
                "setting cgroup controller '%s' parameter '%s' to '%s'", 
                *c,
                *p,
                boost::lexical_cast<std::string>(cfg[*p])
            );
        }
    }

    if((rv = cgroup_create_cgroup(m_cgroup, false)) != 0) {
        cgroup_free(&m_cgroup);
        boost::format message("unable to create the cgroup - %s");
        throw configuration_error_t((message % cgroup_strerror(rv)).str());
    }
}

cgroups_t::~cgroups_t() {
    int rv = 0;

    if((rv = cgroup_delete_cgroup(m_cgroup, false)) != 0) {
        COCAINE_LOG_ERROR(
            m_log,
            "unable to delete the cgroup - %s", 
            cgroup_strerror(rv)
        );
    }

    cgroup_free(&m_cgroup);
}

std::unique_ptr<handle_t>
cgroups_t::spawn(const std::string& path,
                 const std::map<std::string, std::string>& args)
{
    typedef std::map<
        std::string,
        std::string
    > arg_map_t;
    
    pid_t pid = ::fork();

    if(pid == 0) {
        int rv = 0;

        // Attach to the control group.
        if((rv = cgroup_attach_task(m_cgroup)) != 0) {
            COCAINE_LOG_ERROR(
                m_log,
                "unable to attach the process to the cgroup - %s",
                cgroup_strerror(rv)
            );

            std::exit(EXIT_FAILURE);
        }

        char * argv[args.size() * 2 + 2];

        // NOTE: First element is the executable path,
        // last one should be null pointer.
        argv[0] = ::strdup(path.c_str());
        argv[sizeof(argv) / sizeof(char*)] = NULL;

        arg_map_t::const_iterator it(args.begin());
        
        // NOTE: Start with the second element.
        int n = 1;
        
        while(it != args.end()) {
            argv[n++] = ::strdup(it->first.c_str());
            argv[n++] = ::strdup(it->second.c_str());
            
            ++it;
        }

        rv = ::execvp(
            argv[0],
            argv
        );

        if(rv != 0) {
            char buffer[1024];

#ifdef _GNU_SOURCE
            char * message;
            message = ::strerror_r(errno, buffer, 1024);
#else
            ::strerror_r(errno, buffer, 1024);
#endif

            COCAINE_LOG_ERROR(
                m_log,
                "unable to execute '%s' - %s",
                path,
#ifdef _GNU_SOURCE
                message
#else
                buffer
#endif
            );

            std::exit(EXIT_FAILURE);
        }
    } else if(pid < 0) {
        throw system_error_t("fork() failed");
    }
    
    return std::unique_ptr<handle_t>(
        new process_handle_t(pid)
    );
}
