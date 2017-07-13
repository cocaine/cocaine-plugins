#include "completion.hpp"

namespace cocaine { namespace unicat {

completion_t::completion_t(const url_t url, std::shared_ptr<auth::identity_t>& ident):
    url(url),
    identity(ident),
    done(true)
{}

completion_t::completion_t(const url_t url, std::shared_ptr<auth::identity_t>& ident, const std::error_code ec, const Opcode opcode):
    url(url),
    identity(ident),
    done(false),
    err_code(ec),
    opcode(opcode)
{}

completion_t::completion_t(const url_t url, std::shared_ptr<auth::identity_t>& ident, std::exception_ptr eptr):
    url(url),
    identity(ident),
    done(false),
    err_code{},
    eptr(std::move(eptr))
{}

auto completion_t::make_access_error() const -> std::string {
    const auto sch = scheme_to_string(url.scheme);

    switch (opcode) {
        case ReadOp:
            return cocaine::format("read permission denied for {}:{}:{} with error {}",
                sch, url.service_name, url.entity, err_code.message());
        case WriteOp:
            return cocaine::format("write permission denied for {}:{}:{} with error {}",
                sch, url.service_name, url.entity, err_code.message());
        case Nop:
        default:
            return "unknown access error";
    }
}

auto completion_t::make_exception_error() const -> std::string {
    try {
        std::rethrow_exception(eptr);
    } catch(const std::system_error& err) {
        return cocaine::format("{}: {}", err.code(), err.what());
    } catch(const std::exception& err) {
        return cocaine::format("unknown error, msg {}", err.what());
    } // TODO: else?
}

auto completion_t::has_exception() const -> bool {
    return done == false && eptr;
}

auto completion_t::has_error_code() const -> bool {
    return err_code.value();
}

auto opcode_to_string(const completion_t::Opcode opcode) -> std::string {
    switch (opcode) {
        case completion_t::Opcode::ReadOp: return "read";
        case completion_t::Opcode::WriteOp: return "write";
        case completion_t::Opcode::Nop:
        default:
            return "unknown";
    }
}

}
}
