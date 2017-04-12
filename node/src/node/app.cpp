#include "cocaine/service/node/app.hpp"

#include <blackhole/logger.hpp>

#include <cocaine/context.hpp>
#include <cocaine/context/quote.hpp>
#include <cocaine/errors.hpp>
#include <cocaine/locked_ptr.hpp>
#include <cocaine/logging.hpp>
#include <cocaine/repository.hpp>
#include <cocaine/rpc/actor.hpp>
#include <cocaine/rpc/actor_unix.hpp>
#include <cocaine/traits/dynamic.hpp>
#include <cocaine/utility/future.hpp>

#include "cocaine/api/isolate.hpp"
#include "cocaine/idl/node.hpp"
#include "cocaine/repository/isolate.hpp"
#include "cocaine/service/node/manifest.hpp"
#include "cocaine/service/node/overseer.hpp"
#include "cocaine/service/node/profile.hpp"

#include "cocaine/detail/service/node/dispatch/client.hpp"
#include "cocaine/detail/service/node/dispatch/handshake.hpp"
#include "cocaine/detail/service/node/dispatch/init.hpp"
#include "cocaine/detail/service/node/slave/control.hpp"

#include "cocaine/detail/service/node/slave/load.hpp"
#include "cocaine/service/node/slave/id.hpp"

#include "pool_observer.hpp"

namespace ph = std::placeholders;

using namespace cocaine;
using namespace cocaine::service;
using namespace cocaine::service::node;

struct overseer_proxy_t {
    std::shared_ptr<overseer_t> o;

    explicit
    overseer_proxy_t(std::shared_ptr<overseer_t> o):
        o(std::move(o))
    {}
};

// While the client is connected it's okay, balance on numbers depending on received.
// TODO: Drop when all scripts will be rewritten.

namespace {

class control_slot_t:
    public io::basic_slot<io::app::control>
{
    struct controlling_slot_t:
        public io::basic_slot<io::app::control>::dispatch_type
    {
        typedef io::basic_slot<io::app::control>::dispatch_type super;

        typedef io::event_traits<io::app::control>::dispatch_type dispatch_type;
        typedef io::protocol<dispatch_type>::scope protocol;

        control_slot_t* p;

        controlling_slot_t(control_slot_t* p_):
            super("controlling"),
            p(p_)
        {
            on<protocol::chunk>([&](int size) {
                if (auto overseer = p->overseer.lock()) {
                    overseer->o->control_population(size);
                }
            });
        }

        virtual
        void
        discard(const std::error_code&) {
            COCAINE_LOG_DEBUG(p->log, "client has been disappeared, assuming direct control");
            if (auto overseer = p->overseer.lock()) {
                overseer->o->control_population(0);
            }
        }
    };
    typedef std::vector<hpack::header_t> meta_type;

    typedef std::shared_ptr<io::basic_slot<io::app::control>::dispatch_type> result_type;

    const std::unique_ptr<logging::logger_t> log;
    std::weak_ptr<overseer_proxy_t> overseer;

public:
    control_slot_t(std::shared_ptr<overseer_proxy_t> overseer_, std::unique_ptr<logging::logger_t> log_):
        log(std::move(log_)),
        overseer(overseer_)
    {
        COCAINE_LOG_DEBUG(log, "control slot has been created");
    }

    ~control_slot_t() {
        COCAINE_LOG_DEBUG(log, "control slot has been destroyed");
    }

    boost::optional<result_type>
    operator()(tuple_type&& args, upstream_type&& upstream) {
        return operator()({}, std::move(args), std::move(upstream));
    }

    boost::optional<result_type>
    operator()(const meta_type&, tuple_type&& args, upstream_type&&) {
        const auto dispatch = tuple::invoke(std::move(args), [&]() -> result_type {
            return std::make_shared<controlling_slot_t>(this);
        });

        return boost::make_optional(dispatch);
    }
};

}  // namespace

template<class F>
class enqueue_slot : public io::basic_slot<io::app::enqueue> {
public:
    typedef io::app::enqueue event_type;

    typedef std::vector<hpack::header_t> meta_type;
    typedef typename io::basic_slot<event_type>::tuple_type    tuple_type;
    typedef typename io::basic_slot<event_type>::dispatch_type dispatch_type;
    typedef typename io::basic_slot<event_type>::upstream_type upstream_type;

private:
    F f;

public:
    enqueue_slot(F f):
        f(std::move(f))
    {}

    virtual
    boost::optional<std::shared_ptr<dispatch_type>>
    operator()(const meta_type& headers,
               tuple_type&& args,
               upstream_type&& upstream)
    {
        return f(std::move(upstream), headers, std::move(args));
    }
};

template<typename F>
auto make_enqueue_slot(F&& f) -> std::shared_ptr<io::basic_slot<io::app::enqueue>> {
    return std::make_shared<enqueue_slot<F>>(std::forward<F>(f));
}

/// App dispatch, manages incoming enqueue requests. Adds them to the queue.
///
/// \note lives until at least one connection left after actor has been destroyed.
class app_dispatch_t:
    public dispatch<io::app_tag>
{
    const std::unique_ptr<logging::logger_t> log;

    // Yes, weak pointer here indicates about application destruction.
    std::weak_ptr<overseer_proxy_t> overseer;

public:
    app_dispatch_t(context_t& context, const std::string& name, std::shared_ptr<overseer_proxy_t> overseer_) :
        dispatch<io::app_tag>(name),
        log(context.log(format("{}/dispatch", name))),
        overseer(overseer_)
    {
        const auto enqueue = std::bind(&app_dispatch_t::on_enqueue, this, ph::_1, ph::_2, ph::_3);
        on<io::app::enqueue>(make_enqueue_slot(std::move(enqueue)));

        on<io::app::info>(std::bind(&app_dispatch_t::on_info, this));
        on<io::app::control>(std::make_shared<control_slot_t>(overseer_, context.log(format("{}/control", name))));
    }

    ~app_dispatch_t() {
        COCAINE_LOG_DEBUG(log, "app dispatch has been destroyed");
    }

private:
    std::shared_ptr<dispatch<io::stream_of<std::string>::tag>>
    on_enqueue(upstream<io::stream_of<std::string>::tag>&& upstream,
               const std::vector<hpack::header_t>& headers,
               std::tuple<std::string, std::string> args)
    {
        std::string event;
        std::tie(event, std::ignore) = args;

        COCAINE_LOG_DEBUG(log, "processing enqueue '{}' event", event);

        typedef io::protocol<io::event_traits<io::app::enqueue>::dispatch_type>::scope protocol;

        try {
            if (auto overseer = this->overseer.lock()) {
                return overseer->o->enqueue({event, hpack::header_storage_t(headers)}, upstream);
            } else {
                // We shouldn't close the connection here, because there possibly can be events
                // processing.
                throw std::system_error(std::make_error_code(std::errc::broken_pipe), "the application has been stopped");
            }
        } catch (const std::system_error& err) {
            COCAINE_LOG_ERROR(log, "unable to enqueue '{}' event: {}", event, err.what());

            upstream.send<protocol::error>(err.code(), err.what());
        }

        return std::make_shared<client_rpc_dispatch_t>(name());
    }

    dynamic_t
    on_info() const {
        if (auto overseer = this->overseer.lock()) {
            // TODO: Forward flags.
            io::node::info::flags_t flags;
            flags = static_cast<io::node::info::flags_t>(
                  io::node::info::overseer_report);

            return overseer->o->info(flags);
        }

        throw std::system_error(std::make_error_code(std::errc::broken_pipe), "the application has been stopped");
    }
};

/// \note originally there was `boost::variant`, but in 1.46 it has no idea about move-only types.
namespace state {

class base_t {
public:
    virtual
    ~base_t() {}

    virtual
    bool
    stopped() noexcept {
        return false;
    }

    virtual
    dynamic_t::object_t
    info(io::node::info::flags_t flags) const = 0;

    virtual
    std::shared_ptr<overseer_t>
    overseer() const {
        throw cocaine::error_t("invalid state");
    }
};

/// The application is stopped either normally or abnormally.
class stopped_t:
    public base_t
{
    std::error_code ec;

public:
    stopped_t() noexcept {}

    explicit
    stopped_t(std::error_code ec) noexcept:
        ec(std::move(ec))
    {}

    virtual
    bool
    stopped() noexcept {
        return true;
    }

    virtual
    dynamic_t::object_t
    info(io::node::info::flags_t) const {
        dynamic_t::object_t info;
        info["state"] = ec ? std::string("broken") : "stopped";
        info["error"] = ec.value();
        info["cause"] = ec ? ec.message() : "manually stopped";

        return info;
    }
};

/// The application is currently spooling.
class spooling_t:
    public base_t
{
    std::shared_ptr<api::isolate_t> isolate;
    std::shared_ptr<api::cancellation_t> spooler;

public:
    spooling_t(context_t& context,
               asio::io_service& loop,
               const manifest_t& manifest,
               const profile_t& profile,
               logging::logger_t* const log,
               std::shared_ptr<api::spool_handle_base_t> handle)
    {
        isolate = context.repository().get<api::isolate_t>(
            profile.isolate.type,
            context,
            loop,
            manifest.name,
            profile.isolate.type,
            profile.isolate.args
        );

        try {
            // NOTE: Regardless of whether the asynchronous operation completes immediately or not,
            // the handler will not be invoked from within this call. Invocation of the handler
            // will be performed in a manner equivalent to using `boost::asio::io_service::post()`.
            spooler = isolate->spool(handle);
        } catch (const std::system_error& err) {
            COCAINE_LOG_ERROR(log, "uncaught spool exception: {}", error::to_string(err));
            handle->on_abort(err.code(), err.what());
        } catch (const std::exception& err) {
            COCAINE_LOG_ERROR(log, "uncaught spool exception: {}", err.what());
            handle->on_abort(error::uncaught_spool_error, err.what());
        }
    }

    ~spooling_t() {
        if (spooler) {
            spooler->cancel();
        }
    }

    virtual
    dynamic_t::object_t
    info(io::node::info::flags_t) const {
        dynamic_t::object_t info;
        info["state"] = "spooling";
        return info;
    }
};

/// The application has been published and currently running.
class running_t:
    public base_t
{
    logging::logger_t* const log;

    context_t& context;

    std::string name;

    std::unique_ptr<unix_actor_t> engine;
    std::shared_ptr<overseer_proxy_t> overseer_;

public:
    running_t(context_t& context_,
              const manifest_t& manifest,
              const profile_t& profile,
              logging::logger_t* const log,
              std::shared_ptr<asio::io_service> loop):
        log(log),
        context(context_),
        name(manifest.name)
    {
        // Create an Overseer - slave spawner/despawner plus the event queue dispatcher.
        overseer_ = std::make_shared<overseer_proxy_t>(
            std::make_shared<overseer_t>(context, manifest, profile, std::make_shared<observer_adapter_t>(*this), loop)
        );

        // Create an unix actor and bind to {manifest->name}.{pid} unix-socket.
        using namespace detail::service::node;

        COCAINE_LOG_DEBUG(log, "publishing worker service with the context");
        engine.reset(new unix_actor_t(
            context,
            manifest.endpoint,
            std::bind(&overseer_t::prototype, overseer()),
            [](io::dispatch_ptr_t handshake, std::shared_ptr<session_t> session) {
                std::static_pointer_cast<const handshaking_t>(handshake)->bind(session);
            },
            std::make_shared<asio::io_service>(),
            std::make_unique<init_dispatch_t>(manifest.name)
        ));
        engine->run();

        try {
            maybe_publish();
        } catch (const std::exception&) {
            unpublish();
            throw;
        }
    }

    ~running_t() {
        unpublish();
        engine->terminate();
    }

    virtual auto spawned() -> void {
        try {
            maybe_publish();
        } catch (const std::exception&) {
            // Probably if we are here than an application is already published earlier. There is
            // no need to notify anyone about it over and over again.
        }
    }

    virtual auto despawned() -> void {
        maybe_unpublish();
    }

    virtual
    dynamic_t::object_t
    info(io::node::info::flags_t flags) const {
        dynamic_t::object_t info;

        if (is_overseer_report_required(flags)) {
            info = overseer()->info(flags);
        } else {
            info["uptime"] = overseer()->uptime().count();
        }

        info["state"] = "running";
        info["publushed"] = static_cast<bool>(context.locate(name));
        return info;
    }

    std::shared_ptr<overseer_t>
    overseer() const {
        return overseer_->o;
    }

private:
    static
    bool
    is_overseer_report_required(io::node::info::flags_t flags) {
        return flags & io::node::info::overseer_report;
    }

    auto maybe_publish() -> void {
        if (overseer()->active_workers() >= overseer()->profile().publish_on()) {
            publish();
        }
    }

    auto publish() -> void {
        // Create a TCP server and publish it if it's not already there.
        context.insert_with(name, [&] {
            COCAINE_LOG_DEBUG(log, "publishing application service with the context");

            return std::make_unique<actor_t>(
                context,
                std::make_shared<asio::io_service>(),
                std::make_unique<app_dispatch_t>(context, name, overseer_)
            );
        });
    }

    auto maybe_unpublish() -> void {
        if (overseer()->active_workers() < overseer()->profile().unpublish_under()) {
            unpublish();
        }
    }

    auto unpublish() noexcept -> void {
        try {
            // It can throw if someone has removed the service from the context, it's valid.
            // Moreover if the context was unable to bootstrap itself it removes all services from
            // the service list (including child services). It can be that this app has been
            // removed earlier during bootstrap failure.
            context.remove(name);
        } catch (const std::exception& err) {
            COCAINE_LOG_WARNING(log, "failed to remove application service from the context: {}", err.what());
        }
    }

    struct observer_adapter_t : public pool_observer {

        observer_adapter_t(running_t& rstate) :
            parent(rstate)
        {}

        auto despawned(const std::string&) -> void override {
            parent.despawned();
        }

        auto spawned(const std::string&) -> void override {
            parent.spawned();
        }

    private:
        running_t& parent;
    };
};

} // namespace state

class cocaine::service::node::app_state_t
    : public std::enable_shared_from_this<cocaine::service::node::app_state_t>
{
    const std::unique_ptr<logging::logger_t> log;

    context_t& context;

    typedef std::unique_ptr<state::base_t> state_type;

    synchronized<state_type> state;

    /// Node start request's callback.
    std::function<void(std::future<void> future)> callback;

    // Configuration.
    const manifest_t manifest_;
    const profile_t  profile;

    std::shared_ptr<asio::io_service> loop;

    // Bind isolation to application lifetime to ensure,
    // that isolation object is not being recreated all the times.
    api::category_traits<api::isolate_t>::ptr_type isolate;

public:
    app_state_t(context_t& context,
                manifest_t manifest_,
                profile_t profile_,
                std::function<void(std::future<void>)> callback,
                std::shared_ptr<asio::io_service> loop_):
        log(context.log(format("{}/app", manifest_.name))),
        context(context),
        state(new state::stopped_t),
        callback(std::move(callback)),
        manifest_(std::move(manifest_)),
        profile(std::move(profile_)),
        loop(std::move(loop_)),
        isolate()
    {
        isolate = context.repository().get<api::isolate_t>(
            profile.isolate.type,
            context,
            *loop,
            manifest().name,
            profile.isolate.type,
            profile.isolate.args
        );
    }

    auto logger() noexcept -> logging::logger_t& {
        return *log;
    }

    const manifest_t&
    manifest() const noexcept {
        return manifest_;
    }

    dynamic_t
    info(io::node::info::flags_t flags) const {
        return (*state.synchronize())->info(flags);
    }

    std::shared_ptr<overseer_t>
    overseer() const {
        return (*state.synchronize())->overseer();
    }

    auto spool() -> void {
        state.apply([&](state_type& state) {
            if (!state->stopped()) {
                throw std::logic_error("invalid state");
            }

            COCAINE_LOG_DEBUG(log, "app is spooling");
            state.reset(new state::spooling_t(
                context,
                *loop,
                manifest(),
                profile,
                log.get(),
                std::make_shared<spool_handle_t>(shared_from_this())
            ));
        });
    }

    void
    cancel(std::error_code ec) {
        state.synchronize()->reset(new state::stopped_t(std::move(ec)));
    }

private:
    struct spool_handle_t :
        public api::spool_handle_base_t,
        public std::enable_shared_from_this<spool_handle_t>
    {
        virtual
        void
        on_abort(const std::error_code& ec, const std::string& reason) {
            const auto p = parent.lock();
            if (!p) {
                return;
            }
            COCAINE_LOG_ERROR(p->log, "unable to spool app, [{}] {} - {}", ec.value(), ec.message(), reason);
            // Dispatch the completion handler to be sure it will be called in a I/O thread to
            // avoid possible deadlocks.
            p->loop->dispatch(std::bind(&app_state_t::cancel, p, ec));

            p->callback(make_exceptional_future<void>(std::system_error(ec, reason)));
        }

        virtual
        void
        on_ready() {
            const auto p = parent.lock();
            if (!p) {
                return;
            }

            COCAINE_LOG_DEBUG(p->log, "application has been spooled");
            p->loop->dispatch(std::bind(&app_state_t::publish, p));
        }

        spool_handle_t(const std::shared_ptr<app_state_t>& _parent) :
            parent(_parent)
        {}

        // It seems that spool_handle_t could survive somewhere inside isolation
        // spool queue for undefined time, which could exceed parent's lifetime,
        // so to allow parent to pass away in time, it is weak referenced here.
        std::weak_ptr<app_state_t> parent;
    };

    void
    publish() {
        std::error_code ec;

        try {
            state.synchronize()->reset(
                new state::running_t(context, manifest(), profile, log.get(), loop)
            );
            callback(make_ready_future());
        } catch (const std::system_error& err) {
            COCAINE_LOG_ERROR(log, "unable to publish app: {}", error::to_string(err));
            ec = err.code();
            callback(make_exceptional_future<void>(err));
        } catch (const std::exception& err) {
            COCAINE_LOG_ERROR(log, "unable to publish app: {}", err.what());
            ec = error::uncaught_publish_error;
            callback(make_exceptional_future<void>(error_t(error::uncaught_publish_error, err.what())));
        }

        if (ec) {
            cancel(ec);
        }
    }
};

app_t::app_t(context_t& context,
             const std::string& name,
             const std::string& profile,
             std::function<void(std::future<void> future)> callback):
    loop(std::make_shared<asio::io_service>()),
    work(std::make_unique<asio::io_service::work>(*loop)),
    thread(nullptr)
{
    state = std::make_shared<app_state_t>(
        context,
        manifest_t(context, name),
        profile_t(context, profile),
        std::move(callback),
        loop
    );
    COCAINE_LOG_DEBUG(state->logger(), "application has initialized its internal state");

    state->spool();

    thread = std::make_unique<boost::thread>([&] {
        loop->run();
    });
}

app_t::~app_t() {
    COCAINE_LOG_DEBUG(state->logger(), "application is destroying its internal state");

    state->cancel(std::error_code());

    work.reset();
    thread->join();

    COCAINE_LOG_DEBUG(state->logger(), "application has destroyed its internal state");
}

std::string
app_t::name() const {
    return state->manifest().name;
}

dynamic_t
app_t::info(io::node::info::flags_t flags) const {
    return state->info(flags);
}

std::shared_ptr<overseer_t>
app_t::overseer() const {
    return state->overseer();
}
