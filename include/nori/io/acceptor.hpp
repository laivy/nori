#pragma once
#include <cstdint>

#include <nori/core/handle.hpp>
#include <nori/io/awaitable.hpp>
#include <nori/io/connection.hpp>
#include <nori/io/context.hpp>
#include <nori/io/endpoint.hpp>

namespace nori::io::detail
{
struct acceptor_tag;

class acceptor_deleter
{
public:
    void operator()(core::basic_handle<acceptor_tag> handle) const noexcept;
};
} // namespace nori::io::detail

namespace nori::io
{
using acceptor_handle = core::basic_handle<detail::acceptor_tag>;
using unique_acceptor_handle = core::unique_handle<acceptor_handle, detail::acceptor_deleter>;

struct acceptor_options
{
    endpoint local;
    connection_options connection;
};

[[nodiscard]] result<unique_acceptor_handle> create_acceptor(context_handle ctx, acceptor_options options);
[[nodiscard]] awaitable<result<unique_connection_handle>> async_accept(acceptor_handle listener);
void close(acceptor_handle listener);
[[nodiscard]] std::uint16_t port(acceptor_handle listener);
void destroy(acceptor_handle listener);
} // namespace nori::io
