#pragma once
#include <chrono>
#include <stop_token>

#include <nori/io/awaitable.hpp>
#include <nori/io/connection.hpp>
#include <nori/io/context.hpp>
#include <nori/io/endpoint.hpp>

namespace nori::io
{
struct connect_options
{
    endpoint remote;
    std::chrono::steady_clock::duration timeout;
    connection_options connection;
};

[[nodiscard]] awaitable<result<unique_connection_handle>> async_connect(context_handle ctx, connect_options options, std::stop_token stop = {});
} // namespace nori::io
