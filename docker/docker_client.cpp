#include "docker_client.hpp"

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include <memory>
#include <system_error>
#include <errno.h>

using namespace cocaine::docker;

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
    if (endpoint.compare(0, 6, "tcp://") == 0) {
        size_t delim = endpoint.find(':', 6);

        if (delim != std::string::npos) {
            try {
                return endpoint_t(tcp_endpoint_t(
                    endpoint.substr(6, delim - 6),
                    boost::lexical_cast<uint16_t>(endpoint.substr(delim + 1))
                ));
            } catch (...) {
                throw std::runtime_error("Bad format of tcp endpoint.");
            }

        } else {
            throw std::runtime_error("Bad format of tcp endpoint.");
        }
    } else if (endpoint.compare(0, 7, "unix://") == 0) {
        return endpoint_t(endpoint.substr(7));
    } else {
        throw std::runtime_error("Bad format of tcp endpoint.");
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
                           const endpoint_t& endpoint)
{
    connect(ioservice, endpoint);
}

void
connection_t::connect(boost::asio::io_service& ioservice,
                      const endpoint_t& endpoint)
{
    if (endpoint.is_unix()) {
        auto s = std::make_shared<boost::asio::local::stream_protocol::socket>(ioservice);
        s->connect(boost::asio::local::stream_protocol::endpoint(endpoint.get_path()));
        m_socket = s;
    } else {
        boost::asio::ip::tcp::resolver resolver(ioservice);

        auto it = resolver.resolve(boost::asio::ip::tcp::resolver::query(
            endpoint.get_host(),
            boost::lexical_cast<std::string>(endpoint.get_port())
        ));
        auto end = boost::asio::ip::tcp::resolver::iterator();

        std::exception_ptr error;

        for (; it != end; ++it) {
            auto s = std::make_shared<boost::asio::ip::tcp::socket>(ioservice);
            try {
                s->connect(*it);
                m_socket = s;
                return;
            } catch (...) {
                error = std::current_exception();
            }
        }
        std::rethrow_exception(error);
    }
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
            if (!s) {
                throw std::runtime_error("Not connected.");
            }
            return s->native();
        }

        int
        operator()(const std::shared_ptr<connection_t::tcp_socket_t>& s) const {
            if (!s) {
                throw std::runtime_error("Not connected.");
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
        while (begin < end && isspace(*begin)) {
            ++begin;
        }

        while (begin < end && isspace(*(end - 1))) {
            --end;
        }

        if (begin < end) {
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

        if (delim != end) {
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

client_impl_t::client_impl_t(const endpoint_t& endpoint) :
    m_ioservice_ref(m_ioservice),
    m_endpoint(endpoint),
    m_curl(curl_easy_init())
{
    if (m_curl) {
        curl_easy_setopt(m_curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(m_curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP);

        curl_easy_setopt(m_curl, CURLOPT_OPENSOCKETFUNCTION, &open_callback);
        curl_easy_setopt(m_curl, CURLOPT_SOCKOPTFUNCTION, &sockopt_callback);
        curl_easy_setopt(m_curl, CURLOPT_CLOSESOCKETFUNCTION, &close_callback);
        curl_easy_setopt(m_curl, CURLOPT_HEADERFUNCTION, &header_callback);
        curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, &write_callback);

        curl_easy_setopt(m_curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
    } else {
        throw std::runtime_error("Unable to initialize libcurl.");
    }
}

client_impl_t::client_impl_t(boost::asio::io_service& ioservice,
              const endpoint_t& endpoint) :
    m_ioservice_ref(ioservice),
    m_endpoint(endpoint),
    m_curl(curl_easy_init())
{
    if (m_curl) {
        curl_easy_setopt(m_curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(m_curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP);

        curl_easy_setopt(m_curl, CURLOPT_OPENSOCKETFUNCTION, &open_callback);
        curl_easy_setopt(m_curl, CURLOPT_SOCKOPTFUNCTION, &sockopt_callback);
        curl_easy_setopt(m_curl, CURLOPT_CLOSESOCKETFUNCTION, &close_callback);
        curl_easy_setopt(m_curl, CURLOPT_HEADERFUNCTION, &header_callback);
        curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, &write_callback);

        curl_easy_setopt(m_curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
    } else {
        throw std::runtime_error("Unable to initialize libcurl.");
    }
}

client_impl_t::~client_impl_t() {
    if (m_curl) {
        curl_easy_cleanup(m_curl);
    }
}

connection_t
client_impl_t::get(http_response_t& response,
                   const http_request_t& request)
{
    connection_t socket(m_ioservice_ref, m_endpoint);

    std::string url;
    if (m_endpoint.is_tcp()) {
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

    if (!request.method().empty()) {
        curl_easy_setopt(m_curl, CURLOPT_CUSTOMREQUEST, request.method().c_str());
    }

    std::vector<std::string> headers;
    curl_slist *p_headers = NULL;
    for (size_t i = 0; i < request.headers().data().size(); ++i) {
        headers.push_back(
            request.headers().data()[i].first + ": " + request.headers().data()[i].second
        );
        p_headers = curl_slist_append(p_headers, headers.back().c_str());
    }

    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, p_headers);

    CURLcode errc = curl_easy_perform(m_curl);

    int code = 0;
    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &code);
    response.set_code(code);

    curl_slist_free_all(p_headers);

    if (errc != 0) {
        throw std::system_error(errc, std::system_category(), curl_easy_strerror(errc));
    }

    return socket;
}

connection_t
client_impl_t::post(http_response_t& response,
                    const http_request_t& request)
{
    connection_t socket(m_ioservice_ref, m_endpoint);

    std::string url;
    if (m_endpoint.is_tcp()) {
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

    if (!request.method().empty()) {
        curl_easy_setopt(m_curl, CURLOPT_CUSTOMREQUEST, request.method().c_str());
    }

    std::vector<std::string> headers;
    curl_slist *p_headers = NULL;
    for (size_t i = 0; i < request.headers().data().size(); ++i) {
        headers.push_back(
            request.headers().data()[i].first + ": " + request.headers().data()[i].second
        );
        p_headers = curl_slist_append(p_headers, headers.back().c_str());
    }

    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, p_headers);

    CURLcode errc = curl_easy_perform(m_curl);

    int code = 0;
    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &code);
    response.set_code(code);

    curl_slist_free_all(p_headers);

    if (errc != 0) {
        throw std::system_error(errc, std::system_category(), curl_easy_strerror(errc));
    }

    return socket;
}

connection_t
client_impl_t::head(http_response_t& response,
                    const http_request_t& request)
{
    connection_t socket(m_ioservice_ref, m_endpoint);

    std::string url;
    if (m_endpoint.is_tcp()) {
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

    if (!request.method().empty()) {
        curl_easy_setopt(m_curl, CURLOPT_CUSTOMREQUEST, request.method().c_str());
    }

    std::vector<std::string> headers;
    curl_slist *p_headers = NULL;
    for (size_t i = 0; i < request.headers().data().size(); ++i) {
        headers.push_back(
            request.headers().data()[i].first + ": " + request.headers().data()[i].second
        );
        p_headers = curl_slist_append(p_headers, headers.back().c_str());
    }

    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, p_headers);

    CURLcode errc = curl_easy_perform(m_curl);

    int code = 0;
    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &code);
    response.set_code(code);

    curl_slist_free_all(p_headers);

    if (errc != 0) {
        throw std::system_error(errc, std::system_category(), curl_easy_strerror(errc));
    }

    return socket;
}

namespace {
    http_request_t
    make_post(const std::string& url,
              const rapidjson::Value& value = rapidjson::Value())
    {
        http_request_t request("POST", url, "1.0", http_headers_t(), "");

        if (!value.IsNull()) {
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
container_t::start(const std::vector<std::string>& binds) {
    rapidjson::Value args;
    rapidjson::Value b;
    rapidjson::Value::AllocatorType allocator;

    b.SetArray();
    for (auto it = binds.begin(); it != binds.end(); ++it) {
        b.PushBack(it->data(), allocator);
    }

    args.SetObject();
    args.AddMember("Binds", b, allocator);

    http_response_t resp;
    m_client->post(resp, make_post(cocaine::format("/containers/%s/start", id()), args));

    if (!(resp.code() >= 200 && resp.code() < 300)) {
        COCAINE_LOG_WARNING(m_logger,
                            "Unable to start container %s. Docker replied with code %d and body '%s'.",
                            id(),
                            resp.code(),
                            resp.body());
        throw std::runtime_error("Unable to start container " + id() + ".");
    }
}

void
container_t::kill() {
    http_response_t resp;
    m_client->post(resp, make_post(cocaine::format("/containers/%s/kill", id())));

    if (!(resp.code() >= 200 && resp.code() < 300)) {
        COCAINE_LOG_WARNING(m_logger,
                            "Unable to kill container %s. Docker replied with code %d and body '%s'.",
                            id(),
                            resp.code(),
                            resp.body());
        throw std::runtime_error("Unable to kill container " + id() + ".");
    }
}

void
container_t::stop(unsigned int timeout) {
    http_response_t resp;
    m_client->post(resp, make_post(cocaine::format("/containers/%s/stop?t=%d", id(), timeout)));

    if (!(resp.code() >= 200 && resp.code() < 300)) {
        COCAINE_LOG_WARNING(m_logger,
                            "Unable to stop container %s. Docker replied with code %d and body '%s'.",
                            id(),
                            resp.code(),
                            resp.body());
        throw std::runtime_error("Unable to stop container " + id() + ".");
    }
}

void
container_t::remove(bool volumes) {
    http_response_t resp;
    m_client->get(resp, make_del(cocaine::format("/containers/%s?v=%d", id(), volumes?1:0)));

    if (!(resp.code() >= 200 && resp.code() < 300)) {
        COCAINE_LOG_WARNING(m_logger,
                            "Unable to remove container %s. Docker replied with code %d and body '%s'.",
                            id(),
                            resp.code(),
                            resp.body());
        throw std::runtime_error("Unable to remove container " + id() + ".");
    }

    COCAINE_LOG_DEBUG(m_logger,
                      "Container %s has been deleted. Docker replied with code %d and body '%s'.",
                      id(),
                      resp.code(),
                      resp.body());
}

connection_t
container_t::attach() {
    http_response_t resp;
    auto conn = m_client->head(
        resp,
        make_post(cocaine::format("/containers/%s/attach?logs=1&stream=1&stdout=1&stderr=1", id()))
    );

    if (!(resp.code() >= 200 && resp.code() < 300)) {
        COCAINE_LOG_WARNING(m_logger,
                            "Unable to attach container %s. Docker replied with code %d and body '%s'.",
                            id(),
                            resp.code(),
                            resp.body());
        throw std::runtime_error("Unable to attach container " + id() + ".");
    }

    return conn;
}

void
client_t::inspect_image(rapidjson::Document& result,
                        const std::string& image)
{
    http_response_t resp;
    m_client->get(resp, make_get(cocaine::format("/images/%s/json", image)));

    if (resp.code() >= 200 && resp.code() < 300) {
        result.SetNull();
        result.Parse<0>(resp.body().data());
    } else if (resp.code() >= 400 && resp.code() < 500) {
        result.SetNull();
    } else {
        COCAINE_LOG_WARNING(m_logger,
                            "Unable to inspect an image. Docker replied with code %d and body '%s'.",
                            resp.code(),
                            resp.body());
        throw std::runtime_error("Unable to inspect an image.");
    }
}

void
client_t::pull_image(const std::string& registry,
                     const std::string& image,
                     const std::string& tag)
{
    std::string request = "/images/create?";

    std::pair<std::string, std::string> args[3];
    size_t args_count = 1;

    args[0] = std::pair<std::string, std::string>("fromImage", image);
    if (!registry.empty()) {
        args[args_count] = std::pair<std::string, std::string>("registry", registry);
        ++args_count;
    }
    if (!tag.empty()) {
        args[args_count] = std::pair<std::string, std::string>("tag", tag);
        ++args_count;
    }

    for (size_t i = 0; i < args_count; ++i) {
        if (i == 0) {
            request += args[0].first + "=" + args[0].second;
        } else {
            request += "&" + args[0].first + "=" + args[0].second;
        }
    }

    http_response_t resp;
    m_client->post(resp, make_post(request));

    if (resp.code() >= 200 && resp.code() < 300) {
        std::string body = resp.body();
        size_t next_object = 0;
        std::vector<std::string> messages;

        // kostyl-way 7 ultimate
        while (true) {
            size_t end = body.find("}{", next_object);

            if (end == std::string::npos) {
                messages.push_back(body.substr(next_object));
                break;
            } else {
                messages.push_back(body.substr(next_object, end + 1 - next_object));
                next_object = end + 1;
            }
        }

        for (auto it = messages.begin(); it != messages.end(); ++it) {
            rapidjson::Document answer;
            answer.Parse<0>(it->data());

            if (answer.HasMember("error")) {
                COCAINE_LOG_ERROR(m_logger,
                                  "Unable to create an image. Docker replied with body: '%s'.",
                                  resp.body());

                throw std::runtime_error("Unable to create an image.");
            }
        }
    } else {
        COCAINE_LOG_ERROR(m_logger,
                          "Unable to create an image. Docker replied with code %d and body '%s'.",
                          resp.code(),
                          resp.body());
        throw std::runtime_error("Unable to create an image.");
    }
}

container_t
client_t::create_container(const rapidjson::Value& args) {
    http_response_t resp;
    m_client->post(resp, make_post("/containers/create", args));

    if (resp.code() >= 200 && resp.code() < 300) {
        rapidjson::Document answer;
        answer.Parse<0>(resp.body().data());

        if (!answer.HasMember("Id")) {
            COCAINE_LOG_WARNING(m_logger,
                                "Unable to create a container. Id not found in reply from the docker: '%s'.",
                                resp.body());
            throw std::runtime_error("Unable to create a container.");
        }

        if (answer.HasMember("Warnings")) {
            auto& warnings = answer["Warnings"];

            for (auto it = warnings.Begin(); it != warnings.End(); ++it) {
                COCAINE_LOG_WARNING(m_logger,
                                    "Warning from docker: '%s'.",
                                    it->GetString());
            }
        }

        return container_t(answer["Id"].GetString(), m_client, m_logger);
    } else {
        COCAINE_LOG_WARNING(m_logger,
                            "Unable to create a container. Docker replied with code %d and body '%s'.",
                            resp.code(),
                            resp.body());
        throw std::runtime_error("Unable to create a container.");
    }
}
