/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/detail/isolate/process.hpp"
#include "cocaine/detail/isolate/archive.hpp"
#include "cocaine/detail/isolate/fetcher.hpp"
#include "cocaine/detail/isolate/process/cgroup.hpp"

#include <cocaine/api/storage.hpp>
#include <cocaine/context.hpp>
#include <cocaine/dynamic.hpp>
#include <cocaine/errors.hpp>
#include <cocaine/logging.hpp>

#include <array>
#include <iostream>

#include <cassert>
#include <csignal>

#include <asio/deadline_timer.hpp>

#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/system/system_error.hpp>

#include <sys/wait.h>

#include <blackhole/logger.hpp>
#include <blackhole/wrapper.hpp>

#ifdef __APPLE__
#include <crt_externs.h>
#define environ (*_NSGetEnviron())
#else
extern char** environ;
#endif

namespace cocaine {
namespace isolate {

namespace ph = std::placeholders;
namespace fs = boost::filesystem;

namespace {

class process_terminator_t:
    public std::enable_shared_from_this<process_terminator_t>,
    public api::cancellation_t
{
public:
    const std::unique_ptr<logging::logger_t> log;

private:
    pid_t pid;

    struct {
        uint kill;
        uint gc;
    } timeout;

    asio::deadline_timer timer;
    std::shared_ptr<fetcher_t> fetcher;

public:
    process_terminator_t(pid_t pid_,
                         uint kill_timeout,
                         std::unique_ptr<logging::logger_t> log_,
                         asio::io_service& loop,
                         std::shared_ptr<fetcher_t> _fetcher
    ):
    log(std::move(log_)),
    pid(pid_),
    timer(loop),
    fetcher(std::move(_fetcher))
    {
        timeout.kill = kill_timeout;
        timeout.gc   = 5;
    }

    ~process_terminator_t() {
        COCAINE_LOG_DEBUG(log, "process terminator is destroying");

        if (pid) {
            int status = 0;

            switch (::waitpid(pid, &status, WNOHANG)) {
                case -1: {
                    // Some error occurred, check errno.
                    const int ec = errno;

                    COCAINE_LOG_WARNING(log, "unable to properly collect the child: {}", ec);
                }
                    break;
                case 0:
                    // The child is not finished yet, kill it and collect in a blocking way as as last
                    // resort to prevent zombies.
                    if (::kill(pid, SIGKILL) == 0) {
                        if (::waitpid(pid, &status, 0) > 0) {
                            COCAINE_LOG_DEBUG(log, "child has been killed: {}", status);
                        } else {
                            const int ec = errno;

                            COCAINE_LOG_WARNING(log, "unable to properly collect the child: {}", ec);
                        }
                    } else {
                        // Unable to kill for some reasons, check errno.
                        const int ec = errno;

                        COCAINE_LOG_WARNING(log, "unable to send kill signal to the child: {}", ec);
                    }
                    break;
                default:
                    COCAINE_LOG_DEBUG(log, "child has been collected: {}", status);
            }
        }
        fetcher->close();
    }

    void
    cancel() noexcept {
        int status = 0;

        // Attempt to collect the child non-blocking way.
        switch (::waitpid(pid, &status, WNOHANG)) {
            case -1: {
                const int ec = errno;

                COCAINE_LOG_WARNING(log, "unable to collect the child: {}", ec);
                break;
            }
            case 0: {
                // The child is not finished yet, send SIGTERM and try to collect it later after.
                COCAINE_LOG_DEBUG(log, "unable to terminate child right now (not ready), sending SIGTERM", {
                    {"timeout", timeout.kill}
                });

                // Ignore return code here.
                ::kill(pid, SIGTERM);

                timer.expires_from_now(boost::posix_time::seconds(timeout.kill));
                timer.async_wait(std::bind(&process_terminator_t::on_kill_timer, shared_from_this(), ph::_1));
                break;
            }
            default:
                COCAINE_LOG_DEBUG(log, "child has been stopped: {}", status);

                pid = 0;
        }
    }

private:
    void
    on_kill_timer(const std::error_code& ec) {
        if(ec == asio::error::operation_aborted) {
            COCAINE_LOG_DEBUG(log, "process kill timer has called its completion handler: cancelled");
            return;
        } else {
            COCAINE_LOG_DEBUG(log, "process kill timer has called its completion handler");
        }

        int status = 0;

        switch (::waitpid(pid, &status, WNOHANG)) {
            case -1: {
                const int ec = errno;

                COCAINE_LOG_WARNING(log, "unable to collect the child: {}", ec);
                break;
            }
            case 0: {
                COCAINE_LOG_DEBUG(log, "killing the child, resuming after 5 sec");

                // Ignore return code here too.
                ::kill(pid, SIGKILL);

                timer.expires_from_now(boost::posix_time::seconds(timeout.gc));
                timer.async_wait(std::bind(&process_terminator_t::on_gc_action, shared_from_this(), ph::_1));
                break;
            }
            default:
                COCAINE_LOG_DEBUG(log, "child has been terminated: {}", status);

                pid = 0;
        }
    }

    void
    on_gc_action(const std::error_code& ec) {
        if(ec == asio::error::operation_aborted) {
            COCAINE_LOG_DEBUG(log, "process GC timer has called its completion handler: cancelled");
            return;
        } else {
            COCAINE_LOG_DEBUG(log, "process GC timer has called its completion handler");
        }

        int status = 0;

        switch (::waitpid(pid, &status, WNOHANG)) {
            case -1: {
                const int ec = errno;

                COCAINE_LOG_WARNING(log, "unable to collect the child: {}", ec);
                break;
            }
            case 0: {
                COCAINE_LOG_DEBUG(log, "child has not been killed, resuming after 5 sec");

                timer.expires_from_now(boost::posix_time::seconds(timeout.gc));
                timer.async_wait(std::bind(&process_terminator_t::on_gc_action, shared_from_this(), ph::_1));
                break;
            }
            default:
                COCAINE_LOG_DEBUG(log, "child has been killed: {}", status);

                pid = 0;
        }
    }
};

static void
closefrom_dir(boost::filesystem::path path) {
    if (!boost::filesystem::is_directory(path)) {
        throw cocaine::error_t("procfs entry must be a directory");
    }

    std::vector<int> fds;

    // Allocate at least sizeof(int) * PAGE_SIZE bytes to prevent frequent reallocations.
    fds.reserve(static_cast<std::size_t>(::getpagesize()));

    const boost::filesystem::directory_iterator end;
    for (const auto& entry : boost::make_iterator_range(boost::filesystem::directory_iterator(path), end)) {
        const auto filename = entry.path().filename().native();

        try {
            int fd = boost::lexical_cast<int>(filename);

            if (fd < 3) {
                // Do not close STDIN, STDOUT and STDERR.
                continue;
            }

            fds.push_back(fd);
        } catch (const boost::bad_lexical_cast&) {
            // Unable to parse. Not sure it's possible.
        }
    }

    for (int fd : fds) {
        if (::close(fd) < 0) {
            // Ignore.
        }
    }
}

static void
close_all() {
    // FreeBSD has `closefrom` syscall, but we haven't. The only effective solution is to iterate
    // over procfs entries.

#if defined(__linux__)
    closefrom_dir("/proc/self/fd/");
#elif defined(__APPLE__)
    closefrom_dir("/dev/fd");
#else
    throw std::system_error(ENOTSUP, std::system_category());
#endif
}

}

process_t::process_t(context_t& context, asio::io_service& io_context, const std::string& name, const std::string& type, const dynamic_t& args):
    category_type(context, io_context, name, type, args),
    m_context(context),
    io_context(io_context),
    m_log(context.log(name)),
    m_name(name),
    m_working_directory(fs::path(args.as_object().at("spool", "/var/spool/cocaine").as_string()) / name),
    m_kill_timeout(args.as_object().at("kill_timeout", 5ULL).as_uint())
{
    m_cgroup = init_cgroups(m_name.c_str(), args, *m_log);
}

process_t::~process_t() {
    destroy_cgroups(m_cgroup, *m_log);
}

std::unique_ptr<api::cancellation_t>
process_t::spool(std::shared_ptr<api::spool_handle_base_t> handler) {
    COCAINE_LOG_INFO(m_log, "deploying app to {}", m_working_directory);

    const auto storage = api::storage(m_context, "core");

    //TODO: maybe we can change to async, but as far as this isolation is a legacy one there is no need right now
    const auto archive = storage->get<std::string>("apps", m_name).get();

    archive_t(m_context, archive).deploy(m_working_directory.native());
    io_context.post([=](){
        handler->on_ready();
    });
    return std::unique_ptr<api::cancellation_t>(new api::cancellation_t);
}

std::unique_ptr<api::cancellation_t>
process_t::spawn(const std::string& path,
            const api::args_t& args,
            const api::env_t& environment,
            std::shared_ptr<api::spawn_handle_base_t> handle)
{
    std::array<int, 2> pipes;

    if(::pipe(pipes.data()) != 0) {
        throw std::system_error(errno, std::system_category(), "unable to create an output pipe");
    }

    for(auto it = pipes.begin(); it != pipes.end(); ++it) {
        ::fcntl(*it, F_SETFD, FD_CLOEXEC);
    }

    const pid_t pid = ::fork();

    if(pid < 0) {
        std::for_each(pipes.begin(), pipes.end(), ::close);

        throw std::system_error(errno, std::system_category(), "unable to fork");
    }

    ::close(pipes[pid > 0]);

    if(pid > 0) {
        io_context.post([=](){
            handle->on_ready();
        });
        std::unique_ptr<logging::logger_t> fetcher_logger(m_context.log("process_fetcher"));
        std::unique_ptr<logging::logger_t> terminator_logger(m_context.log("process_terminator"));
        auto fetcher = std::make_shared<fetcher_t>(io_context, handle, std::move(fetcher_logger));
        fetcher->assign(pipes[0]);
        std::unique_ptr<api::cancellation_t> terminator(new process_terminator_t(
            pid,
            m_kill_timeout,
            std::move(terminator_logger),
            io_context,
            std::move(fetcher)
        ));
        return terminator;
    }

    // Child initialization

    ::dup2(pipes[1], STDOUT_FILENO);
    ::dup2(pipes[1], STDERR_FILENO);

    try {
        close_all();
    } catch (const std::exception& e) {
        std::cerr << format("unable to close all file descriptors: {}", e.what()) << std::endl;
    }

    attach_cgroups(m_cgroup, *m_log);

    // Set the correct working directory

    try {
        fs::current_path(m_working_directory);
    } catch(const fs::filesystem_error& e) {
        std::cerr << cocaine::format("unable to change the working directory to '{}' - {}", m_working_directory, e.what());
        std::_Exit(EXIT_FAILURE);
    }

    // Prepare the command line and the environment

    auto target = fs::path(path);

    if(!target.is_absolute()) {
        target = m_working_directory / target;
    }

    std::vector<char*> argv = { ::strdup(target.native().c_str()) }, envp;

    for(auto it = args.begin(); it != args.end(); ++it) {
        argv.push_back(::strdup(it->first.c_str()));
        argv.push_back(::strdup(it->second.c_str()));
    }

    argv.push_back(nullptr);

    for(char** ptr = environ; *ptr != nullptr; ++ptr) {
        envp.push_back(::strdup(*ptr));
    }

    for(auto it = environment.begin(); it != environment.end(); ++it) {
        envp.push_back(::strdup(cocaine::format("{}={}", it->first, it->second).c_str()));
    }

    envp.push_back(nullptr);

    // Unblock all the signals

    sigset_t sigset;

    sigfillset(&sigset);

    ::sigprocmask(SIG_UNBLOCK, &sigset, nullptr);

    // Spawn the slave

    if(::execve(argv[0], argv.data(), envp.data()) != 0) {
        std::error_code ec(errno, std::system_category());
        std::cerr << cocaine::format("unable to execute '{}' - [{}] {}", path, ec.value(), ec.message());
    }

    std::_Exit(EXIT_FAILURE);
}

void
process_t::metrics(const dynamic_t&, std::shared_ptr<api::metrics_handle_base_t> handle) const
{
    assert(handle);
    return handle->on_data({});
}

} // namespace isolate
} // namespace cocaine
