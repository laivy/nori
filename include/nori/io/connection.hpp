#pragma once
#include <chrono>
#include <cstddef>

#include <nori/core/handle.hpp>
#include <nori/io/awaitable.hpp>
#include <nori/io/endpoint.hpp>
#include <nori/io/error_code.hpp>
#include <nori/io/packet.hpp>

namespace nori::io::detail
{
struct connection_tag;

class connection_deleter
{
public:
    void operator()(core::basic_handle<connection_tag> handle) const noexcept;
};
} // namespace nori::io::detail

namespace nori::io
{
using connection_handle = core::basic_handle<detail::connection_tag>;
using unique_connection_handle = core::unique_handle<connection_handle, detail::connection_deleter>;

struct connection_options
{
    std::size_t max_packet_size;
    std::size_t max_pending_send_bytes;
    bool no_delay;
    std::chrono::steady_clock::duration send_timeout;
    std::chrono::steady_clock::duration receive_timeout;
};

[[nodiscard]] awaitable<result<void>> async_send(connection_handle target, opacket packet);
[[nodiscard]] awaitable<result<ipacket>> async_receive(connection_handle target);
void close(connection_handle target);
[[nodiscard]] endpoint remote_endpoint(connection_handle target);
void destroy(connection_handle target);
} // namespace nori::io
