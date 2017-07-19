#pragma once

#include <cocaine/api/service.hpp>
#include <cocaine/rpc/dispatch.hpp>

#include "cocaine/idl/uniresis.hpp"

namespace cocaine {
namespace service {

class uniresis_t : public api::service_t , dispatch<io::uniresis_tag> {
public:
    uniresis_t(context_t& context, asio::io_service& loop, const std::string& name, const dynamic_t& args);

    auto
    prototype() -> io::basic_dispatch_t& {
        return *this;
    }
};

} // namespace uniresis
} // namespace cocaine
