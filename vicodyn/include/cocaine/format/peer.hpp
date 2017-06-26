#pragma once

#include "cocaine/vicodyn/peer.hpp"

#include <cocaine/format.hpp>
#include <cocaine/format/base.hpp>
#include <cocaine/format/map.hpp>

#include <iomanip>

namespace cocaine {

///TODO: move to core
template<class Clock, class Duration>
struct display<std::chrono::time_point<Clock, Duration>> {
    using value_t = std::chrono::time_point<Clock, Duration>;
    static
    auto
    apply(std::ostream& stream, const value_t& value) -> std::ostream& {
        std::time_t time = Clock::to_time_t(value);
        struct tm time_data = tm();
        gmtime_r(&time, &time_data);
        std::array<char, 128> buffer;
        strftime(buffer.data(), buffer.size(), "%F %T", &time_data);
        stream << buffer.data();
        return stream;
    }
};

template<>
struct display<vicodyn::peer_t::state_t> {
    static
    auto
    apply(std::ostream& stream, vicodyn::peer_t::state_t value) -> std::ostream& {
        switch (value) {
            case vicodyn::peer_t::state_t::disconnected:
                stream << "disconnected";
                break;
            case vicodyn::peer_t::state_t::connecting:
                stream << "connecting";
                break;
            case vicodyn::peer_t::state_t::connected:
                stream << "connected";
                break;
            case vicodyn::peer_t::state_t::freezed:
                stream << "freezed";
                break;
        }
        return stream;
    }
};

template<>
struct display<vicodyn::peer_t> {
    static
    auto
    apply(std::ostream& stream, const vicodyn::peer_t& value) -> std::ostream& {
        stream << format("{{state: {}, "
                          "uuid: {}, "
                          "local: {}, "
                          "freezed_till: {}, "
                          "last_used: {}"
                          "endpoints: {}}}",
                         value.state(), value.uuid(), value.local(), value.freezed_till(), value.last_used());
        return stream;
    }
};

template<>
struct display<vicodyn::peers_t> {
    static
    auto
    apply(std::ostream& stream, const vicodyn::peers_t& value) -> std::ostream& {
        stream << "{";
        using state_t = vicodyn::peer_t::state_t;
        for (auto state: {state_t::disconnected, state_t::connecting, state_t::connected, state_t::freezed}) {
            display<state_t>::apply(stream, state);
            stream << ": ";
            auto peer_group = value.get(state);
            display<decltype(peer_group)>::apply(stream, peer_group);
            if(state != state_t::freezed) {
                stream << ", ";
            }
        }
        return stream << "}";
    }
};

template<>
struct display_traits<vicodyn::peer_t>: public lazy_display<vicodyn::peer_t> {};

template<>
struct display_traits<vicodyn::peers_t>: public lazy_display<vicodyn::peers_t> {};

template<>
struct display_traits<vicodyn::peer_t::state_t>: public lazy_display<vicodyn::peer_t::state_t> {};


template<class Clock, class Duration>
struct display_traits<std::chrono::time_point<Clock, Duration>>:
        public lazy_display<std::chrono::time_point<Clock, Duration>>
{};

} // namespace cocaine
