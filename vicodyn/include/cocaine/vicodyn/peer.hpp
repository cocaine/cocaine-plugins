#pragma once

#include "cocaine/idl/node.hpp"

#include "cocaine/format/endpoint.hpp"

#include <cocaine/executor/asio.hpp>
#include <cocaine/forwards.hpp>
#include <cocaine/locked_ptr.hpp>
#include <cocaine/logging.hpp>

#include <asio/ip/tcp.hpp>

#include <future>

namespace cocaine {
namespace vicodyn {

class peer_t : public std::enable_shared_from_this<peer_t> {
public:
    using app_streaming_tag = io::stream_of<std::string>::tag;

    using endpoints_t = std::vector<asio::ip::tcp::endpoint>;

    ~peer_t();

    peer_t(context_t& context, asio::io_service& loop, endpoints_t endpoints, std::string uuid);

    auto open_stream(std::shared_ptr<io::basic_dispatch_t> dispatch) -> io::upstream_ptr_t;

    auto connect() -> void;

    auto schedule_reconnect() -> void;

    auto uuid() const -> const std::string&;

    auto endpoints() const -> const std::vector<asio::ip::tcp::endpoint>&;

    auto connected() const -> bool;

    auto last_active() const -> std::chrono::system_clock::time_point;

private:
    context_t& context;
    std::string service_name;
    asio::io_service& loop;
    asio::deadline_timer timer;
    std::unique_ptr<logging::logger_t> logger;
    synchronized<std::shared_ptr<cocaine::session_t>> session;
    bool connecting;

    struct {
        std::string uuid;
        std::vector<asio::ip::tcp::endpoint> endpoints;
        std::chrono::system_clock::time_point last_active;
    } d;

};

// thread safe wrapper on map of peers indexed by uuid
class peers_t {
public:
    using endpoints_t = std::vector<asio::ip::tcp::endpoint>;
    using peers_data_t = std::map<std::string, std::shared_ptr<peer_t>>;
    using app_data_t = std::map<std::string, std::vector<std::string>>;

    struct data_t {
        peers_data_t peers;
        app_data_t apps;
    };


private:
    context_t& context;
    std::unique_ptr<logging::logger_t> logger;
    executor::owning_asio_t executor;
    synchronized<data_t> data;


public:
    peers_t(context_t& context);

    auto register_peer(const std::string& uuid, const endpoints_t& endpoints) -> std::shared_ptr<peer_t>;
    auto register_peer(const std::string& uuid, std::shared_ptr<peer_t> peer) -> void;

    auto erase_peer(const std::string& uuid) -> void;

    auto register_app( const std::string& uuid, const std::string& name) -> void;

    auto erase_app(const std::string& uuid, const std::string& name) -> void;

    auto erase(const std::string& uuid) -> void;

    auto inner() -> synchronized<data_t>&;
};

} // namespace vicodyn
} // namespace cocaine
