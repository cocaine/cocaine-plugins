#include "http.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include <system_error>
#include <algorithm>

using namespace cocaine::docker;

std::vector<std::string>
http_headers_t::headers(const std::string& key) const {
    std::vector<std::string> result;

    for (auto it = m_headers.begin(); it != m_headers.end(); ++it) {
        if (boost::iequals(it->first, key)) {
            result.push_back(it->second);
        }
    }

    return result;
}

boost::optional<std::string>
http_headers_t::header(const std::string& key) const {
    for (auto it = m_headers.begin(); it != m_headers.end(); ++it) {
        if (boost::iequals(it->first, key)) {
            return boost::optional<std::string>(it->second);
        }
    }

    return boost::optional<std::string>();
}

void
http_headers_t::add_header(const std::string& key,
                           const std::string& value)
{
    m_headers.emplace_back(key, value);
}

void
http_headers_t::reset_header(const std::string& key,
                             const std::vector<std::string>& values)
{
    headers_vector_t new_headers;
    new_headers.reserve(m_headers.size() + values.size());

    for (auto header = m_headers.begin(); header != m_headers.end(); ++header) {
        if (!boost::iequals(header->first, key)) {
            new_headers.push_back(*header);
        }
    }

    for (auto value = values.begin(); value != values.end(); ++value) {
        new_headers.emplace_back(key, *value);
    }

    m_headers.swap(new_headers);
}

void
http_headers_t::reset_header(const std::string& key,
                             const std::string& value)
{
    headers_vector_t new_headers;
    new_headers.reserve(m_headers.size() + 1);

    for (auto header = m_headers.begin(); header != m_headers.end(); ++header) {
        if (!boost::iequals(header->first, key)) {
            new_headers.push_back(*header);
        }
    }

    m_headers.swap(new_headers);

    add_header(key, value);
}
