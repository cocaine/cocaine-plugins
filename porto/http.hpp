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

#ifndef COCAINE_DOCKER_HTTP_HPP
#define COCAINE_DOCKER_HTTP_HPP

#include <string>
#include <vector>
#include <memory>

#include <boost/optional.hpp>

namespace cocaine { namespace docker {

class http_headers_t {
    typedef std::vector<std::pair<std::string, std::string>>
            headers_vector_t;

public:
    explicit
    http_headers_t(const headers_vector_t& headers = headers_vector_t()) :
        m_headers(headers)
    {
        // pass
    }

    http_headers_t(headers_vector_t&& headers) :
        m_headers(std::move(headers))
    {
        // pass
    }

    http_headers_t(const http_headers_t& other) :
        m_headers(other.m_headers)
    {
        // pass
    }

    http_headers_t(http_headers_t&& other) :
        m_headers(std::move(other.m_headers))
    {
        // pass
    }

    http_headers_t&
    operator=(const http_headers_t& other) {
        m_headers = other.m_headers;
        return *this;
    }

    http_headers_t&
    operator=(http_headers_t&& other) {
        m_headers = std::move(other.m_headers);
        return *this;
    }

    const headers_vector_t&
    data() const {
        return m_headers;
    }

    std::vector<std::string>
    headers(const std::string& key) const;

    boost::optional<std::string>
    header(const std::string& key) const;

    // add new entry
    void
    add_header(const std::string& key,
               const std::string& value);

    // remove all previous 'key' entries and add new ones
    void
    reset_header(const std::string& key,
                 const std::vector<std::string>& values);

    // same as reset_header(key, std::vector<std::string> {value})
    void
    reset_header(const std::string& key,
                 const std::string& value);

private:
    headers_vector_t m_headers;
};

struct http_request_t {
    http_request_t() {
        // pass
    }

    http_request_t(const std::string& method,
                   const std::string& uri,
                   const std::string& http_version,
                   const http_headers_t& headers,
                   const std::string& body) :
        m_method(method),
        m_uri(uri),
        m_version(http_version),
        m_headers(headers),
        m_body(body)
    {
        // pass
    }

    http_request_t(const http_request_t& other) :
        m_method(other.m_method),
        m_uri(other.m_uri),
        m_version(other.m_version),
        m_headers(other.m_headers),
        m_body(other.m_body)
    {
        // pass
    }

    http_request_t(http_request_t&& other) :
        m_method(std::move(other.m_method)),
        m_uri(std::move(other.m_uri)),
        m_version(std::move(other.m_version)),
        m_headers(std::move(other.m_headers)),
        m_body(std::move(other.m_body))
    {
        // pass
    }

    http_request_t&
    operator=(http_request_t&& other) {
        m_method = std::move(other.m_method);
        m_uri = std::move(other.m_uri);
        m_version = std::move(other.m_version);
        m_headers = std::move(other.m_headers);
        m_body = std::move(other.m_body);

        return *this;
    }

    http_request_t&
    operator=(const http_request_t& other) {
        m_method = other.m_method;
        m_uri = other.m_uri;
        m_version = other.m_version;
        m_headers = other.m_headers;
        m_body = other.m_body;

        return *this;
    }

    const std::string&
    method() const {
        return m_method;
    }

    void
    set_method(const std::string& method) {
        m_method = method;
    }

    const std::string&
    uri() const {
        return m_uri;
    }

    void
    set_uri(const std::string& uri) {
        m_uri = uri;
    }

    const std::string&
    http_version() const {
        return m_version;
    }

    void
    set_http_version(const std::string& version) {
        m_version = version;
    }

    const http_headers_t&
    headers() const {
        return m_headers;
    }

    http_headers_t&
    headers() {
        return m_headers;
    }

    void
    set_headers(const http_headers_t& headers) {
        m_headers = headers;
    }

    const std::string&
    body() const {
        return m_body;
    }

    std::string&
    body() {
        return m_body;
    }

    void
    set_body(const std::string& body) {
        m_body = body;
    }

private:
    std::string m_method;
    std::string m_uri;
    std::string m_version;
    http_headers_t m_headers;
    std::string m_body;
};

struct http_response_t {
    http_response_t() {
        // pass
    }

    http_response_t(int code,
                    const http_headers_t& headers,
                    const std::string& body) :
        m_code(code),
        m_headers(headers),
        m_body(body)
    {
        // pass
    }

    http_response_t(http_response_t&& other) :
        m_code(std::move(other.m_code)),
        m_headers(std::move(other.m_headers)),
        m_body(std::move(other.m_body))
    {
        // pass
    }

    http_response_t(const http_response_t& other) :
        m_code(other.m_code),
        m_headers(other.m_headers),
        m_body(other.m_body)
    {
        // pass
    }

    http_response_t&
    operator=(http_response_t&& other) {
        m_code = std::move(other.m_code);
        m_headers = std::move(other.m_headers);
        m_body = std::move(other.m_body);

        return *this;
    }

    http_response_t&
    operator=(const http_response_t& other) {
        m_code = other.m_code;
        m_headers = other.m_headers;
        m_body = other.m_body;

        return *this;
    }

    int
    code() const {
        return m_code;
    }

    void
    set_code(int code) {
        m_code = code;
    }

    const http_headers_t&
    headers() const {
        return m_headers;
    }

    http_headers_t&
    headers() {
        return m_headers;
    }

    void
    set_headers(const http_headers_t& headers) {
        m_headers = headers;
    }

    const std::string&
    body() const {
        return m_body;
    }

    std::string&
    body() {
        return m_body;
    }

    void
    set_body(const std::string& body) {
        m_body = body;
    }

private:
    int m_code;
    http_headers_t m_headers;
    std::string m_body;
};

}} // namespace cocaine::docker

#endif // COCAINE_DOCKER_HTTP_HPP
