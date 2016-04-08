/*
    Copyright (c) 2011-2013 Andrey Goryachev <andrey.goryachev@gmail.com>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "docker_client.hpp"

#include <cocaine/format.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include <blackhole/logger.hpp>

#include <chrono>
#include <iostream>
#include <memory>
#include <system_error>

#include <errno.h>

namespace cocaine {
namespace error {
namespace {

class docker_curl_category_t:
    public std::error_category
{
public:
    virtual
    const char*
    name() const noexcept {
        return "curl category";
    }

    virtual
    std::string
    message(int ec) const noexcept {
        return curl_easy_strerror(static_cast<CURLcode>(ec));
    }
};

} // namespace

const std::error_category&
docker_curl_category() {
    static docker_curl_category_t category;
    return category;
}

} // namespace error
} // namespace cocaine

using namespace cocaine;
using namespace cocaine::docker;

namespace {

// This magic is intended to run curl_global_init during static initialization in single-threaded
// environment. See http://curl.haxx.se/libcurl/c/curl_global_init.html for details.
int
curl_init() {
    if(curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) {
        std::cout << "curl initialization failed - aborting execution" << std::endl;
        std::terminate();
    }

    return 42;
}

int curl_init_placeholder = curl_init();

} // namespace

endpoint_t::endpoint_t() {
    // pass
}

endpoint_t::endpoint_t(const tcp_endpoint_t& e) :
    m_value(e)
{
    // pass
}

endpoint_t::endpoint_t(const unix_endpoint_t& e) :
    m_value(e)
{
    // pass
}

endpoint_t
endpoint_t::from_string(const std::string& endpoint) {
    if(endpoint.compare(0, 6, "tcp://") == 0) {
        size_t delim = endpoint.find(':', 6);

        if(delim != std::string::npos) {
            try {
                return endpoint_t(tcp_endpoint_t(
                    endpoint.substr(6, delim - 6),
                    boost::lexical_cast<uint16_t>(endpoint.substr(delim + 1))
                ));
            } catch (...) {
                throw std::runtime_error("Bad format of tcp endpoint");
            }

        } else {
            throw std::runtime_error("Bad format of tcp endpoint");
        }
    } else if(endpoint.compare(0, 7, "unix://") == 0) {
        return endpoint_t(endpoint.substr(7));
    } else {
        throw std::runtime_error("Bad format of tcp endpoint");
    }
}

bool
endpoint_t::is_unix() const {
    return static_cast<bool>(boost::get<unix_endpoint_t>(&m_value));
}

bool
endpoint_t::is_tcp() const {
    return static_cast<bool>(boost::get<tcp_endpoint_t>(&m_value));
}

const std::string&
endpoint_t::get_host() const {
    return boost::get<tcp_endpoint_t>(m_value).first;
}

uint16_t
endpoint_t::get_port() const {
    return boost::get<tcp_endpoint_t>(m_value).second;
}

const std::string&
endpoint_t::get_path() const {
    return boost::get<unix_endpoint_t>(m_value);
}

namespace {
    struct to_string_visitor :
        public boost::static_visitor<std::string>
    {
        std::string
        operator()(const std::pair<std::string, uint16_t>& e) const {
            return "tcp://" + e.first + ":" + boost::lexical_cast<std::string>(e.second);
        }

        std::string
        operator()(const std::string& e) const {
            return "unix://" + e;
        }
    };
} // namespace

std::string
endpoint_t::to_string() const {
    return boost::apply_visitor(to_string_visitor(), m_value);
}


connection_t::connection_t() {
    // pass
}

connection_t::connection_t(boost::asio::io_service& ioservice,
                           const endpoint_t& endpoint,
                           std::shared_ptr<logging::logger_t> logger): m_logger(std::move(logger))
{
    connect(ioservice, endpoint);
}

namespace {

    void
    timeout_handler(const boost::system::error_code& error,
                    const std::shared_ptr<logging::logger_t>& logger,
                    bool& exit_with_timeout,
                    boost::asio::io_service& ioservice)
    {
        if(!error) {
            COCAINE_LOG_DEBUG(logger, "connection timeout");
            exit_with_timeout = true;
            ioservice.stop();
        } else {
            COCAINE_LOG_DEBUG(logger, "connection timer was cancelled: {}", error.message());
        }
    }

    void
    local_connection_handler(const boost::system::error_code& error,
                             const std::shared_ptr<logging::logger_t>& logger,
                             boost::system::error_code& result,
                             boost::asio::io_service& ioservice)
    {
        if(error == boost::asio::error::operation_aborted) {
            COCAINE_LOG_DEBUG(logger, "connection to unix socket aborted");
            return;
        } else if(error) {
            COCAINE_LOG_DEBUG(logger, "connection to unix socket failed: {}", error.message());
            result = error;
        }

        ioservice.stop();
    }

    struct tcp_connector {
        void
        resolve_handler(const boost::system::error_code& error,
                        const std::shared_ptr<logging::logger_t>& logger,
                        boost::asio::ip::tcp::resolver::iterator endpoint)
        {
            this->error = error;

            if(error == boost::asio::error::operation_aborted) {
                COCAINE_LOG_DEBUG(logger, "resolve of tcp address aborted");
                return;
            } else if(error) {
                COCAINE_LOG_DEBUG(logger, "resolve of tcp address failed: {}", error.message());
                this->ioservice.stop();
            } else if (endpoint == boost::asio::ip::tcp::resolver::iterator()) {
                COCAINE_LOG_DEBUG(logger, "resolve of tcp address failed: last endpoint reached");
                this->ioservice.stop();
            } else {
                endpoints.assign(endpoint, boost::asio::ip::tcp::resolver::iterator());
                socket = std::make_shared<boost::asio::ip::tcp::socket>(ioservice);
                COCAINE_LOG_DEBUG(logger, "connecting the socket");
                socket->async_connect(*endpoint,
                                      std::bind(&tcp_connector::handler,
                                                this,
                                                std::placeholders::_1,
                                                logger,
                                                1));
            }
        }
        void
        handler(const boost::system::error_code& error,
                const std::shared_ptr<logging::logger_t>& logger,
                size_t next_endpoint)
        {
            if(error == boost::asio::error::operation_aborted) {
                COCAINE_LOG_DEBUG(logger, "connect to tcp address aborted");
                return;
            } else if(!error) {
                COCAINE_LOG_DEBUG(logger, "connect to tcp address succeeded");
                this->ioservice.stop();
            } else if(next_endpoint < endpoints.size()) {
                COCAINE_LOG_DEBUG(logger, "connect to tcp address failed: {}, trying next endpoint", error.message());
                socket = std::make_shared<boost::asio::ip::tcp::socket>(ioservice);
                socket->async_connect(endpoints[next_endpoint],
                                      std::bind(&tcp_connector::handler,
                                                this,
                                                std::placeholders::_1,
                                                logger,
                                                next_endpoint + 1));
            } else {
                COCAINE_LOG_DEBUG(logger, "connect to tcp address failed: {}", error.message());
                this->error = error;
                this->socket.reset();
                this->ioservice.stop();
            }
        }

        boost::asio::io_service& ioservice;
        std::shared_ptr<boost::asio::ip::tcp::socket> socket;
        boost::system::error_code error;

        std::vector<boost::asio::ip::tcp::endpoint> endpoints;
    };

} // namespace

void
connection_t::connect(boost::asio::io_service& ioservice,
                      const endpoint_t& endpoint,
                      unsigned int connect_timeout)
{
    ioservice.reset();

    if(endpoint.is_unix()) {
        auto s = std::make_shared<boost::asio::local::stream_protocol::socket>(ioservice);
        boost::system::error_code error;
        COCAINE_LOG_DEBUG(m_logger, "creating a connection to {}", endpoint.get_path());

        s->async_connect(boost::asio::local::stream_protocol::endpoint(endpoint.get_path()),
                         std::bind(&local_connection_handler,
                                   std::placeholders::_1,
                                   m_logger,
                                   std::ref(error),
                                   std::ref(ioservice)));

        if(run_with_timeout(ioservice, connect_timeout)) {
            throw std::runtime_error("Connection timed out");
        } else if(error) {
            throw boost::system::system_error(error);
        }

        m_socket = s;
    } else {
        boost::asio::ip::tcp::resolver resolver(ioservice);

        tcp_connector conn = {
            ioservice,
            std::shared_ptr<boost::asio::ip::tcp::socket>(),
            boost::system::error_code(),
            std::vector<boost::asio::ip::tcp::endpoint>()
        };

        resolver.async_resolve(
            boost::asio::ip::tcp::resolver::query(
                endpoint.get_host(),
                boost::lexical_cast<std::string>(endpoint.get_port())
            ),
            std::bind(&tcp_connector::resolve_handler,
                      &conn,
                      std::placeholders::_1,
                      m_logger,
                      std::placeholders::_2)
        );

        if(run_with_timeout(ioservice, connect_timeout)) {
            throw std::runtime_error("Connection timed out");
        } else if(conn.socket) {
            m_socket = conn.socket;
        } else {
            throw boost::system::system_error(conn.error);
        }
    }
}

bool
connection_t::run_with_timeout(boost::asio::io_service& ioservice,
                               unsigned int timeout)
{
    bool exited_with_timeout = false;

    boost::asio::deadline_timer timer(ioservice);
    timer.expires_from_now(boost::posix_time::milliseconds(timeout));
    timer.async_wait(std::bind(&timeout_handler,
                               std::placeholders::_1,
                               m_logger,
                               std::ref(exited_with_timeout),
                               std::ref(ioservice)));

    ioservice.run();

    return exited_with_timeout;
}

bool
connection_t::is_unix() const {
    return static_cast<bool>(boost::get<std::shared_ptr<unix_socket_t>>(&m_socket));
}

bool
connection_t::is_tcp() const {
    return static_cast<bool>(boost::get<std::shared_ptr<tcp_socket_t>>(&m_socket));
}

std::shared_ptr<connection_t::unix_socket_t>
connection_t::get_unix() const {
    return boost::get<std::shared_ptr<unix_socket_t>>(m_socket);
}

std::shared_ptr<connection_t::tcp_socket_t>
connection_t::get_tcp() const {
    return boost::get<std::shared_ptr<tcp_socket_t>>(m_socket);
}

namespace {

    struct fd_visitor :
        public boost::static_visitor<int>
    {
        int
        operator()(const std::shared_ptr<connection_t::unix_socket_t>& s) const {
            if(!s) {
                throw std::runtime_error("Not connected");
            }
            return s->native();
        }

        int
        operator()(const std::shared_ptr<connection_t::tcp_socket_t>& s) const {
            if(!s) {
                throw std::runtime_error("Not connected");
            }
            return s->native();
        }
    };

}

int
connection_t::fd() const {
    return boost::apply_visitor(fd_visitor(), m_socket);
}


namespace {

    curl_socket_t
    open_callback(void* user_data,
                  curlsocktype /* purpose */,
                  curl_sockaddr * /* address */)
    {
        return static_cast<connection_t*>(user_data)->fd();
    }

    int
    sockopt_callback(void * /* user_data */,
                     curl_socket_t /* fd */,
                     curlsocktype /* purpose */)
    {
        return CURL_SOCKOPT_ALREADY_CONNECTED;
    }

    int
    close_callback(void * /* user_data */,
                   curl_socket_t /* fd */)
    {
        return 0;
    }

    std::string
    strip(const char *begin,
          const char *end)
    {
        while(begin < end && isspace(*begin)) {
            ++begin;
        }

        while(begin < end && isspace(*(end - 1))) {
            --end;
        }

        if(begin < end) {
            return std::string(begin, end - begin);
        } else {
            return std::string();
        }
    }

    size_t
    header_callback(const char *header,
                    size_t size,
                    size_t nmemb,
                    void *user_data)
    {
        const char *end = header + size * nmemb;
        const char *delim = std::find(header, end, ':');

        if(delim != end) {
            const char *last_char = std::find(delim, end, '\n');

            std::string field = strip(header, delim);
            std::string value = strip(delim + 1, last_char);

            static_cast<http_response_t*>(user_data)
                ->headers().add_header(std::move(field), std::move(value));
        }

        return size * nmemb;
    }

    size_t
    write_callback(const char *body,
                   size_t size,
                   size_t nmemb,
                   void *user_data)
    {
        static_cast<http_response_t*>(user_data)->body() += std::string(body, size * nmemb);
        return size * nmemb;
    }
}

client_impl_t::client_impl_t(const endpoint_t& endpoint, std::shared_ptr<logging::logger_t> logger) :
    m_logger(std::move(logger)),
    m_endpoint(endpoint),
    m_curl(curl_easy_init())
{
    if(m_curl) {
        curl_easy_setopt(m_curl, CURLOPT_NOSIGNAL, 1L);
        // This is a magic number similar to connection timeout in .hpp
        // TODO: Move it somewhere out.
        curl_easy_setopt(m_curl, CURLOPT_TIMEOUT_MS, 100000);
        curl_easy_setopt(m_curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP);
        curl_easy_setopt(m_curl, CURLOPT_FORBID_REUSE, 1L);

        curl_easy_setopt(m_curl, CURLOPT_OPENSOCKETFUNCTION, &open_callback);
        curl_easy_setopt(m_curl, CURLOPT_SOCKOPTFUNCTION, &sockopt_callback);
        curl_easy_setopt(m_curl, CURLOPT_CLOSESOCKETFUNCTION, &close_callback);
        curl_easy_setopt(m_curl, CURLOPT_HEADERFUNCTION, &header_callback);
        curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, &write_callback);

        curl_easy_setopt(m_curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
    } else {
        throw std::runtime_error("Unable to initialize libcurl");
    }
}

client_impl_t::~client_impl_t() {
    if(m_curl) {
        curl_easy_cleanup(m_curl);
    }
}

connection_t
client_impl_t::get(http_response_t& response,
                   const http_request_t& request)
{
    connection_t socket(m_ioservice, m_endpoint, m_logger);

    std::string url;
    if(m_endpoint.is_tcp()) {
        url = "http://"
            + m_endpoint.get_host() + ":" + boost::lexical_cast<std::string>(m_endpoint.get_port())
            + request.uri();
    } else {
        url = std::string("http://")
            + "127.0.0.1"
            + request.uri();
    }

    curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());

    curl_easy_setopt(m_curl, CURLOPT_OPENSOCKETDATA, &socket);
    curl_easy_setopt(m_curl, CURLOPT_CLOSESOCKETDATA, 0);
    curl_easy_setopt(m_curl, CURLOPT_HEADERDATA, &response);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &response);

    curl_easy_setopt(m_curl, CURLOPT_HTTPGET, 1L);

    if(!request.method().empty()) {
        curl_easy_setopt(m_curl, CURLOPT_CUSTOMREQUEST, request.method().c_str());
    }

    std::vector<std::string> headers;
    headers.reserve(request.headers().data().size());
    for(size_t i = 0; i < request.headers().data().size(); ++i) {
        headers.push_back(
            request.headers().data()[i].first + ": " + request.headers().data()[i].second
        );
    }

    curl_slist *p_headers = NULL;
    for(size_t i = 0; i < headers.size(); ++i) {
        p_headers = curl_slist_append(p_headers, headers[i].c_str());
    }

    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, p_headers);

    CURLcode errc = curl_easy_perform(m_curl);

    long code = 0;
    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &code);
    response.set_code(code);

    curl_slist_free_all(p_headers);

    if(errc != 0) {
        throw std::system_error(errc, error::docker_curl_category());
    }

    return socket;
}

connection_t
client_impl_t::post(http_response_t& response,
                    const http_request_t& request)
{
    connection_t socket(m_ioservice, m_endpoint, m_logger);

    std::string url;
    if(m_endpoint.is_tcp()) {
        url = "http://"
            + m_endpoint.get_host() + ":" + boost::lexical_cast<std::string>(m_endpoint.get_port())
            + request.uri();
    } else {
        url = std::string("http://")
            + "127.0.0.1"
            + request.uri();
    }

    curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());

    curl_easy_setopt(m_curl, CURLOPT_OPENSOCKETDATA, &socket);
    curl_easy_setopt(m_curl, CURLOPT_CLOSESOCKETDATA, 0);
    curl_easy_setopt(m_curl, CURLOPT_HEADERDATA, &response);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &response);

    curl_easy_setopt(m_curl, CURLOPT_POST, 1L);
    curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, request.body().data());
    curl_easy_setopt(m_curl, CURLOPT_POSTFIELDSIZE, request.body().size());

    if(!request.method().empty()) {
        curl_easy_setopt(m_curl, CURLOPT_CUSTOMREQUEST, request.method().c_str());
    }

    std::vector<std::string> headers;
    headers.reserve(request.headers().data().size());
    for(size_t i = 0; i < request.headers().data().size(); ++i) {
        headers.push_back(
            request.headers().data()[i].first + ": " + request.headers().data()[i].second
        );
    }

    curl_slist *p_headers = NULL;
    for(size_t i = 0; i < headers.size(); ++i) {
        p_headers = curl_slist_append(p_headers, headers[i].c_str());
    }

    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, p_headers);

    CURLcode errc = curl_easy_perform(m_curl);

    long code = 0;
    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &code);
    response.set_code(code);

    curl_slist_free_all(p_headers);

    if(errc != 0) {
        throw std::system_error(errc, error::docker_curl_category());
    }

    return socket;
}

connection_t
client_impl_t::head(http_response_t& response,
                    const http_request_t& request)
{
    connection_t socket(m_ioservice, m_endpoint, m_logger);

    std::string url;
    if(m_endpoint.is_tcp()) {
        url = "http://"
            + m_endpoint.get_host() + ":" + boost::lexical_cast<std::string>(m_endpoint.get_port())
            + request.uri();
    } else {
        url = std::string("http://")
            + "127.0.0.1"
            + request.uri();
    }

    curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());

    curl_easy_setopt(m_curl, CURLOPT_OPENSOCKETDATA, &socket);
    curl_easy_setopt(m_curl, CURLOPT_CLOSESOCKETDATA, 0);
    curl_easy_setopt(m_curl, CURLOPT_HEADERDATA, &response);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &response);

    curl_easy_setopt(m_curl, CURLOPT_NOBODY, 1L);

    if(!request.method().empty()) {
        curl_easy_setopt(m_curl, CURLOPT_CUSTOMREQUEST, request.method().c_str());
    }

    std::vector<std::string> headers;
    headers.reserve(request.headers().data().size());
    for(size_t i = 0; i < request.headers().data().size(); ++i) {
        headers.push_back(
            request.headers().data()[i].first + ": " + request.headers().data()[i].second
        );
    }

    curl_slist *p_headers = NULL;
    for(size_t i = 0; i < headers.size(); ++i) {
        p_headers = curl_slist_append(p_headers, headers[i].c_str());
    }

    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, p_headers);

    CURLcode errc = curl_easy_perform(m_curl);

    long code = 0;
    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &code);
    response.set_code(code);

    curl_slist_free_all(p_headers);

    if(errc != 0) {
        throw std::system_error(errc, error::docker_curl_category());
    }

    return socket;
}

namespace {
    std::string api_version = "/v1.14";

    http_request_t
    make_post(const std::string& url,
              const rapidjson::Value& value = rapidjson::Value())
    {
        http_request_t request("POST", url, "1.0", http_headers_t(), "");

        if(!value.IsNull()) {
            rapidjson::GenericStringBuffer<rapidjson::UTF8<>> buffer;
            rapidjson::Writer<rapidjson::GenericStringBuffer<rapidjson::UTF8<>>> writer(buffer);

            value.Accept(writer);

            request.body().assign(buffer.GetString(), buffer.Size());

            request.headers().reset_header("Content-Type", "application/json");
            request.headers().reset_header("Content-Length",
                                           boost::lexical_cast<std::string>(buffer.Size()));
        }

        return request;
    }

    http_request_t
    make_get(const std::string& url) {
        return http_request_t("GET", url, "1.0", http_headers_t(), "");
    }

    http_request_t
    make_del(const std::string& url) {
        return http_request_t("DELETE", url, "1.0", http_headers_t(), "");
    }
}

void
container_t::start(const rapidjson::Value& args) {
    http_response_t resp;

    m_client->synchronize()->post(resp, make_post(api_version + cocaine::format("/containers/{}/start", id()), args));

    if(!(resp.code() >= 200 && resp.code() < 400)) {
        COCAINE_LOG_WARNING(m_logger,
                            "unable to start container {}: Docker replied with code {} and body '{}'",
                            id(),
                            resp.code(),
                            resp.body());
        throw std::runtime_error("Unable to start container " + id());
    }
}

void
container_t::kill() {
    http_response_t resp;
    m_client->synchronize()->post(resp, make_post(api_version + cocaine::format("/containers/{}/kill", id())));

    if(!(resp.code() >= 200 && resp.code() < 400)) {
        COCAINE_LOG_WARNING(m_logger,
                            "unable to kill container {}: Docker replied with code {} and body '{}'",
                            id(),
                            resp.code(),
                            resp.body());
        throw std::runtime_error("Unable to kill container " + id());
    }
}

void
container_t::stop(unsigned int timeout) {
    http_response_t resp;
    m_client->synchronize()->post(resp, make_post(api_version + cocaine::format("/containers/{}/stop?t={}", id(), timeout)));

    if(!(resp.code() >= 200 && resp.code() < 400)) {
        COCAINE_LOG_WARNING(m_logger,
                            "unable to stop container {}: Docker replied with code {} and body '{}'",
                            id(),
                            resp.code(),
                            resp.body());
        throw std::runtime_error("Unable to stop container " + id());
    }
}

void
container_t::remove(bool volumes) {
    http_response_t resp;
    m_client->synchronize()->get(resp, make_del(api_version + cocaine::format("/containers/{}?v={}", id(), volumes?1:0)));

    if(!(resp.code() >= 200 && resp.code() < 300)) {
        COCAINE_LOG_WARNING(m_logger,
                            "Unable to remove container {}. Docker replied with code {} and body '{}'",
                            id(),
                            resp.code(),
                            resp.body());
        throw std::runtime_error("Unable to remove container " + id());
    }

    COCAINE_LOG_DEBUG(m_logger,
                      "container {} has been deleted: Docker replied with code {} and body '{}'",
                      id(),
                      resp.code(),
                      resp.body());
}

connection_t
container_t::attach() {
    http_response_t resp;
    auto conn = m_client->synchronize()->head(
        resp,
        make_post(api_version + cocaine::format("/containers/{}/attach?logs=1&stream=1&stdout=1&stderr=1", id()))
    );

    if(!(resp.code() >= 200 && resp.code() < 300)) {
        COCAINE_LOG_WARNING(m_logger,
                            "Unable to attach container {}. Docker replied with code {} and body '{}'",
                            id(),
                            resp.code(),
                            resp.body());
        throw std::runtime_error("Unable to attach container " + id());
    }

    return conn;
}

void
client_t::inspect_image(rapidjson::Document& result,
                        const std::string& image)
{
    http_response_t resp;
    m_client->synchronize()->get(resp, make_get(api_version + cocaine::format("/images/{}/json", image)));

    if(resp.code() >= 200 && resp.code() < 300) {
        result.SetNull();
        result.Parse<0>(resp.body().data());
    } else if(resp.code() >= 400 && resp.code() < 500) {
        result.SetNull();
    } else {
        COCAINE_LOG_WARNING(m_logger,
                            "unable to inspect an image: Docker replied with code {} and body '{}'",
                            resp.code(),
                            resp.body());
        throw std::runtime_error("Unable to inspect an image");
    }
}

void
client_t::pull_image(const std::string& image,
                     const std::string& tag)
{
    std::string request = "/images/create?";

    std::pair<std::string, std::string> args[2];
    size_t args_count = 1;

    args[0] = std::pair<std::string, std::string>("fromImage", image);
    if(!tag.empty()) {
        args[args_count] = std::pair<std::string, std::string>("tag", tag);
        ++args_count;
    }

    for(size_t i = 0; i < args_count; ++i) {
        if(i == 0) {
            request += args[0].first + "=" + args[0].second;
        } else {
            request += "&" + args[0].first + "=" + args[0].second;
        }
    }

    http_response_t resp;
    m_client->synchronize()->post(resp, make_post(api_version + request));

    if(resp.code() >= 200 && resp.code() < 300) {
        rapidjson::GenericStringStream<rapidjson::UTF8<>> stream(resp.body().data());

        while(stream.Peek() != '\0') {
            rapidjson::Document status;
            status.ParseStream<rapidjson::kParseStreamFlag>(stream);

            if(status.HasParseError() || (status.IsObject() && status.HasMember("error"))) {
                COCAINE_LOG_WARNING(m_logger,
                                    "unable to create an image: Docker replied with body: '{}'",
                                    resp.body());

                throw std::runtime_error("Unable to create an image");
            }
        }
    } else {
        COCAINE_LOG_ERROR(m_logger,
                          "unable to create an image: Docker replied with code {} and body '{}'",
                          resp.code(),
                          resp.body());
        throw std::runtime_error("Unable to create an image");
    }
}

container_t
client_t::create_container(const rapidjson::Value& args) {
    http_response_t resp;
    m_client->synchronize()->post(resp, make_post(api_version + "/containers/create", args));

    if(resp.code() >= 200 && resp.code() < 300) {
        rapidjson::Document answer;
        answer.Parse<0>(resp.body().data());

        if(!answer.HasMember("Id")) {
            COCAINE_LOG_WARNING(m_logger,
                                "unable to create a container: id not found in reply from the docker: '{}'",
                                resp.body());
            throw std::runtime_error("Unable to create a container");
        }

        if(answer.HasMember("Warnings") && answer["Warnings"].IsArray()) {
            auto& warnings = answer["Warnings"];

            for(auto it = warnings.Begin(); it != warnings.End(); ++it) {
                COCAINE_LOG_WARNING(m_logger, "warning from docker: '{}'", it->GetString());
            }
        }

        return container_t(answer["Id"].GetString(), m_client, m_logger);
    } else {
        COCAINE_LOG_WARNING(m_logger,
                            "unable to create a container: Docker replied with code {} and body '{}'",
                            resp.code(),
                            resp.body());
        throw std::runtime_error("Unable to create a container");
    }
}
