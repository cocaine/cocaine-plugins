#pragma once
#include "cocaine/vicodyn/stream.hpp"

#include <cocaine/rpc/basic_dispatch.hpp>

namespace cocaine {
namespace vicodyn {
namespace proxy {

class dispatch_t :
    public io::basic_dispatch_t,
    public std::enable_shared_from_this<dispatch_t>
{
public:
    auto root() const -> const io::graph_root_t& override;

    auto version() const -> int override;

    dispatch_t(const std::string& name, std::shared_ptr<stream_t> proxy_stream, const io::graph_node_t& _current_state);

    auto process(const io::decoder_t::message_type& incoming_message, const io::upstream_ptr_t&) const
        -> boost::optional<io::dispatch_ptr_t> override;

    auto discard(const std::error_code& ec) const -> void override;

private:
    std::shared_ptr<stream_t> proxy_stream;
    mutable std::string full_name;
    mutable const io::graph_node_t* current_state;
    mutable io::graph_root_t current_root;
};

} // namespace proxy
} // namespace vicodyn
} // namespace cocaine
