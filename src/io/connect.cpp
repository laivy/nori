#include <chrono>
#include <expected>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <tuple>
#include <utility>

#include <asio/any_io_executor.hpp>
#include <asio/connect.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/steady_timer.hpp>

#include <nori/io/connect.hpp>
#include <nori/io/post.hpp>
#include <nori/io/strand.hpp>

#include "connection.hpp"
#include "context.hpp"
#include "error.hpp"
#include "io_operation.hpp"
#include "registry.hpp"
#include "strand.hpp"

namespace
{
struct resolve_result
{
    asio::error_code ec;
    asio::ip::tcp::resolver::results_type endpoints;
};

class connect_state
{
public:
    connect_state(asio::any_io_executor executor, std::chrono::steady_clock::duration timeout);

    void cancel(nori::io::error_code reason);

    asio::ip::tcp::resolver resolver;
    asio::ip::tcp::socket socket;
    asio::steady_timer steady_timer;
    std::optional<nori::io::error_code> stop_reason;
};

connect_state::connect_state(asio::any_io_executor executor, std::chrono::steady_clock::duration timeout) :
    resolver{executor},
    socket{executor},
    steady_timer{std::move(executor), std::chrono::steady_clock::now() + timeout}
{
}

void connect_state::cancel(nori::io::error_code reason)
{
    if (stop_reason)
    {
        return;
    }
    stop_reason = reason;
    resolver.cancel();
    asio::error_code ignored;
    socket.cancel(ignored);
}
} // namespace

namespace nori::io
{
awaitable<result<unique_connection_handle>> async_connect(context_handle ctx, connect_options options, std::stop_token stop)
{
    if (stop.stop_requested())
    {
        co_return std::unexpected{error_code::cancelled};
    }
    auto executor{std::make_shared<strand>(acquire(ctx))};
    if (!co_await schedule_on(executor))
    {
        co_return std::unexpected{error_code::invalid_state};
    }

    const auto state{std::make_shared<connect_state>(executor->native_executor(), options.timeout)};
    const std::weak_ptr<connect_state> state_ref{state};
    state->steady_timer.async_wait([state_ref](const asio::error_code& ec)
    {
        const std::shared_ptr<connect_state> state{state_ref.lock()};
        if (ec || !state)
        {
            return;
        }

        state->cancel(error_code::timed_out);
    });

    const std::weak_ptr<detail::executor> executor_ref{executor};
    const std::stop_callback cancellation{stop, [state_ref, executor_ref]()
    {
        if (const auto executor{executor_ref.lock()})
        {
            std::ignore = executor->post([state_ref]()
            {
                if (const auto state{state_ref.lock()})
                {
                    state->cancel(error_code::cancelled);
                }
            });
        }
    }};

    if (stop.stop_requested())
    {
        co_return std::unexpected{error_code::cancelled};
    }

    const resolve_result resolved{co_await async_io_operation<resolve_result>([state, address = options.remote.address, port = options.remote.port](auto completion)
    {
        state->resolver.async_resolve(address, std::to_string(port), [completion = std::move(completion)](const asio::error_code& ec, asio::ip::tcp::resolver::results_type endpoints) mutable
        {
            std::move(completion)(resolve_result{.ec = ec, .endpoints = std::move(endpoints)});
        });
    })};
    if (state->stop_reason || stop.stop_requested() || resolved.ec)
    {
        co_return std::unexpected{state->stop_reason.value_or(stop.stop_requested() ? error_code::cancelled : to_error_code(resolved.ec, error_code::resolve_failed))};
    }

    const asio::error_code connected{co_await async_io_operation<asio::error_code>([state, &resolved](auto completion)
    {
        asio::async_connect(state->socket, resolved.endpoints, [completion = std::move(completion)](const asio::error_code& ec, const asio::ip::tcp::endpoint&) mutable
        {
            std::move(completion)(ec);
        });
    })};

    if (state->stop_reason || stop.stop_requested() || connected)
    {
        co_return std::unexpected{state->stop_reason.value_or(stop.stop_requested() ? error_code::cancelled : to_error_code(connected, error_code::connection_failed))};
    }

    co_return create_connection(std::move(executor), std::move(state->socket), options.connection);
}
} // namespace nori::io
