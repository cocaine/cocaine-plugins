#pragma once

#include <cocaine/api/service.hpp>
#include <cocaine/rpc/dispatch.hpp>

#include "cocaine/idl/echo.hpp"

namespace cocaine {
namespace service {

class echo_t : public api::service_t, public dispatch<io::echo_tag> {
public:
    echo_t(context_t& context, asio::io_service& asio, const std::string& name, const dynamic_t& args);

    auto
    prototype() const -> const io::basic_dispatch_t& {
        return *this;
    }
};

}  // namespace service
}  // namespace cocaine
