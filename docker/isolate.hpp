#ifndef COCAINE_DOCKER_ISOLATE_HPP
#define COCAINE_DOCKER_ISOLATE_HPP

#include "docker_client.hpp"

#include <cocaine/api/isolate.hpp>

namespace cocaine { namespace isolate {

class docker_t:
    public api::isolate_t
{
    COCAINE_DECLARE_NONCOPYABLE(docker_t)

public:
    typedef api::isolate_t category_type;

public:
    docker_t(context_t& context,
             const std::string& name,
             const Json::Value& args);

    virtual
   ~docker_t();

    virtual
    std::unique_ptr<api::handle_t>
    spawn(const std::string& path,
          const api::string_map_t& args,
          const api::string_map_t& environment);

private:
    std::shared_ptr<cocaine::logging::log_t> m_log;

    std::string m_rundir;

    docker::client_t m_docker_client;
    rapidjson::Value m_run_config;
    rapidjson::Value::AllocatorType m_json_allocator;
};

}} // namespace cocaine::storage

#endif // COCAINE_DOCKER_ISOLATE_HPP
