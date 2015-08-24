#include "cocaine/detail/service/node/app.hpp"

#include "cocaine/api/isolate.hpp"
#include "cocaine/context.hpp"
#include "cocaine/errors.hpp"
#include "cocaine/idl/node.hpp"
#include "cocaine/locked_ptr.hpp"
#include "cocaine/rpc/actor.hpp"
#include "cocaine/rpc/actor_unix.hpp"
#include "cocaine/traits/dynamic.hpp"

#include "cocaine/detail/service/node/dispatch/client.hpp"
#include "cocaine/detail/service/node/dispatch/handshaker.hpp"
#include "cocaine/detail/service/node/dispatch/hostess.hpp"
#include "cocaine/detail/service/node/manifest.hpp"
#include "cocaine/detail/service/node/overseer.hpp"
#include "cocaine/detail/service/node/profile.hpp"

namespace ph = std::placeholders;

using namespace cocaine;
using namespace cocaine::service;
using namespace cocaine::service::node;

// While the client is connected it's okay, balance on numbers depending on received.
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
            p(p_),
            super("controlling")
        {
            on<protocol::chunk>([&](const std::size_t& size) {
                p->overseer->keep_alive(size);
            });

            p->locked.store(true);
        }

        ~controlling_slot_t() {
            p->locked.store(false);
        }

        virtual
        void
        discard(const std::error_code& ec) const {
            COCAINE_LOG_DEBUG(p->log, "client has been disappeared, assuming direct control");
            p->overseer->keep_alive(0);
        }
    };

    typedef std::shared_ptr<const io::basic_slot<io::app::control>::dispatch_type> result_type;

    const std::unique_ptr<logging::log_t> log;
    std::atomic<bool> locked;
    std::shared_ptr<overseer_t> overseer;

public:
    control_slot_t(std::shared_ptr<overseer_t> overseer_, std::unique_ptr<logging::log_t> log_):
        log(std::move(log_)),
        locked(false),
        overseer(overseer_)
    {
        COCAINE_LOG_DEBUG(log, "control slot has been created");
    }

    ~control_slot_t() {
        COCAINE_LOG_DEBUG(log, "control slot has been destroyed");
    }

    boost::optional<result_type>
    operator()(tuple_type&& args, upstream_type&& upstream) {
        typedef io::protocol<io::event_traits<io::app::control>::upstream_type>::scope protocol;

        const auto dispatch = tuple::invoke(std::move(args), [&]() -> result_type {
            if (locked) {
                upstream.send<protocol::error>(std::make_error_code(std::errc::resource_unavailable_try_again));
                return nullptr;
            }
            return std::make_shared<controlling_slot_t>(this);
        });

        return boost::make_optional(dispatch);
    }
};

/// App dispatch, manages incoming enqueue requests. Adds them to the queue.
class app_dispatch_t:
    public dispatch<io::app_tag>
{
    typedef io::streaming_slot<io::app::enqueue> slot_type;

    const std::unique_ptr<logging::log_t> log;

    // Yes, weak pointer here indicates about application destruction.
    std::weak_ptr<overseer_t> overseer;

public:
    app_dispatch_t(context_t& context, const std::string& name, std::shared_ptr<overseer_t> overseer_) :
        dispatch<io::app_tag>(name),
        log(context.log(format("%s/dispatch", name))),
        overseer(overseer_)
    {
        on<io::app::enqueue>(std::make_shared<slot_type>(
            std::bind(&app_dispatch_t::on_enqueue, this, ph::_1, ph::_2, ph::_3)
        ));

        on<io::app::info>(std::bind(&app_dispatch_t::on_info, this));

        on<io::app::control>(std::make_shared<control_slot_t>(overseer_, context.log(format("%s/control", name))));
    }

    ~app_dispatch_t() {
        COCAINE_LOG_DEBUG(log, "app dispatch has been destroyed");
    }

private:
    std::shared_ptr<const slot_type::dispatch_type>
    on_enqueue(slot_type::upstream_type&& upstream , const std::string& event, const std::string& id) {
        COCAINE_LOG_DEBUG(log, "processing enqueue '%s' event", event);

        typedef io::protocol<io::event_traits<io::app::enqueue>::dispatch_type>::scope protocol;

        try {
            if (auto overseer = this->overseer.lock()) {
                if (id.empty()) {
                    return overseer->enqueue(upstream, event, boost::none);
                } else {
                    return overseer->enqueue(upstream, event, service::node::slave::id_t(id));
                }
            } else {
                // We shouldn't close the connection here, because there possibly can be events
                // processing.
                throw std::system_error(std::make_error_code(std::errc::broken_pipe), "the application has been stopped");
            }
        } catch (const std::system_error& err) {
            COCAINE_LOG_ERROR(log, "unable to enqueue '%s' event: %s", event, err.what());

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

            return overseer->info(flags);
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
    std::unique_ptr<api::cancellation_t> spooler;

public:
    template<class F>
    spooling_t(context_t& context,
               asio::io_service& loop,
               const manifest_t& manifest,
               const profile_t& profile,
               const logging::log_t* log,
               F handler)
    {
        isolate = context.get<api::isolate_t>(
            profile.isolate.type,
            context,
            loop,
            manifest.name,
            profile.isolate.args
        );

        try {
            // NOTE: Regardless of whether the asynchronous operation completes immediately or not,
            // the handler will not be invoked from within this call. Invocation of the handler
            // will be performed in a manner equivalent to using `boost::asio::io_service::post()`.
            spooler = isolate->async_spool(handler);
        } catch (const std::system_error& err) {
            COCAINE_LOG_ERROR(log, "uncaught spool exception: [%d] %s", err.code().value(), err.code().message());
            handler(err.code());
        } catch (const std::exception& err) {
            COCAINE_LOG_ERROR(log, "uncaught spool exception: %s", err.what());
            handler(error::uncaught_spool_error);
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
    const logging::log_t* log;

    context_t& context;

    std::string name;

    std::unique_ptr<unix_actor_t> engine;
    std::shared_ptr<overseer_t> overseer;

public:
    running_t(context_t& context_,
              const manifest_t& manifest,
              const profile_t& profile,
              const logging::log_t* log,
              std::shared_ptr<asio::io_service> loop):
        log(log),
        context(context_),
        name(manifest.name)
    {
        // Create the Overseer - slave spawner/despawner plus the event queue dispatcher.
        overseer.reset(new overseer_t(context, manifest, profile, loop));

        // Create a TCP server and publish it.
        COCAINE_LOG_DEBUG(log, "publishing application service with the context");
        context.insert(manifest.name, std::make_unique<actor_t>(
            context,
            std::make_shared<asio::io_service>(),
            std::make_unique<app_dispatch_t>(context, manifest.name, overseer)
        ));

        // Create an unix actor and bind to {manifest->name}.{pid} unix-socket.
        COCAINE_LOG_DEBUG(log, "publishing worker service with the context");
        engine.reset(new unix_actor_t(
            context,
            manifest.endpoint,
            std::bind(&overseer_t::prototype, overseer),
            [](io::dispatch_ptr_t handshaker, std::shared_ptr<session_t> session) {
                std::static_pointer_cast<const handshaker_t>(handshaker)->bind(session);
            },
            std::make_shared<asio::io_service>(),
            std::make_unique<hostess_t>(manifest.name)
        ));
        engine->run();
    }

    ~running_t() {
        COCAINE_LOG_DEBUG(log, "removing application service from the context");

        try {
            // NOTE: It can throw if someone has removed the service from the context, it's valid.
            //
            // Moreover if the context was unable to bootstrap itself it removes all services from
            // the service list (including child services). It can be that this app has been removed
            // earlier during bootstrap failure.
            context.remove(name);
        } catch (const std::exception& err) {
            COCAINE_LOG_WARNING(log, "unable to remove application service from the context: %s", err.what());
        }

        engine->terminate();
        overseer->cancel();
    }

    virtual
    dynamic_t::object_t
    info(io::node::info::flags_t flags) const {
        dynamic_t::object_t info;

        if (is_overseer_report_required(flags)) {
            info = overseer->info(flags);
        } else {
            info["uptime"] = overseer->uptime().count();
        }

        info["state"] = "running";
        return info;
    }

private:
    static
    bool
    is_overseer_report_required(io::node::info::flags_t flags) {
        return flags & io::node::info::overseer_report;
    }
};

} // namespace state

class cocaine::service::node::app_state_t:
    public std::enable_shared_from_this<app_state_t>
{
    const std::unique_ptr<logging::log_t> log;

    context_t& context;

    typedef std::unique_ptr<state::base_t> state_type;

    synchronized<state_type> state;

    /// Node start request's deferred.
    cocaine::deferred<void> deferred;

    // Configuration.
    const manifest_t manifest_;
    const profile_t  profile;

    std::shared_ptr<asio::io_service> loop;
    std::unique_ptr<asio::io_service::work> work;
    boost::thread thread;

public:
    app_state_t(context_t& context,
                manifest_t manifest_,
                profile_t profile_,
                cocaine::deferred<void> deferred_):
        log(context.log(format("%s/app", manifest_.name))),
        context(context),
        state(new state::stopped_t),
        deferred(std::move(deferred_)),
        manifest_(std::move(manifest_)),
        profile(std::move(profile_)),
        loop(std::make_shared<asio::io_service>()),
        work(std::make_unique<asio::io_service::work>(*loop))
    {
        COCAINE_LOG_DEBUG(log, "application has initialized its internal state");

        thread = boost::thread([=] {
            loop->run();
        });
    }

    ~app_state_t() {
        COCAINE_LOG_DEBUG(log, "application is destroying its internal state");

        work.reset();
        thread.join();

        COCAINE_LOG_DEBUG(log, "application has destroyed its internal state");
    }

    const manifest_t&
    manifest() const noexcept {
        return manifest_;
    }

    dynamic_t
    info(io::node::info::flags_t flags) const {
        return (*state.synchronize())->info(flags);
    }

    void
    spool() {
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
                std::bind(&app_state_t::on_spool, shared_from_this(), ph::_1)
            ));
        });
    }

    void
    cancel(std::error_code ec) {
        state.synchronize()->reset(new state::stopped_t(std::move(ec)));
    }

private:
    void
    on_spool(const std::error_code& ec) {
        if (ec) {
            COCAINE_LOG_ERROR(log, "unable to spool app - [%d] %s", ec.value(), ec.message());

            loop->dispatch(std::bind(&app_state_t::cancel, shared_from_this(), ec));

            // Attempt to finish node service's request.
            try {
                deferred.abort(ec, ec.message());
            } catch (const std::exception&) {
                // Ignore if the client has been disconnected.
            }
        } else {
            // Dispatch the completion handler to be sure it will be called in a I/O thread to
            // avoid possible deadlocks.
            loop->dispatch(std::bind(&app_state_t::publish, shared_from_this()));
        }
    }

    void
    publish() {
        std::error_code ec;

        try {
            state.synchronize()->reset(
                new state::running_t(context, manifest(), profile, log.get(), loop)
            );
        } catch (const std::system_error& err) {
            COCAINE_LOG_ERROR(log, "unable to publish app: [%d] %s", err.code().value(), err.code().message());
            ec = err.code();
        } catch (const std::exception& err) {
            COCAINE_LOG_ERROR(log, "unable to publish app: %s", err.what());
            ec = error::uncaught_publish_error;
        }

        // Attempt to finish node service's request.
        try {
            if (ec) {
                cancel(ec);
                deferred.abort(ec, ec.message());
            } else {
                deferred.close();
            }
        } catch (const std::exception&) {
            // Ignore if the client has been disconnected.
        }
    }
};

app_t::app_t(context_t& context,
             const std::string& manifest,
             const std::string& profile,
             cocaine::deferred<void> deferred):
    state(std::make_shared<app_state_t>(context, manifest_t(context, manifest), profile_t(context, profile), deferred))
{
    state->spool();
}

app_t::~app_t() {
    state->cancel(std::error_code());
}

std::string
app_t::name() const {
    return state->manifest().name;
}

dynamic_t
app_t::info(io::node::info::flags_t flags) const {
    return state->info(flags);
}
