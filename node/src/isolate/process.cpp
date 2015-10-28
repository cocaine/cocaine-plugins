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

#include <cocaine/context.hpp>
#include <cocaine/logging.hpp>

#include "cocaine/traits/endpoint.hpp"
#include "cocaine/traits/graph.hpp"
#include "cocaine/traits/siginfo.hpp"
#include "cocaine/traits/vector.hpp"

#include <array>
#include <iostream>

#include <csignal>

#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/system/system_error.hpp>

#ifdef COCAINE_ALLOW_CGROUPS
    #include <boost/lexical_cast.hpp>
    #include <libcgroup.h>
#endif

#include <sys/wait.h>

using namespace cocaine;
using namespace cocaine::isolate;

namespace fs = boost::filesystem;
namespace ph = std::placeholders;

namespace {

class process_terminator_t:
    public std::enable_shared_from_this<process_terminator_t>,
    public dispatch<io::context_tag>
{
public:
    process_terminator_t(
            pid_t pid_,
            uint kill_timeout_,
            std::unique_ptr<logging::log_t> log_,
            asio::io_service& loop
    ) :
        dispatch(format("process_terminator %i", pid)),
        log(std::move(log_)),
        pid(pid_),
        collected(false),
        kill_timeout(kill_timeout_),
        timer(loop)
    {
        on<io::context::os_signal>(std::bind(&process_terminator_t::on_signal, this, ph::_1, ph::_2));
    }

    void
    start() {
        COCAINE_LOG_INFO(log, "terminator for pid %i started", pid);
        terminate(SIGTERM);
    }

    void
    on_timer(const std::error_code& ec) {
        if(ec) {
            COCAINE_LOG_ERROR(log, "termination timer stopped: %s", ec.message());
        } else if(!try_collect()) {
            terminate(SIGKILL);
        }
    }

    void
    on_signal(int, const siginfo_t& info) {
        if(info.si_pid && info.si_pid != pid) {
            return;
        } else if(try_collect()) {
            COCAINE_LOG_INFO(log, "child with pid %d has been collected by signal", pid);
            timer.cancel();
        }
    }

    const std::unique_ptr<logging::log_t> log;

private:
    process_terminator_t(const process_terminator_t&) = delete;
    process_terminator_t& operator=(const process_terminator_t&) = delete;

    bool try_collect() {
        COCAINE_LOG_DEBUG(log, "trying to collect");
        int status = 0;
        auto rc = ::waitpid(pid, &status, WNOHANG);
        return collected.apply([&](bool& collected) {
            if(collected) {
                return true;
            }
            if((rc == -1 && errno == ECHILD) || rc != 0) {
                COCAINE_LOG_INFO(log, "child with pid %d has been collected", pid);
                collected = true;
            } else if(rc == -1) {
                COCAINE_LOG_WARNING(log, "unknow waitpid error: %i", errno);
            } else {
                COCAINE_LOG_INFO(log, "pid is still running");
            }
            return collected;
        });
    }

    void terminate(int signal) {
        collected.apply([&](const bool& collected){
            if(collected) {
                return;
            }
            ::kill(pid, signal);
            COCAINE_LOG_INFO(log, "terminating pid with %i", signal);
            timer.expires_from_now(boost::posix_time::seconds(kill_timeout));
            timer.async_wait(std::bind(&process_terminator_t::on_timer, shared_from_this(), ph::_1));
        });
    }

    pid_t pid;
    synchronized<bool> collected;
    uint kill_timeout;
    asio::deadline_timer timer;
};

struct process_handle_t:
    public api::handle_t
{
private:
    std::shared_ptr<process_terminator_t> terminator;

    const int m_stdout;

public:
    process_handle_t(pid_t pid,
                     context_t& context,
                     int stdout,
                     uint kill_timeout,
                     std::unique_ptr<logging::log_t> log,
                     asio::io_service& loop):
        terminator(std::make_shared<process_terminator_t>(pid, kill_timeout, std::move(log), loop)),
        m_stdout(stdout)
    {
        context.listen(terminator, loop);
        COCAINE_LOG_INFO(terminator->log, "process handle has been created");
    }

    ~process_handle_t() {
        terminate();
        COCAINE_LOG_INFO(terminator->log, "process handle has been destroyed");
    }

    virtual
    void
    terminate() {
        terminator->start();
    }

    virtual
    int
    stdout() const {
        return m_stdout;
    }
};

#ifdef COCAINE_ALLOW_CGROUPS
struct cgroup_configurator_t:
    public boost::static_visitor<void>
{
    cgroup_configurator_t(cgroup_controller* ptr_, const char* parameter_):
        ptr(ptr_),
        parameter(parameter_)
    { }

    void
    operator()(const dynamic_t::bool_t& value) const {
        cgroup_add_value_bool(ptr, parameter, value);
    }

    void
    operator()(const dynamic_t::int_t& value) const {
        cgroup_add_value_int64(ptr, parameter, value);
    }

    void
    operator()(const dynamic_t::uint_t& value) const {
        cgroup_add_value_uint64(ptr, parameter, value);
    }

    void
    operator()(const dynamic_t::string_t& value) const {
        cgroup_add_value_string(ptr, parameter, value.c_str());
    }

    template<class T>
    void
    operator()(const T& COCAINE_UNUSED_(value)) const {
        throw std::system_error(std::make_error_code(std::errc::invalid_argument));
    }

private:
    cgroup_controller *const ptr;

    // Parameter name is something like "cpu.shares" or "blkio.weight", i.e. it includes the name of
    // the actual controller it corresponds to.
    const char* parameter;
};

struct cgroup_category_t:
    public std::error_category
{
    virtual
    auto
    name() const throw() -> const char* {
        return "cocaine.isolate.process.cgroups";
    }

    virtual
    auto
    message(int code) const -> std::string {
        return cgroup_strerror(code);
    }
};
#endif

} // namespace

#ifdef COCAINE_ALLOW_CGROUPS
namespace cocaine { namespace error {

auto
cgroup_category() -> const std::error_category& {
    static cgroup_category_t instance;
    return instance;
}

}} // namespace cocaine::error
#endif

process_t::process_t(context_t& context, asio::io_service& io_context, const std::string& name, const dynamic_t& args):
    category_type(context, io_context, name, args),
    m_context(context),
    io_context(io_context),
    m_log(context.log(name)),
    m_name(name),
    m_working_directory(fs::path(args.as_object().at("spool", "/var/spool/cocaine").as_string()) / name),
    m_kill_timeout(args.as_object().at("kill_timeout", 5ULL).as_uint())
{
#ifdef COCAINE_ALLOW_CGROUPS
    int rv = 0;

    if((rv = cgroup_init()) != 0) {
        throw std::system_error(rv, error::cgroup_category(), "unable to initialize cgroups");
    }

    m_cgroup = cgroup_new_cgroup(m_name.c_str());

    // NOTE: Looks like if this is not done, then libcgroup will chown everything as root.
    cgroup_set_uid_gid(m_cgroup, getuid(), getgid(), getuid(), getgid());

    for(auto type = args.as_object().begin(); type != args.as_object().end(); ++type) {
        if(!type->second.is_object() || type->second.as_object().empty()) {
            continue;
        }

        cgroup_controller* ptr = cgroup_add_controller(m_cgroup, type->first.c_str());

        for(auto it = type->second.as_object().begin(); it != type->second.as_object().end(); ++it) {
            COCAINE_LOG_INFO(m_log, "setting cgroup controller '%s' parameter '%s' to '%s'",
                type->first, it->first, boost::lexical_cast<std::string>(it->second)
            );

            try {
                it->second.apply(cgroup_configurator_t(ptr, it->first.c_str()));
            } catch(const std::system_error& e) {
                COCAINE_LOG_ERROR(m_log, "unable to set cgroup controller '%s' parameter '%s' - %s",
                    type->first, it->first, e.what()
                );
            }
        }
    }

    if((rv = cgroup_create_cgroup(m_cgroup, false)) != 0) {
        cgroup_free(&m_cgroup);

        throw std::system_error(rv, error::cgroup_category(), "unable to create cgroup");
    }
#endif
}

process_t::~process_t() {
#ifdef COCAINE_ALLOW_CGROUPS
    int rv = 0;

    if((rv = cgroup_delete_cgroup(m_cgroup, false)) != 0) {
        COCAINE_LOG_ERROR(m_log, "unable to delete cgroup: %s", cgroup_strerror(rv));
    }

    cgroup_free(&m_cgroup);
#endif
}

#ifdef __APPLE__
    #include <crt_externs.h>
    #define environ (*_NSGetEnviron())
#else
    extern char** environ;
#endif

static void
closefrom_dir(boost::filesystem::path path) {
    if (!boost::filesystem::is_directory(path)) {
        throw cocaine::error_t("procfs entry must be a directory");
    }

    std::vector<int> fds;

    // Allocate at least sizeof(int) * PAGE_SIZE bytes to prevent frequent reallocations.
    fds.reserve(::getpagesize());

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

std::unique_ptr<api::handle_t>
process_t::spawn(const std::string& path, const api::string_map_t& args, const api::string_map_t& environment) {
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
        return std::make_unique<process_handle_t>(
            pid,
            m_context,
            pipes[0],
            m_kill_timeout,
            m_context.log(format("%s/process", m_name), {{ "pid", blackhole::attribute::value_t(pid) }}),
            io_context
        );
    }

    // Child initialization

    ::dup2(pipes[1], STDOUT_FILENO);
    ::dup2(pipes[1], STDERR_FILENO);

    try {
        ::close_all();
    } catch (const std::exception& e) {
        std::cerr << format("unable to close all file descriptors: %s", e.what()) << std::endl;
    }

#ifdef COCAINE_ALLOW_CGROUPS
    // Attach to the control group

    int rv = 0;

    if((rv = cgroup_attach_task(m_cgroup)) != 0) {
        std::cerr << cocaine::format("unable to attach the process to a cgroup - %s", cgroup_strerror(rv));
        std::_Exit(EXIT_FAILURE);
    }
#endif

    // Set the correct working directory

    try {
        fs::current_path(m_working_directory);
    } catch(const fs::filesystem_error& e) {
        std::cerr << cocaine::format("unable to change the working directory to '%s' - %s", m_working_directory, e.what());
        std::_Exit(EXIT_FAILURE);
    }

    // Prepare the command line and the environment

    auto target = fs::path(path);

#if BOOST_VERSION >= 104600
    if(!target.is_absolute()) {
#else
    if(!target.is_complete()) {
#endif
        target = m_working_directory / target;
    }

#if BOOST_VERSION >= 104600
    std::vector<char*> argv = { ::strdup(target.native().c_str()) }, envp;
#else
    std::vector<char*> argv = { ::strdup(target.string().c_str()) }, envp;
#endif

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

    sigset_t sigset;

    sigfillset(&sigset);

    ::sigprocmask(SIG_UNBLOCK, &sigset, nullptr);

    // Spawn the slave

    if(::execve(argv[0], argv.data(), envp.data()) != 0) {
        std::error_code ec(errno, std::system_category());
        std::cerr << cocaine::format("unable to execute '%s' - [%d] %s", path, ec.value(), ec.message());
    }

    std::_Exit(EXIT_FAILURE);
}
