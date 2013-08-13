#pragma once

#include <swarm/networkrequest.h>
#include <swarm/networkmanager.h>

#include <cocaine/api/service.hpp>
#include <cocaine/asio/reactor.hpp>

#include "cocaine/io/protocol.hpp"

namespace cocaine { namespace service {

namespace Response {
typedef io::event_traits<io::elasticsearch::get>::result_type Get;
typedef io::event_traits<io::elasticsearch::index>::result_type Index;
}

class elasticsearch_t : public api::service_t {
    ioremap::swarm::network_manager m_manager;
    std::shared_ptr<logging::log_t> m_log;

public:
    elasticsearch_t(context_t &context, io::reactor_t &reactor, const std::string& name, const Json::Value& args);

    cocaine::deferred<Response::Get> get(const std::string &index);
    cocaine::deferred<Response::Index> index(const std::string &index, const std::string &data);

private:
};

} }
