#include <libcgroup.h>

#include <boost/lexical_cast.hpp>

#include <blackhole/logger.hpp>

#include <cocaine/dynamic.hpp>
#include <cocaine/logging.hpp>

#include "cocaine/detail/isolate/process/cgroup.hpp"

namespace cocaine {
namespace error {

auto cgroup_category() -> const std::error_category& {
    static cgroup_category_t instance;
    return instance;
}

}  // namespace error

namespace isolate {

namespace {

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
    operator()(const T&) const {
        throw std::system_error(std::make_error_code(std::errc::invalid_argument));
    }

private:
    cgroup_controller *const ptr;

    // Parameter name is something like "cpu.shares" or "blkio.weight", i.e. it includes the name of
    // the actual controller it corresponds to.
    const char* parameter;
};

}

void* init_cgroups(const char* cgroup_name, const dynamic_t& args, logging::logger_t& log) {
    int rv = 0;

    if((rv = cgroup_init()) != 0) {
        throw std::system_error(rv, error::cgroup_category(), "unable to initialize cgroups");
    }

    auto cgroup_ptr = cgroup_new_cgroup(cgroup_name);

    // NOTE: Looks like if this is not done, then libcgroup will chown everything as root.
    cgroup_set_uid_gid(cgroup_ptr, getuid(), getgid(), getuid(), getgid());

    for(auto type = args.as_object().begin(); type != args.as_object().end(); ++type) {
        if(!type->second.is_object() || type->second.as_object().empty()) {
            continue;
        }

        cgroup_controller* ptr = cgroup_add_controller(cgroup_ptr, type->first.c_str());

        for(auto it = type->second.as_object().begin(); it != type->second.as_object().end(); ++it) {
            COCAINE_LOG_INFO(log, "setting cgroup controller '{}' parameter '{}' to '{}'",
                             type->first, it->first, boost::lexical_cast<std::string>(it->second)
            );

            try {
                it->second.apply(cgroup_configurator_t(ptr, it->first.c_str()));
            } catch(const std::system_error& e) {
                COCAINE_LOG_ERROR(log, "unable to set cgroup controller '{}' parameter '{}' - {}",
                                  type->first, it->first, e.what()
                );
            }
        }
    }

    if((rv = cgroup_create_cgroup(cgroup_ptr, false)) != 0) {
        cgroup_free(&cgroup_ptr);

        throw std::system_error(rv, error::cgroup_category(), "unable to create cgroup");
    }
    return reinterpret_cast<void*>(cgroup_ptr);
}

void destroy_cgroups(void* cgroup_raw_ptr, logging::logger_t& log) {
    int rv = 0;
    cgroup* cgroup_ptr = reinterpret_cast<cgroup*>(cgroup_raw_ptr);

    if((rv = cgroup_delete_cgroup(cgroup_ptr, false)) != 0) {
        COCAINE_LOG_ERROR(log, "unable to delete cgroup: {}", cgroup_strerror(rv));
    }

    cgroup_free(&cgroup_ptr);
}

void attach_cgroups(void* cgroup_raw_ptr, logging::logger_t& log) {
    cgroup* cgroup_ptr = reinterpret_cast<cgroup*>(cgroup_raw_ptr);

    // Attach to the control group
    int rv = 0;

    if((rv = cgroup_attach_task(cgroup_ptr)) != 0) {
        COCAINE_LOG_ERROR(log, "unable to attach the process to a cgroup - {}", cgroup_strerror(rv));
        std::cerr << cocaine::format("unable to attach the process to a cgroup - {}", cgroup_strerror(rv));
        std::_Exit(EXIT_FAILURE);
    }
}

const char* get_cgroup_error(int code) {
    return cgroup_strerror(code);
}

} // namespace isolate
} // namespace cocaine
