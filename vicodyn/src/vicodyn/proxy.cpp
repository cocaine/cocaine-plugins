#include "cocaine/vicodyn/proxy.hpp"

#include "cocaine/api/vicodyn/balancer.hpp"
#include "cocaine/format/ptr.hpp"
#include "cocaine/format/peer.hpp"
#include "cocaine/repository/vicodyn/balancer.hpp"

#include "cocaine/vicodyn/access_log.hpp"
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

template<class Tag>
class discardable : public dispatch<Tag> {
public:
    using discarder_t = std::function<void(const std::error_code&)>;

    discardable(const std::string& name):
        dispatch<app_tag>(name)
    {}

    auto discard(const std::error_code& ec) -> void override {
        if(discarder != nullptr) {
            discarder(ec);
        }
    }

    auto on_discard(discarder_t d) -> void {
        discarder = std::move(d);
    }

private:
    discarder_t discarder;
};

class safe_stream_t {
    bool closed;
    boost::optional<upstream<app_tag>> stream;

public:
    using protocol = io::protocol<app_tag>::scope;

    safe_stream_t(upstream<app_tag> stream) :
        closed(false),
        stream(std::move(stream))
    {}

    safe_stream_t() :
        closed(false),
        stream()
    {}

    operator bool() {
        return stream;
    }

    auto chunk(const hpack::headers_t& headers, std::string data) -> bool {
        if(!closed && stream) {
            stream = stream->send<protocol::chunk>(headers, std::move(data));
            return true;
        }
        return false;
    }

    auto close(const hpack::headers_t& headers) -> bool {
        if(!closed && stream) {
            stream->send<protocol::choke>(headers);
            return true;
        }
        closed = true;
        return false;
    }

    auto error(const hpack::headers_t& headers, std::error_code ec, std::string msg) -> bool {
        if(!closed && stream) {
            stream->send<protocol::error>(headers, std::move(ec), std::move(msg));
            return true;
        }
        closed = true;
        return false;
    }
};

class vicodyn_dispatch_t : public std::enable_shared_from_this<vicodyn_dispatch_t> {
    using protocol = io::protocol<app_tag>::scope;

    proxy_t& proxy;
    std::shared_ptr<access_log_t> access;
    std::unique_ptr<logging::logger_t> logger;
    std::shared_ptr<peer_t> peer;
    discardable<app_tag> forward_dispatch;
    discardable<app_tag> backward_dispatch;

    safe_stream_t backward_stream;
    safe_stream_t forward_stream;

    std::string enqueue_frame;
    hpack::headers_t enqueue_headers;
    std::vector<std::string> chunks;
    std::vector<hpack::headers_t> chunk_headers;
    bool choke_sent;
    hpack::headers_t choke_headers;

    bool buffering_enabled;

    size_t retry_counter;

    synchronized<void> mutex;

public:
    class check_stream_t {
        vicodyn_dispatch_t* parent;
    public:
        explicit
        check_stream_t(vicodyn_dispatch_t* parent) :
            parent(parent)
        {}

        template<typename Event, typename F, typename... Args>
        auto
        operator()(F fn, Event, const hpack::headers_t& headers, Args&&... args) -> void {
            if(!parent->forward_stream) {
                COCAINE_LOG_WARNING(parent->logger, "skipping sending {} - forward stream is missing"
                    "(this can happen if enqueue was unsuccessfull)", Event::alias());
                return;
            }
            return fn(headers, std::forward<Args>(args)...);
        }
    };

    class locked_t {
        vicodyn_dispatch_t* parent;
    public:
        explicit
        locked_t(vicodyn_dispatch_t* parent) :
            parent(parent)
        {}

        template<typename Event, typename F, typename... Args>
        auto
        operator()(F fn, Event, const hpack::headers_t& headers, Args&&... args) -> void {
            auto guard = parent->mutex.synchronize();
            return fn(headers, std::forward<Args>(args)...);
        }
    };

    class catcher_t {
        vicodyn_dispatch_t* parent;
    public:
        explicit
        catcher_t(vicodyn_dispatch_t* parent) :
            parent(parent)
        {}

        template<typename Event, typename F, typename... Args>
        auto
        operator()(F fn, Event, const hpack::headers_t& headers, Args&&... args) -> void {
            try {
                return fn(headers, std::forward<Args>(args)...);
            }
            catch(const std::system_error& e) {
                COCAINE_LOG_WARNING(parent->logger, "failed to send error to forward dispatch - {}", error::to_string(e));
                parent->backward_stream.error({}, e.code(), "failed to send error to forward dispatch");
                parent->peer->schedule_reconnect();
            }
        }
    };

    vicodyn_dispatch_t(proxy_t& _proxy, std::shared_ptr<access_log_t> access, const std::string& name,
                       upstream<app_tag> b_stream, std::shared_ptr<peer_t> _peer) :
        proxy(_proxy),
        access(std::move(access)),
        logger(new blackhole::wrapper_t(*proxy.logger, {{"peer", _peer->uuid()}})),
        peer(std::move(_peer)),
        forward_dispatch(name + "/forward"),
        backward_dispatch(name + "/backward"),
        backward_stream(std::move(b_stream)),
        forward_stream(),
        buffering_enabled(true),
        retry_counter(0)
    {
        namespace ph = std::placeholders;

        check_stream_t check_stream(this);
        locked_t locked(this);
        catcher_t catcher(this);
        forward_dispatch.on<protocol::chunk>()
            .with_middleware(locked)
            .with_middleware(check_stream)
            .with_middleware(catcher)
            .execute(std::bind(&vicodyn_dispatch_t::on_forward_chunk, this, ph::_1, ph::_2));

        forward_dispatch.on<protocol::choke>()
            .with_middleware(locked)
            .with_middleware(check_stream)
            .with_middleware(catcher)
            .execute(std::bind(&vicodyn_dispatch_t::on_forward_choke, this, ph::_1));

        forward_dispatch.on<protocol::error>()
            .with_middleware(locked)
            .with_middleware(check_stream)
            .with_middleware(catcher)
            .execute(std::bind(&vicodyn_dispatch_t::on_forward_error, this, ph::_1, ph::_2, ph::_3));

        forward_dispatch.on_discard([&](const std::error_code&){
            on_client_disconnection();
        });


        backward_dispatch.on<protocol::chunk>()
            .execute(std::bind(&vicodyn_dispatch_t::on_backward_chunk, this, ph::_1, ph::_2));

        backward_dispatch.on<protocol::choke>()
            .execute(std::bind(&vicodyn_dispatch_t::on_backward_choke, this, ph::_1));

        backward_dispatch.on<protocol::error>()
            .execute(std::bind(&vicodyn_dispatch_t::on_backward_error, this, ph::_1, ph::_2, ph::_3));

        backward_dispatch.on_discard([&](const std::error_code& ec) {
            try {
                backward_stream.error({}, ec, "vicodyn upstream has been disconnected");
            } catch (const std::exception& e) {
                COCAINE_LOG_WARNING(logger, "could not send error {} to upstream - {}", ec, e);
            }
        });
    }

    ~vicodyn_dispatch_t() {
        access->add({"retries", retry_counter});
    }

    auto on_forward_chunk(const hpack::headers_t& headers, std::string chunk) -> void {
        COCAINE_LOG_DEBUG(logger, "processing chunk");
        chunks.push_back(std::move(chunk));
        chunk_headers.push_back(headers);
        forward_stream.chunk(headers, chunks.back());
        access->add_checkpoint("after_fchunk");
    }

    auto on_forward_choke(const hpack::headers_t& headers) -> void {
        COCAINE_LOG_DEBUG(logger, "processing choke");
        choke_sent = true;
        choke_headers = headers;
        forward_stream.close(headers);
        access->add_checkpoint("after_fchoke");
    }

    auto on_forward_error(const hpack::headers_t& headers, const std::error_code& ec, const std::string& msg) -> void {
        COCAINE_LOG_INFO(logger, "processing error");
        forward_stream.error(headers, ec, msg);
        access->add_checkpoint("after_ferror");
    }

    auto on_backward_chunk(const hpack::headers_t& headers, std::string chunk) -> void {
        disable_buffering();
        try {
            backward_stream.chunk(headers, std::move(chunk));
            access->add_checkpoint("after_bchunk");
        } catch (const std::system_error& e) {
            on_client_disconnection();
        }
    }

    auto on_backward_error(const hpack::headers_t& headers, const std::error_code& ec, const std::string& msg) -> void {
        COCAINE_LOG_WARNING(logger, "received error from peer {}({}) - {}", ec.message(), ec.value(), msg);
        proxy.balancer->on_error(peer, ec, msg);
        if(proxy.balancer->is_recoverable(peer, ec)) {
            try {
                access->add_checkpoint("recoverable_error");
                retry();
            } catch(const std::system_error& e) {
                backward_stream.error({}, e.code(), e.what());
                access->add_checkpoint("after_recoverable_error_failed_retry");
            }
        } else {
            try {
                backward_stream.error(headers, ec, msg);
                access->add_checkpoint("after_berror");
            } catch (const std::system_error& e) {
                on_client_disconnection();
            }
            access->fail(ec, msg);
        }
    };


    auto on_backward_choke(const hpack::headers_t& headers) -> void {
        try {
            if(backward_stream.close(headers)) {
                access->add_checkpoint("after_bchoke");
            }
        } catch (const std::system_error& e) {
            on_client_disconnection();
        }
        access->finish();
    }

    auto retry() -> void {
        mutex.apply([&](){
            retry_unsafe();
        });
    }

    auto disable_buffering() -> void {
        mutex.apply([&](){
            disable_buffering_unsafe();
        });
    }

    auto on_client_disconnection() -> void {
        // TODO: Do we need lock here?
        COCAINE_LOG_DEBUG(logger, "sending discard frame");
        auto ec = make_error_code(error::dispatch_errors::not_connected);
        forward_stream.error({}, ec, "vicodyn client was disconnected");
    }

    auto enqueue(const hpack::headers_t& headers, std::string event) -> void {
        mutex.apply([&](){
            enqueue_unsafe(headers, std::move(event));
        });
    }

    auto on_error(std::error_code ec, const std::string& msg) -> void {
        return proxy.balancer->on_error(peer, ec, msg);
    }

    auto is_recoverable(std::error_code ec) -> bool {
        return proxy.balancer->is_recoverable(peer, ec);
    }

    auto shared_backward_dispatch() -> std::shared_ptr<dispatch<app_tag>> {
        return std::shared_ptr<dispatch<app_tag>>(shared_from_this(), &backward_dispatch);
    }

    auto shared_forward_dispatch() -> std::shared_ptr<dispatch<app_tag>> {
        return std::shared_ptr<dispatch<app_tag>>(shared_from_this(), &forward_dispatch);
    }

private:
    auto disable_buffering_unsafe() -> void {
        buffering_enabled = false;
        enqueue_frame.clear();
        enqueue_headers.clear();
        chunk_headers.clear();
        chunks.clear();
        COCAINE_LOG_DEBUG(logger, "disabled buffernig");
    }

    auto enqueue_unsafe(const hpack::headers_t& headers, std::string event) -> void {
        COCAINE_LOG_DEBUG(logger, "processing enqueue");
        enqueue_frame = std::move(event);
        enqueue_headers = std::move(headers);
        try {
            auto u = peer->open_stream<io::node::enqueue>(shared_backward_dispatch(), enqueue_headers, proxy.app_name, enqueue_frame);
            forward_stream = safe_stream_t(std::move(u));
            access->add_checkpoint("after_enqueue");
        } catch (const std::system_error& e) {
            COCAINE_LOG_WARNING(logger, "failed to send enqueue to forward stream - {}", error::to_string(e));
            peer->schedule_reconnect();
            //TODO: maybe cycle here?
            try {
                retry_unsafe();
            } catch(std::system_error& e) {
                COCAINE_LOG_WARNING(logger, "could not retry enqueue - {}", error::to_string(e));
                backward_stream.error({}, e.code(), "failed to retry enqueue");
            }
        }
    }

    auto retry_unsafe() -> void {
        COCAINE_LOG_INFO(logger, "retrying");
        retry_counter++;
        if(!buffering_enabled) {
            throw error_t("buffering is already disabled - response chunk was sent");
        }
        if(retry_counter > proxy.balancer->retry_count()) {
            throw error_t("maximum number of retries reached");
        }
        peer = proxy.choose_peer(enqueue_headers, enqueue_frame);
        access->add_checkpoint("retry");
        access->add({"peer", peer->uuid()});
        logger.reset(new blackhole::wrapper_t(*proxy.logger, {{"peer", peer->uuid()}}));
        auto u = peer->open_stream<io::node::enqueue>(shared_backward_dispatch(), enqueue_headers, proxy.app_name, enqueue_frame);
        forward_stream = safe_stream_t(std::move(u));
        for(size_t i = 0; i < chunks.size(); i++) {
            forward_stream.chunk(chunk_headers[i], chunks[i]);
        }
        if(choke_sent) {
            forward_stream.close(choke_headers);
        }
        access->add_checkpoint("after_retry");
    }
};

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
        auto access = std::make_shared<access_log_t>(*logger);
        auto event = std::get<0>(args);
        std::shared_ptr<peer_t> peer;
        peer = choose_peer(headers, event);
        access->add({"peer", peer->uuid()});
        COCAINE_LOG_DEBUG(logger, "chosen peer {}", peer);
        auto dispatch_name = format("{}/{}/streaming", this->name(), event);

        dispatch_name = format("{}/{}/streaming/forward", this->name(), event);
        auto dispatch = std::make_shared<vicodyn_dispatch_t>(*this, access, dispatch_name, backward_stream, peer);
        //TODO: remove this and move creation of backward_disptch inside f_d
        dispatch->enqueue(headers, event);
        return result_t(dispatch->shared_forward_dispatch());
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
