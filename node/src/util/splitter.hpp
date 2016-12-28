#pragma once

#include <string>

#include <boost/optional/optional.hpp>

namespace cocaine {
namespace util {

/// Helper generator that consumes strings and yields them splitted by the given separator.
class splitter_t {
    char sep;
    std::string unparsed;

public:
    splitter_t() :
        sep('\n')
    {}

    explicit
    splitter_t(char sep) :
        sep(sep)
    {}

    boost::optional<std::string>
    next() {
        const auto pos = unparsed.find(sep);
        if (pos == std::string::npos) {
            return boost::none;
        }

        auto line = unparsed.substr(0, pos);
        unparsed.erase(0, pos + 1);
        return boost::make_optional(line);
    }

    bool
    empty() const {
        return unparsed.empty();
    }

    const std::string&
    data() const {
        return unparsed;
    }

    void
    consume(const std::string& data) {
        unparsed.append(data);
    }
};

}  // namespace util
}  // namespace cocaine
