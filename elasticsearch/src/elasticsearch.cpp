#include <functional>
#include <tuple>

#include <boost/foreach.hpp>

#include <cocaine/traits/tuple.hpp>
#include <cocaine/logging.hpp>

#include "cocaine/io/service/elasticsearch.hpp"

using namespace std::placeholders;

using namespace ioremap;

using namespace cocaine::service;

template<typename T>
struct SimpleRedirect {
    void operator()(cocaine::deferred<T> deferred, const std::string &data) {
        deferred.write(data);
    }
};

template<typename T>
struct RequestHandler {
    typedef std::function<void(cocaine::deferred<T>, const std::string&)> Callback;

    cocaine::deferred<T> deferred;
    Callback callback;

    RequestHandler(cocaine::deferred<T> deferred) :
        deferred(deferred),
        callback()
    {}

    RequestHandler(cocaine::deferred<T> deferred, Callback callback) :
        deferred(deferred),
        callback(callback)
    {}

    void operator()(const swarm::network_reply& reply) {
        const std::string data = reply.get_data();
        const int code = reply.get_code();
        const bool success = (reply.get_error() == 0 && (code < 400 || code >= 600));

        if (success) {
        } else {
            if (reply.get_code() == 0) {
                // Socket-only error, no valid http response
                const std::string &message = cocaine::format("Unable to download %s, network error code %d",
                                                             reply.get_request().get_url(),
                                                             reply.get_error());
                deferred.abort(-reply.get_error(), message);
                return;
            }
        }
        callback(deferred, data);
    }
};

elasticsearch_t::elasticsearch_t(cocaine::context_t &context, cocaine::io::reactor_t &reactor, const std::string &name, const Json::Value &args) :
    service_t(context, reactor, name, args),
    m_manager(reactor.native()),
    m_log(new logging::log_t(context, name))
{
    on<io::elasticsearch::get>("get", std::bind(&elasticsearch_t::get, this, _1));
    on<io::elasticsearch::index>("index", std::bind(&elasticsearch_t::index, this, _1, _2));
}

cocaine::deferred<Response::Get> elasticsearch_t::get(const std::string &index)
{
    const std::string &url = cocaine::format("http://localhost:9200/%s/_source", index);

    cocaine::deferred<Response::Get> deferred;
    RequestHandler<Response::Get> handler(deferred);

    swarm::network_request request;
    request.set_url(url);
    m_manager.get(handler, request);
    return deferred;
}

struct IndexHandler {
    std::shared_ptr<cocaine::logging::log_t> log;

    void operator()(cocaine::deferred<Response::Index> deferred, const std::string &data) {
        //!@todo: process elasticsearch errors

        Json::Value root;
        Json::Reader reader;
        bool parsingSuccessful = reader.parse(data, root);
        if (!parsingSuccessful)
            COCAINE_LOG_ERROR(log, "parse failed");

        std::string id = root["_id"].asString();
        COCAINE_LOG_INFO(log, cocaine::format("Received data: %s", data));

        Response::Index tuple = std::make_tuple(id);
        deferred.write(tuple);
    }
};

cocaine::deferred<Response::Index> elasticsearch_t::index(const std::string &index, const std::string &data)
{
    const std::string &url = cocaine::format("http://localhost:9200/%s", index);
    COCAINE_LOG_DEBUG(m_log, cocaine::format("Indexing: '%s' at '%s'", data, url));

    cocaine::deferred<Response::Index> deferred;
    IndexHandler indexHandler {
        m_log
    };
    RequestHandler<Response::Index> handler(deferred, indexHandler);

    swarm::network_request request;
    request.set_url(url);
    m_manager.post(handler, request, data);
    return deferred;
}
