#include "zookeeper.hpp"

#include <boost/algorithm/string/trim.hpp>

#include <cocaine/context.hpp>
#include <cocaine/logging.hpp>

static auto do_log(void* handler, const char* buf, size_t size) -> int {
    cocaine::logging::logger_t* log = static_cast<cocaine::logging::logger_t*>(handler);

    std::string message(buf, size);
    boost::trim(message);

    auto pos = message.find("ZOO_");
    if (pos == std::string::npos || pos + 4 >= message.size()) {
        return 0;
    }

    cocaine::logging::priorities severity = cocaine::logging::debug;
    switch (message[pos + 4]) {
    case 'D':
        severity = cocaine::logging::debug;
        break;
    case 'I':
        severity = cocaine::logging::info;
        break;
    case 'W':
        severity = cocaine::logging::warning;
        break;
    case 'E':
        severity = cocaine::logging::error;
        break;
    default:
        ;
    }

    pos = message.find(": ");
    message = message.substr(pos + 2);
    if (message.size() > 0) {
        message.front() = std::tolower(message.front());
    }
    COCAINE_LOG(log, severity, "{}", message);

    return size;
}

#ifdef __linux__

static auto memfile_read(void*, char*, size_t) -> ssize_t {
    __builtin_unreachable();
    return 0;
}

static auto memfile_write(void* handler, const char* buf, size_t size) -> ssize_t {
    return do_log(handler, buf, static_cast<size_t>(size));
}

static auto memfile_seek(void*, off64_t*, int) -> int {
    __builtin_unreachable();
    return 0;
}

#elif defined(__APPLE__)

static auto memfile_read(void*, char*, int) -> int {
    __builtin_unreachable();
    return 0;
}

static auto memfile_write(void* handler, const char* buf, int size) -> int {
    return do_log(handler, buf, static_cast<size_t>(size));
}

static auto memfile_seek(void*, fpos_t, int) -> fpos_t {
    __builtin_unreachable();
    return 0;
}

#endif

static auto memfile_close(void* handler) -> int {
    delete static_cast<cocaine::logging::logger_t*>(handler);
    return 0;
}

static auto init_logging(cocaine::logging::logger_t* log) -> FILE* {
    FILE* fh = nullptr;

#if defined(__linux__)
    cookie_io_functions_t memfile_func;
    memfile_func.read  = memfile_read;
    memfile_func.write = memfile_write;
    memfile_func.seek  = memfile_seek;
    memfile_func.close = memfile_close;

    fh = ::fopencookie(log, "a+", memfile_func);
    ::zoo_set_log_stream(fh);
#elif defined(__APPLE__)
    fh = ::funopen(log, memfile_read, memfile_write, memfile_seek, memfile_close);
    ::zoo_set_log_stream(fh);
#else
    // Do nothing on unsupported platforms.
#endif
    return fh;
}

namespace cocaine {
namespace api {

zookeeper_factory_t::zookeeper_factory_t() :
    fh(nullptr)
{}

zookeeper_factory_t::~zookeeper_factory_t() {
    if (fh) {
        ::zoo_set_log_stream(nullptr);
        ::fclose(fh);
    }
}

zookeeper_factory_t::ptr_type
zookeeper_factory_t::get(context_t& context, const std::string& name, const dynamic_t& args) {
    std::call_once(init_flag, [&] {
        fh = init_logging(context.log(cocaine::format("zookeeper")).release());
    });
    return category_traits<unicorn_t>::default_factory<unicorn::zookeeper_t>::get(context, name, args);
}

}  // namespace api
}  // namespace cocaine
