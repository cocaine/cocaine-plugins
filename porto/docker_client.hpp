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

#ifndef COCAINE_DOCKER_CLIENT_HPP
#define COCAINE_DOCKER_CLIENT_HPP

#include "http.hpp"

#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <cocaine/common.hpp>
#include <cocaine/logging.hpp>

#include <boost/variant.hpp>
#include <boost/asio.hpp>

#include <curl/curl.h>

namespace cocaine { namespace docker {

class endpoint_t {
public:
    typedef std::pair<std::string, uint16_t>
            tcp_endpoint_t;

    typedef std::string
            unix_endpoint_t;

    static
    endpoint_t
    from_string(const std::string& endpoint);

    endpoint_t();

    endpoint_t(const tcp_endpoint_t& e);

    endpoint_t(const unix_endpoint_t& e);

    bool
    is_unix() const;

    bool
    is_tcp() const;

    const std::string&
    get_host() const;

    uint16_t
    get_port() const;

    const std::string&
    get_path() const;

    std::string
    to_string() const;

private:
    typedef boost::variant<unix_endpoint_t, tcp_endpoint_t>
            variant_t;

    mutable variant_t m_value;
};

class connection_t {
public:
    typedef boost::asio::local::stream_protocol::socket
            unix_socket_t;

    typedef boost::asio::ip::tcp::socket
            tcp_socket_t;

    connection_t();

    connection_t(boost::asio::io_service& ioservice,
                 const endpoint_t& endpoint,
                 std::shared_ptr<logging::log_t> logger);

    void
    connect(boost::asio::io_service& ioservice,
            const endpoint_t& endpoint,
            unsigned int connect_timeout = 10000);

    bool
    is_unix() const;

    bool
    is_tcp() const;

    std::shared_ptr<unix_socket_t>
    get_unix() const;

    std::shared_ptr<tcp_socket_t>
    get_tcp() const;

    int
    fd() const;

private:
    bool
    run_with_timeout(boost::asio::io_service& ioservice,
                     unsigned int timeout);

private:
    typedef boost::variant<std::shared_ptr<unix_socket_t>, std::shared_ptr<tcp_socket_t>>
            variant_t;

    mutable variant_t m_socket;
    std::shared_ptr<logging::log_t> m_logger;
};

class client_impl_t {
public:
    client_impl_t(const endpoint_t& endpoint, std::shared_ptr<logging::log_t> logger);

    ~client_impl_t();

    // Name of method defines how libcurl will do request, but request.method() is what will be written in request.
    // For example, if call head() with request.method() = "POST",
    // then libcurl will send post-request without body and will not read body of response.
    connection_t
    get(http_response_t& response,
        const http_request_t& request);

    connection_t
    post(http_response_t& response,
         const http_request_t& request);

    connection_t
    head(http_response_t& response,
         const http_request_t& request);

private:
    std::shared_ptr<logging::log_t> m_logger;
    boost::asio::io_service m_ioservice;
    endpoint_t m_endpoint;
    CURL *m_curl;
};

class container_t {
public:
    container_t(const std::string& id,
                std::shared_ptr<synchronized<client_impl_t>> client,
                std::shared_ptr<logging::log_t> logger) :
        m_id(id),
        m_client(client),
        m_logger(logger)
    {
        // pass
    }

    const std::string&
    id() const {
        return m_id;
    }

    void
    start(const rapidjson::Value& args);

    void
    kill();

    void
    stop(unsigned int timeout);

    void
    remove(bool volumes = false);

    connection_t
    attach();

private:
    std::string m_id;

    std::shared_ptr<synchronized<client_impl_t>> m_client;
    std::shared_ptr<logging::log_t> m_logger;
};

class client_t
{
public:
    client_t(const endpoint_t& endpoint,
             std::shared_ptr<logging::log_t> logger) :
        m_client(new synchronized<client_impl_t>(endpoint, logger)),
        m_logger(logger)
    {
        // pass
    }

    void
    inspect_image(rapidjson::Document& result,
                  const std::string& image);

    void
    pull_image(const std::string& image,
               const std::string& tag);

    container_t
    create_container(const rapidjson::Value& args);

private:
    std::shared_ptr<synchronized<client_impl_t>> m_client;
    std::shared_ptr<cocaine::logging::log_t> m_logger;
};

}} // namespace cocaine::docker

#endif // COCAINE_DOCKER_CLIENT_HPP
