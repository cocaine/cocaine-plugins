#pragma once

#include "cocaine/vicodyn/peer.hpp"

#include <cocaine/format.hpp>
#include <cocaine/format/base.hpp>
#include <cocaine/format/map.hpp>
#include <cocaine/format/vector.hpp>

#include <asio/ip/basic_endpoint.hpp>

#include <iomanip>

namespace cocaine {

template<>
struct display<vicodyn::peer_t> {
    static
    auto
    apply(std::ostream& stream, const vicodyn::peer_t& value) -> std::ostream& {
        stream << format("{{uuid: {}, endpoints: {}}}", value.uuid(), value.endpoints());
        return stream;
    }
};


template<>
struct display_traits<vicodyn::peer_t>: public lazy_display<vicodyn::peer_t> {};

} // namespace cocaine
