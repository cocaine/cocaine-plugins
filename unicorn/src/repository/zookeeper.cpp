#include "zookeeper.hpp"

#include <cocaine/context.hpp>
#include <cocaine/logging.hpp>

static int do_log(void* handler, const char* buf, size_t size) {
    cocaine::logging::logger_t* log = static_cast<cocaine::logging::logger_t*>(handler);

    std::string message(buf, size);
    while (message.size() > 0 && message.back() == '\n') {
        message.resize(message.size() - 1);
    }

    auto pos = message.find("ZOO_");
    if (pos == std::string::npos || pos + 4 >= message.size()) {
        return 0;
    }

    int severity = 0;
    switch (message[pos + 4]) {
    case 'D':
        severity = 0;
        break;
    case 'I':
        severity = 1;
        break;
    case 'W':
        severity = 2;
        break;
    case 'E':
        severity = 3;
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

static ssize_t memfile_read(void*, char*, size_t) {
    std::terminate();
    return 0;
}

static ssize_t memfile_write(void* handler, const char* buf, size_t size) {
    return do_log(handler, buf, static_cast<size_t>(size));
}

static int memfile_seek(void*, off64_t*, int) {
    std::terminate();
    return 0;
}

#elif defined(__APPLE__)

static int memfile_read(void*, char*, int) {
    std::terminate();
    return 0;
}

static int memfile_write(void* handler, const char* buf, int size) {
    return do_log(handler, buf, static_cast<size_t>(size));
}

static fpos_t memfile_seek(void*, fpos_t, int) {
    std::terminate();
    return 0;
}

#endif

static int memfile_close(void* handler) {
    ::free(handler);
    return 0;
}

static FILE* init_logging(cocaine::logging::logger_t* log) {
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

zookeeper_factory_t::zookeeper_factory_t() {}
zookeeper_factory_t::~zookeeper_factory_t() {
    ::fclose(fh);
}

zookeeper_factory_t::ptr_type
zookeeper_factory_t::get(context_t& context, const std::string& name, const dynamic_t& args) {
    if (initialized.test_and_set() == false) {
        fh = init_logging(context.log(cocaine::format("zookeeper")).release());
    }
    return category_traits<unicorn_t>::default_factory<unicorn::zookeeper_t>::get(context, name, args);
}

}  // namespace api
}  // namespace cocaine
