/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
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

#include <libcgroup.h>

using namespace cocaine;
using namespace cocaine::isolate;

namespace fs = boost::filesystem;

namespace {

struct process_handle_t:
    public api::handle_t
{
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
    const pid_t m_pid;
};

}

cgroups_t::cgroups_t(context_t& context, const std::string& name, const Json::Value& args):
    category_type(context, name, args),
    m_log(new logging::log_t(context, name)),
#if BOOST_VERSION >= 104600
    m_working_directory((fs::path(context.config.path.spool) / name).native())
#else
    m_working_directory((fs::path(context.config.path.spool) / name).string())
#endif
{
    int rv = 0;

    if((rv = cgroup_init()) != 0) {
        throw cocaine::error_t("unable to initialize the cgroups isolate - %s", cgroup_strerror(rv));
    }

    m_cgroup = cgroup_new_cgroup(name.c_str());

    // TODO: Check if it changes anything.
    cgroup_set_uid_gid(m_cgroup, getuid(), getgid(), getuid(), getgid());

    Json::Value::Members controllers(args.getMemberNames());

    for(auto c = controllers.begin(); c != controllers.end(); ++c) {
        Json::Value cfg(args[*c]);

        if(!cfg.isObject() || cfg.empty()) {
            continue;
        }

        cgroup_controller * ctl = cgroup_add_controller(m_cgroup, c->c_str());

        Json::Value::Members parameters(cfg.getMemberNames());

        for(auto p = parameters.begin(); p != parameters.end(); ++p) {
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
        throw cocaine::error_t("unable to create the cgroup - %s", cgroup_strerror(rv));
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

std::unique_ptr<api::handle_t>
cgroups_t::spawn(const std::string& path, const api::string_map_t& args, const api::string_map_t& environment, int pipe) {
    const pid_t pid = ::fork();

    if(pid < 0) {
        throw std::system_error(errno, std::system_category(), "unable to fork");
    }

    if(pid > 0) {
        return std::make_unique<process_handle_t>(pid);
    }

    ::dup2(pipe, STDOUT_FILENO);
    ::dup2(pipe, STDERR_FILENO);

    // Attach to the control group

    int rv = 0;

    if((rv = cgroup_attach_task(m_cgroup)) != 0) {
        std::cerr << cocaine::format("unable to attach the process to a cgroup - %s", cgroup_strerror(rv));
        std::_Exit(EXIT_FAILURE);
    }

    // Set the correct working directory

    try {
        fs::current_path(m_working_directory);
    } catch(const fs::filesystem_error& e) {
        std::cerr << cocaine::format("unable to change the working directory to '%s' - %s", path, e.what());
        std::_Exit(EXIT_FAILURE);
    }

    // Prepare the command line and the environment

    std::vector<char*> argv = { ::strdup(path.c_str()) }, envp;

    for(auto it = args.begin(); it != args.end(); ++it) {
        argv.push_back(::strdup(it->first.c_str()));
        argv.push_back(::strdup(it->second.c_str()));
    }

    argv.push_back(nullptr);

    for(char** ptr = environ; *ptr != nullptr; ++ptr) {
        envp.push_back(::strdup(*ptr));
    }

    boost::format format("%s=%s");

    for(auto it = environment.begin(); it != environment.end(); ++it, format.clear()) {
        envp.push_back(::strdup((format % it->first % it->second).str().c_str()));
    }

    envp.push_back(nullptr);

    // Unblock all the signals

    sigset_t signals;

    sigfillset(&signals);

    ::sigprocmask(SIG_UNBLOCK, &signals, nullptr);

    // Spawn the slave

    if(::execve(argv[0], argv.data(), envp.data()) != 0) {
        std::error_code ec(errno, std::system_category());
        std::cerr << cocaine::format("unable to execute '%s' - %s", path, ec.message());
    }

    std::_Exit(EXIT_FAILURE);
}
