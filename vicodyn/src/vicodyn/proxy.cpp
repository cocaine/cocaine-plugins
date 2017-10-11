#include "cocaine/vicodyn/proxy.hpp"

#include "cocaine/api/vicodyn/balancer.hpp"
#include "cocaine/format/ptr.hpp"
#include "cocaine/format/peer.hpp"
#include "cocaine/repository/vicodyn/balancer.hpp"

#include "cocaine/vicodyn/peer.hpp"
#include "cocaine/service/node/slave/error.hpp"

#include <cocaine/context.hpp>
#include <cocaine/dynamic.hpp>
#include <cocaine/errors.hpp>
#include <cocaine/format.hpp>
#include <cocaine/format/exception.hpp>
#include <cocaine/logging.hpp>

#include <blackhole/logger.hpp>
#include <blackhole/wrapper.hpp>
#include <cocaine/rpc/slot.hpp>

#include <cocaine/traits/map.hpp>

namespace cocaine {
namespace vicodyn {

using app_tag = io::stream_of<std::string>::tag;

class forward_dispatch_t : public dispatch<app_tag>, public std::enable_shared_from_this<forward_dispatch_t> {
    using protocol = io::protocol<app_tag>::scope;

    friend class backward_dispatch_t;
    proxy_t& proxy;
    std::unique_ptr<logging::logger_t> logger;
    std::shared_ptr<peer_t> peer;
    std::shared_ptr<dispatch<app_tag>> backward_dispatch;

    struct data_t {

        upstream<io::stream_of<std::string>::tag> backward_stream;

        io::upstream_ptr_t forward_stream;
        std::string enqueue_frame;
        hpack::headers_t enqueue_headers;

        std::vector<std::string> chunks;
        std::vector<hpack::headers_t> chunk_headers;

        bool choke_sent;
        hpack::headers_t choke_headers;

        bool buffering_enabled;

        size_t retry_counter;


        data_t(upstream<app_tag> backward_stream):
            backward_stream(std::move(backward_stream)),
            choke_sent(false),
            buffering_enabled(true),
            retry_counter(0)
        {}
    };

    synchronized<data_t> d;

public:
    forward_dispatch_t(proxy_t& _proxy, const std::string& name, upstream<app_tag> b_stream,
                       std::shared_ptr<dispatch<app_tag>> b_dispatch, std::shared_ptr<peer_t> _peer) :
        dispatch(name),
        proxy(_proxy),
        logger(new blackhole::wrapper_t(*proxy.logger, {{"peer", _peer->uuid()}})),
        peer(std::move(_peer)),
        backward_dispatch(std::move(b_dispatch)),
        d(std::move(b_stream))
    {
        on<protocol::chunk>().execute([this](const hpack::headers_t& headers, std::string chunk){
            COCAINE_LOG_DEBUG(logger, "processing chunk in");
            d.apply([&](data_t& d){
                if(!d.forward_stream) {
                    COCAINE_LOG_WARNING(logger, "skipping sending chunk - forward stream is missing"
                        "(this can happen if enqueue was unsuccessfull)");
                    return;
                }
                try {
                    d.chunks.push_back(std::move(chunk));
                    d.chunk_headers.push_back(headers);
                    d.forward_stream->send<protocol::chunk>(headers, d.chunks.back());
                } catch (const std::system_error& e) {
                    COCAINE_LOG_WARNING(logger, "failed to send chunk to forward dispatch - {}", error::to_string(e));
                    d.backward_stream.send<protocol::error>(e.code(), "failed to send chunk to forward dispatch");
                    peer->schedule_reconnect();
                }
            });
        });

        on<protocol::choke>().execute([this](const hpack::headers_t& headers){
            COCAINE_LOG_DEBUG(logger, "processing choke");
            d.apply([&](data_t& d){
                if(!d.forward_stream) {
                    COCAINE_LOG_WARNING(logger, "skipping sending chunk - forward stream is missing"
                        "(this can happen if enqueue was unsuccessfull)");
                    return;
                }
                try {
                    d.choke_sent = true;
                    d.choke_headers = headers;
                    d.forward_stream->send<protocol::choke>(headers);
                } catch (const std::system_error& e) {
                    COCAINE_LOG_WARNING(logger, "failed to send choke to forward dispatch - {}", error::to_string(e));
                    d.backward_stream.send<protocol::error>(e.code(), "failed to send choke to forward dispatch");
                    peer->schedule_reconnect();
                }
            });
        });

        on<protocol::error>().execute([this](const hpack::headers_t& headers, std::error_code ec, std::string msg){
            COCAINE_LOG_INFO(logger, "processing error");
            COCAINE_LOG_ERROR(logger, "UPYACHKA!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
            d.apply([&](data_t& d){
                d.buffering_enabled = false;
                if(!d.forward_stream) {
                    COCAINE_LOG_WARNING(logger, "skipping sending error - forward stream is missing"
                        "(this can happen if enqueue was unsuccessfull)");
                    return;
                }
                try {
                    d.forward_stream->send<protocol::error>(headers, ec, msg);
                } catch (const std::system_error& e) {
                    COCAINE_LOG_WARNING(logger, "failed to send error to forward dispatch - {}", error::to_string(e));
                    d.backward_stream.send<protocol::error>(e.code(), "failed to send error to forward dispatch");
                    peer->schedule_reconnect();
                }
            });
        });
    }

    auto disable_buffering() -> void {
        d.apply([&](data_t& d){
            disable_buffering(d);
        });
    }

    auto disable_buffering(data_t& d) -> void {
        d.buffering_enabled = false;
        d.enqueue_frame.clear();
        d.enqueue_headers.clear();
        d.chunk_headers.clear();
        d.chunks.clear();
        COCAINE_LOG_DEBUG(logger, "disabled buffernig");
    }

    auto send_discard_frame() -> void {
        COCAINE_LOG_DEBUG(logger, "sending discard frame");
        d.apply([&](data_t& d) {
            try {
                auto ec = make_error_code(error::dispatch_errors::not_connected);
                if(!d.forward_stream) {
                    return;
                }
                d.forward_stream->send<protocol::error>(hpack::headers_t{}, ec, "vicodyn client was disconnected");
            } catch (const std::system_error& e) {
                // pass
            }
        });
    }

    auto enqueue(const hpack::headers_t& headers, std::string event) -> void {
        COCAINE_LOG_DEBUG(logger, "processing enqueue");
        d.apply([&](data_t& d){
            try {
                d.enqueue_frame = std::move(event);
                d.enqueue_headers = std::move(headers);
                d.forward_stream = peer->open_stream<io::node::enqueue>(backward_dispatch, d.enqueue_headers,
                                                                        proxy.app_name, d.enqueue_frame);
            } catch (const std::system_error& e) {
                COCAINE_LOG_WARNING(logger, "failed to send enqueue to forward stream - {}", error::to_string(e));
                peer->schedule_reconnect();
                //TODO: maybe cycle here?
                try {
                    retry(d);
                } catch(std::system_error& e) {
                    COCAINE_LOG_WARNING(logger, "could not retry enqueue - {}", error::to_string(e));
                    d.backward_stream.send<protocol::error>(e.code(), "failed to retry enqueue");
                }
            }
        });
    }

    auto retry() -> void;

    auto on_error(std::error_code ec, const std::string& msg) -> void {
        return proxy.balancer->on_error(peer, ec, msg);
    }

    auto is_recoverable(std::error_code ec) -> bool {
        return proxy.balancer->is_recoverable(peer, ec);
    }

private:
    auto retry(data_t& d) -> void;
};

class backward_dispatch_t : public dispatch<app_tag> {
    using protocol = io::protocol<app_tag>::scope;

    proxy_t& proxy;
    upstream<app_tag> backward_stream;
    std::shared_ptr<forward_dispatch_t> forward_dispatch;

public:
    backward_dispatch_t(const std::string& name, proxy_t& _proxy, upstream<app_tag> back_stream):
        dispatch(name),
        proxy(_proxy),
        backward_stream(std::move(back_stream))
    {
        on<protocol::chunk>().execute([this](const hpack::headers_t& headers, std::string chunk) mutable {
            forward_dispatch->disable_buffering();
            try {
                backward_stream = backward_stream.send<protocol::chunk>(headers, std::move(chunk));
            } catch (const std::system_error& e) {
                forward_dispatch->send_discard_frame();
            }
        });

        on<protocol::error>().execute([this](const hpack::headers_t& headers, std::error_code ec, std::string msg) mutable {
            COCAINE_LOG_WARNING(forward_dispatch->logger, "received error from peer {}({}) - {}", ec.message(), ec.value(), msg);
            forward_dispatch->on_error(ec, msg);
            if(forward_dispatch->is_recoverable(ec)) {
                try {
                    forward_dispatch->retry();
                } catch(const std::system_error& e) {
                    backward_stream.send<protocol::error>(hpack::headers_t{}, e.code(), e.what());
                }
            } else {
                try {
                    backward_stream.send<protocol::error>(headers, ec, msg);
                } catch (const std::system_error& e) {
                    forward_dispatch->send_discard_frame();
                }
            }
        });

        on<protocol::choke>().execute([this](const hpack::headers_t& headers) mutable {
            forward_dispatch->disable_buffering();
            try {
                backward_stream.send<protocol::choke>(headers);
            } catch (const std::system_error& e) {
                forward_dispatch->send_discard_frame();
            }
        });

    }

    auto attach(std::shared_ptr<forward_dispatch_t> f_dispatch) -> void {
        forward_dispatch = std::move(f_dispatch);
    }

    auto discard(const std::error_code& ec) -> void override {
        try {
            backward_stream.send<protocol::error>(ec, "vicodyn upstream has been disconnected");
        } catch (const std::exception& e) {
            COCAINE_LOG_WARNING(forward_dispatch->logger, "could not send error {} to upstream - {}", ec, e);
        }
    }
};

auto forward_dispatch_t::retry() -> void {
    d.apply([&](data_t& d) {
        retry(d);
    });
}

auto forward_dispatch_t::retry(data_t& d) -> void {
    COCAINE_LOG_INFO(logger, "retrying");
    d.retry_counter++;
    if(!d.buffering_enabled) {
        throw error_t("buffering is already disabled - response chunk was sent");
    }
    if(d.retry_counter > proxy.balancer->retry_count()) {
        throw error_t("maximum number of retries reached");
    }
    peer = proxy.choose_peer(d.enqueue_headers, d.enqueue_frame);
    logger.reset(new blackhole::wrapper_t(*proxy.logger, {{"peer", peer->uuid()}}));
//    auto dispatch_name = format("{}/{}/streaming/backward", proxy.name(), d.enqueue_frame);
//    auto backward_dispatch = std::make_shared<backward_dispatch_t>(dispatch_name, proxy, d.backward_stream, shared_from_this());
    //TODO: remove duplication
    d.forward_stream = peer->open_stream<io::node::enqueue>(backward_dispatch, d.enqueue_headers, proxy.app_name, d.enqueue_frame);
    for(size_t i = 0; i < d.chunks.size(); i++) {
        d.forward_stream->send<protocol::chunk>(d.chunk_headers[i], d.chunks[i]);
    }
    if(d.choke_sent) {
        d.forward_stream->send<protocol::choke>(d.choke_headers);
    }

}

auto proxy_t::make_balancer(const dynamic_t& args, const dynamic_t::object_t& extra) -> api::vicodyn::balancer_ptr {
    auto balancer_conf = args.as_object().at("balancer", dynamic_t::empty_object);
    auto name = balancer_conf.as_object().at("type", "simple").as_string();
    auto balancer_args = balancer_conf.as_object().at("args", dynamic_t::empty_object).as_object();
    return context.repository().get<api::vicodyn::balancer_t>(name, context, peers, executor.asio(), app_name,
                                                              balancer_args, extra);
}

proxy_t::proxy_t(context_t& context, peers_t& peers, const std::string& name, const dynamic_t& args,
                 const dynamic_t::object_t& extra) :
    dispatch(name),
    context(context),
    peers(peers),
    app_name(name.substr(sizeof("virtual::") - 1)),
    balancer(make_balancer(args, extra)),
    logger(context.log(name))
{
    COCAINE_LOG_DEBUG(logger, "created proxy for app {}", app_name);
    on<event_t>([&](const hpack::headers_t& headers, slot_t::tuple_type&& args, slot_t::upstream_type&& backward_stream){
//        backward_stream = backward_stream.send<app_protocol::chunk>("");
//        backward_stream = backward_stream.send<app_protocol::chunk>("HI");

//        io::aux::encoded_buffers_t buffer;
//        msgpack::packer<io::aux::encoded_buffers_t> packer(buffer);
//        io::type_traits<std::pair<unsigned , std::vector<std::pair<std::string, std::string>>>>::pack(packer, {200, {}});
//        backward_stream = backward_stream.send<app_protocol::chunk>(std::string(buffer.data(), buffer.size()));
//
//
//        io::aux::encoded_buffers_t buffer2;
//        msgpack::packer<io::aux::encoded_buffers_t> packer2(buffer2);
//        io::type_traits<std::string>::pack(packer2, "body");
//
//        backward_stream = backward_stream.send<app_protocol::chunk>(std::string(buffer2.data(), buffer2.size()));
//        backward_stream.send<app_protocol::choke>();
//        auto dispatch = std::make_shared<slot_t::dispatch_type>("");
//        dispatch->on<app_protocol::error>([this](std::error_code, std::string){});
//        dispatch->on<app_protocol::chunk>([this](std::string){});
//        dispatch->on<app_protocol::choke>([this](){});
//        return result_t(dispatch);

        auto event = std::get<0>(args);
        std::shared_ptr<peer_t> peer;
        try {
            peer = choose_peer(headers, event);
            COCAINE_LOG_DEBUG(logger, "chosen peer {}", peer);
            auto dispatch_name = format("{}/{}/streaming/backward", this->name(), event);
            auto backward_dispatch = std::make_shared<backward_dispatch_t>(dispatch_name, *this, backward_stream);

            dispatch_name = format("{}/{}/streaming/forward", this->name(), event);
            auto forward_dispatch = std::make_shared<forward_dispatch_t>(*this, dispatch_name, backward_stream, backward_dispatch, peer);
            //TODO: remove this and move creation of backward_disptch inside f_d
            backward_dispatch->attach(forward_dispatch);
            forward_dispatch->enqueue(headers, event);
            return result_t(forward_dispatch);
        } catch (const std::system_error& e) {
            COCAINE_LOG_WARNING(logger, "could not process enqueue via {} - {}", peer, error::to_string(e));
            backward_stream.send<app_protocol::error>(e.code(), e.what());
            auto dispatch = std::make_shared<slot_t::dispatch_type>(format("{}/{}/empty", app_name, event));
            dispatch->on<app_protocol::error>([this](std::error_code, std::string){});
            dispatch->on<app_protocol::chunk>([this](std::string){});
            dispatch->on<app_protocol::choke>([this](){});
            return result_t(dispatch);
        }
    });
}

auto proxy_t::choose_peer(const hpack::headers_t& headers, const std::string& event) -> std::shared_ptr<peer_t> {
    return balancer->choose_peer(headers, event);
}

auto proxy_t::empty() -> bool {
    return peers.inner().apply([&](peers_t::data_t& data) -> bool {
        auto it = data.apps.find(app_name);
        if(it == data.apps.end() || it->second.empty()) {
            return true;
        }
        return false;
    });
}

auto proxy_t::size() -> size_t {
    return peers.inner().apply([&](peers_t::data_t& data) -> size_t {
        auto it = data.apps.find(app_name);
        if(it == data.apps.end()) {
            return 0u;
        }
        return it->second.size();
    });
}

} // namespace vicodyn
} // namespace cocaine
