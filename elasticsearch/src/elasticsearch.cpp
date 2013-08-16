#include <functional>
#include <tuple>
#include <stdexcept>

#include <boost/foreach.hpp>

#include <cocaine/traits/tuple.hpp>
#include <cocaine/logging.hpp>

#include "cocaine/service/elasticsearch/config.hpp"
#include "cocaine/service/elasticsearch.hpp"
#include "rest/handlers.hpp"
#include "rest/index.hpp"
#include "rest/get.hpp"
#include "rest/search.hpp"
#include "rest/delete.hpp"

using namespace std::placeholders;

using namespace ioremap;

using namespace cocaine::service;
using namespace cocaine::service::rest;

class elasticsearch_t::impl_t {
public:
    std::string m_url_prefix;
    swarm::network_manager m_manager;
    std::shared_ptr<logging::log_t> m_log;

    impl_t(cocaine::context_t &context, cocaine::io::reactor_t &reactor, const std::string &name) :
        m_manager(reactor.native()),
        m_log(new logging::log_t(context, name))
    {
    }

    template<typename T, typename H>
    cocaine::deferred<T>
    do_rest_get(const std::string &url, H handler) const
    {
        Get<T> action { m_manager };
        return do_rest<T, H, Get<T>>(url, handler, action);
    }

    template<typename T, typename H>
    cocaine::deferred<T>
    do_rest_post(const std::string &url, const std::string &body, H handler) const
    {
        Post<T> action { m_manager, body };
        return do_rest<T, H, Post<T>>(url, handler, action);
    }

    template<typename T, typename H>
    cocaine::deferred<T>
    do_rest_delete(const std::string &url, H handler) const
    {
#ifdef ELASTICSEARCH_DELETE_SUPPORT
        Delete<T> action { m_manager };
        return do_rest<T, H, Delete<T>>(url, handler, action);
#else
        cocaine::deferred<response::delete_index> deferred;
        deferred.abort(-1, "Delete operation is not supported");
        return deferred;
#endif
    }

    template<typename T, typename H, typename Action>
    cocaine::deferred<T>
    do_rest(const std::string &url, H handler, Action action) const
    {
        cocaine::deferred<T> deferred;
        request_handler_t<T> request_handler(deferred, handler);

        swarm::network_request request;
        request.set_url(url);
        action(request, request_handler);
        return deferred;
    }
};

elasticsearch_t::elasticsearch_t(cocaine::context_t &context, cocaine::io::reactor_t &reactor, const std::string &name, const Json::Value &args) :
    service_t(context, reactor, name, args),
    d(new impl_t(context, reactor, name))
{
    const std::string &host = args.get("host", "127.0.0.1").asString();
    const uint16_t port = args.get("port", 9200).asUInt();
    d->m_url_prefix = cocaine::format("http://%s:%d", host, port);

    COCAINE_LOG_INFO(d->m_log, "Elasticsearch endpoint: %s", d->m_url_prefix);

    on<io::elasticsearch::get>("get", std::bind(&elasticsearch_t::get, this, _1, _2, _3));
    on<io::elasticsearch::index>("index", std::bind(&elasticsearch_t::index, this, _1, _2, _3, _4));
    on<io::elasticsearch::search>("search", std::bind(&elasticsearch_t::search, this, _1, _2, _3, _4));
    on<io::elasticsearch::delete_index>("delete", std::bind(&elasticsearch_t::delete_index, this, _1, _2, _3));
}

elasticsearch_t::~elasticsearch_t()
{
}

cocaine::deferred<response::get> elasticsearch_t::get(const std::string &index, const std::string &type, const std::string &id) const
{
    const std::string &url = cocaine::format("%s/%s/%s/%s/", d->m_url_prefix, index, type, id);
    get_handler_t handler { d->m_log };
    return d->do_rest_get<response::get>(url, handler);
}

cocaine::deferred<response::index> elasticsearch_t::index(const std::string &data, const std::string &index, const std::string &type, const std::string &id) const
{
    const std::string &url = cocaine::format("%s/%s/%s/%s", d->m_url_prefix, index, type, id);
    index_handler_t handler { d->m_log };
    return d->do_rest_post<response::index>(url, data, handler);
}

cocaine::deferred<response::search> elasticsearch_t::search(const std::string &index, const std::string &type, const std::string &query, int size) const
{
    if (size <= 0)
        throw cocaine::error_t("desired search size (%d) must be positive number", size);

    const std::string &url = cocaine::format("%s/%s/%s/_search?q=%s&size=%d", d->m_url_prefix, index, type, query, size);
    search_handler_t handler { d->m_log };
    return d->do_rest_get<response::search>(url, handler);
}

cocaine::deferred<response::delete_index> elasticsearch_t::delete_index(const std::string &index, const std::string &type, const std::string &id) const
{
    const std::string &url = cocaine::format("%s/%s/%s/%s", d->m_url_prefix, index, type, id);
    delete_handler_t handler { d->m_log };
    return d->do_rest_delete<response::delete_index>(url, handler);
}
