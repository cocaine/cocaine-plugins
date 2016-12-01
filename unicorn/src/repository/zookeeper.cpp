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

static auto memfile_read(void*, char*, size_t) -> ssize_t {
    std::terminate();
    return 0;
}

static auto memfile_write(void* handler, const char* buf, size_t size) -> ssize_t {
    return do_log(handler, buf, static_cast<size_t>(size));
}

static auto memfile_seek(void*, off64_t*, int) -> int {
    std::terminate();
    return 0;
}

#elif defined(__APPLE__)

static auto memfile_read(void*, char*, int) -> int {
    std::terminate();
    return 0;
}

static auto memfile_write(void* handler, const char* buf, int size) -> int {
    return do_log(handler, buf, static_cast<size_t>(size));
}

static auto memfile_seek(void*, fpos_t, int) -> fpos_t {
    std::terminate();
    return 0;
}

#endif

static auto memfile_close(void* handler) -> int {
    delete static_cast<cocaine::logging::logger_t*>(handler);
    return 0;
}

namespace {

class initializer_t {
    FILE* fh;

public:
    initializer_t() :
        fh(nullptr)
    {}

    ~initializer_t() {
       if (fh) {
           ::fclose(fh);
           ::zoo_set_log_stream(nullptr);
       }
   }

    auto init(cocaine::logging::logger_t* log) -> void {
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
    }

    static auto instance() -> initializer_t& {
        static initializer_t self;
        return self;
    }
};

std::once_flag init_once;

}  // namespace

namespace cocaine {
namespace api {

zookeeper_factory_t::ptr_type
zookeeper_factory_t::get(context_t& context, const std::string& name, const dynamic_t& args) {
    std::call_once(init_once, [&] {
        // Wrapper pointer will be released at static destruction time.
        initializer_t::instance().init(context.log(cocaine::format("zookeeper")).release());
    });
    return category_traits<unicorn_t>::default_factory<unicorn::zookeeper_t>::get(context, name, args);
}

}  // namespace api
}  // namespace cocaine
