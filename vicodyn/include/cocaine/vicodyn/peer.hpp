#pragma once

#include "cocaine/idl/node.hpp"

#include <asio/ip/tcp.hpp>

#include <future>

namespace cocaine {
namespace vicodyn {

class peer_t : public std::enable_shared_from_this<peer_t> {
public:
    using app_streaming_tag = io::stream_of<std::string>::tag;

    using endpoints_t = std::vector<asio::ip::tcp::endpoint>;

    ~peer_t();

    peer_t(context_t& context, std::string service_name, asio::io_service& loop, endpoints_t endpoints, std::string uuid);

    auto open_stream(std::shared_ptr<io::basic_dispatch_t> dispatch) -> io::upstream_ptr_t;

    auto connect() -> void;

    auto schedule_reconnect() -> void;

    auto uuid() const -> const std::string&;

    auto endpoints() const -> const std::vector<asio::ip::tcp::endpoint>&;

private:

    context_t& context;
    std::string service_name;
    asio::io_service& loop;
    asio::deadline_timer timer;
    std::unique_ptr<logging::logger_t> logger;
    std::shared_ptr<cocaine::session_t> session;

    struct {
        std::string uuid;
        std::vector<asio::ip::tcp::endpoint> endpoints;
    } d;

};

} // namespace vicodyn
} // namespace cocaine
