#pragma once

#include <memory>
#include <string>

#include <cocaine/api/service.hpp>
#include <cocaine/rpc/dispatch.hpp>

#include "cocaine/idl/unicat.hpp"

namespace cocaine {
namespace service {

class unicat_t : public api::service_t, public dispatch<io::unicat_tag> {
public:
    unicat_t(context_t& context, asio::io_service& asio, const std::string& name, const dynamic_t& args);

    auto prototype() -> io::basic_dispatch_t& {
        return *this;
    }
};

}  // namespace service
}  // namespace cocaine
