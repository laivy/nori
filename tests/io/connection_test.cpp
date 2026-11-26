#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <future>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>
#include <gtest/gtest.h>

#include <nori/io.hpp>

#include "io_test_helpers.hpp"

namespace
{
constexpr nori::io::connection_options make_connection_options()
{
    return nori::io::connection_options{
        .max_packet_size = 1024,
        .max_pending_send_bytes = 1024 * 1024,
        .no_delay = true,
        .send_timeout = std::chrono::seconds{2},
        .receive_timeout = std::chrono::seconds{2},
    };
}

template <typename T>
nori::io::awaitable<void> async_complete(nori::io::awaitable<T> operation, std::promise<T>& promise)
{
    if constexpr (std::is_void_v<T>)
    {
        co_await std::move(operation);
        promise.set_value();
    }
    else
    {
        promise.set_value(co_await std::move(operation));
    }
}

nori::io::awaitable<void> async_complete(nori::io::awaitable<nori::io::result<nori::io::unique_connection_handle>> operation, std::promise<nori::io::result<nori::io::connection_handle>>& promise)
{
    auto result{co_await std::move(operation)};
    if (result)
    {
        promise.set_value(result->release());
    }
    else
    {
        promise.set_value(std::unexpected{result.error()});
    }
}

using received_values_result = std::optional<std::array<std::int32_t, 2>>;

nori::io::awaitable<void> send_with_resource(nori::io::connection_handle target, nori::io::opacket packet, std::shared_ptr<int>)
{
    std::ignore = co_await nori::io::async_send(target, std::move(packet));
}

nori::io::awaitable<void> join_released_send(nori::io::task_handle task, std::weak_ptr<int> lifetime, std::promise<bool>& promise)
{
    co_await nori::io::async_join(task);
    promise.set_value(lifetime.expired());
}

nori::io::awaitable<void> async_receive_values(nori::io::connection_handle connection, std::promise<received_values_result>& promise)
{
    std::array<std::int32_t, 2> values{};
    for (std::size_t i{0}; i < values.size(); ++i)
    {
        auto packet{co_await nori::io::async_receive(connection)};
        if (!packet)
        {
            promise.set_value(std::nullopt);
            co_return;
        }

        const auto value{packet->read<std::int32_t>()};
        if (!value)
        {
            promise.set_value(std::nullopt);
            co_return;
        }
        values[i] = *value;
    }

    promise.set_value(values);
}

struct connection_pair
{
    nori::io::connection_handle client;
    nori::io::connection_handle server;
};

nori::io::result<connection_pair> make_connection_pair(nori::io::context_handle context, nori::io::acceptor_handle acceptor, nori::io::connection_options client_options, std::stop_token stop = {})
{
    std::promise<nori::io::result<nori::io::connection_handle>> accepted_promise;
    std::promise<nori::io::result<nori::io::connection_handle>> connected_promise;
    auto accepted_future{accepted_promise.get_future()};
    auto connected_future{connected_promise.get_future()};

    if (!nori::io::co_spawn(context, async_complete(nori::io::async_accept(acceptor), accepted_promise)) ||
        !nori::io::co_spawn(context, async_complete(nori::io::async_connect(context, {
                                                                                         .remote = {.address = "127.0.0.1", .port = nori::io::port(acceptor)},
                                                                                         .timeout = std::chrono::seconds{2},
                                                                                         .connection = client_options,
                                                                                     },
                                                        stop),
                                         connected_promise)))
    {
        return std::unexpected{nori::io::error_code::invalid_state};
    }

    if (!wait_until_ready(accepted_future) || !wait_until_ready(connected_future))
    {
        return std::unexpected{nori::io::error_code::timed_out};
    }

    auto accepted{accepted_future.get()};
    auto connected{connected_future.get()};
    if (!accepted)
    {
        return std::unexpected{accepted.error()};
    }
    if (!connected)
    {
        return std::unexpected{connected.error()};
    }
    return connection_pair{.client = std::move(*connected), .server = std::move(*accepted)};
}

nori::io::result<connection_pair> make_connection_pair(nori::io::context_handle context, nori::io::acceptor_handle acceptor)
{
    return make_connection_pair(context, acceptor, make_connection_options());
}

nori::io::result<nori::io::connection_handle> make_raw_connection(nori::io::context_handle context, asio::ip::tcp::socket& peer, nori::io::connection_options options = make_connection_options())
{
    auto acceptor{nori::io::create_acceptor(context, {
                                                         .local = {.address = "127.0.0.1"},
                                                         .connection = options,
                                                     })};
    if (!acceptor)
    {
        return std::unexpected{acceptor.error()};
    }
    std::promise<nori::io::result<nori::io::connection_handle>> promise;
    auto future{promise.get_future()};
    if (!nori::io::co_spawn(context, async_complete(nori::io::async_accept(acceptor->get()), promise)))
    {
        return std::unexpected{nori::io::error_code::invalid_state};
    }
    asio::error_code error;
    peer.connect(asio::ip::tcp::endpoint{asio::ip::address_v4::loopback(), nori::io::port(acceptor->get())}, error);
    if (error)
    {
        return std::unexpected{nori::io::error_code::connection_failed};
    }
    if (!wait_until_ready(future))
    {
        return std::unexpected{nori::io::error_code::timed_out};
    }
    return future.get();
}
} // namespace

TEST(connection_test, packet_exchange)
{
    const context_lifetime context_scope;
    const auto context{context_scope.handle()};
    context_runner runner{context, 2};
    auto acceptor_result{nori::io::create_acceptor(context, {
                                                                .local = {.address = "127.0.0.1"},
                                                                .connection = make_connection_options(),
                                                            })};
    ASSERT_TRUE(acceptor_result);
    nori::io::acceptor_handle acceptor{acceptor_result->get()};
    ASSERT_GT(nori::io::port(acceptor), 0);
    std::stop_source stop;
    auto connections{make_connection_pair(context, acceptor, make_connection_options(), stop.get_token())};
    ASSERT_TRUE(connections);
    auto [client, server]{std::move(*connections)};
    stop.request_stop();

    std::promise<nori::io::result<nori::io::ipacket>> received_promise;
    auto received_future{received_promise.get_future()};
    auto receive_operation{nori::io::async_receive(server)};
    nori::io::connection_handle moved_server{std::move(server)};
    ASSERT_TRUE(nori::io::co_spawn(context, async_complete(std::move(receive_operation), received_promise)));

    nori::io::opacket packet;
    packet.write(test_packet_type::value);
    packet.write(std::uint32_t{42});
    const std::array payload{std::byte{0}, std::byte{0xFF}, std::byte{0x42}};
    ASSERT_TRUE(packet.write(payload));
    ASSERT_TRUE(packet.write(std::span<const std::byte>{}));
    ASSERT_TRUE(packet.write(std::string_view{}));
    std::promise<nori::io::result<void>> sent_promise;
    auto sent_future{sent_promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(context, async_complete(nori::io::async_send(client, std::move(packet)), sent_promise)));

    ASSERT_TRUE(wait_until_ready(sent_future));
    ASSERT_TRUE(wait_until_ready(received_future));
    const auto sent{sent_future.get()};
    auto received{received_future.get()};
    ASSERT_TRUE(sent);
    ASSERT_TRUE(received) << nori::io::to_string(received.error());
    const auto type{received->read<test_packet_type>()};
    ASSERT_TRUE(type);
    EXPECT_EQ(*type, test_packet_type::value);
    const auto value{received->read<std::uint32_t>()};
    ASSERT_TRUE(value);
    EXPECT_EQ(*value, 42);
    const auto bytes{received->read_bytes()};
    ASSERT_TRUE(bytes);
    const auto empty_bytes{received->read_bytes()};
    ASSERT_TRUE(empty_bytes);
    EXPECT_TRUE(empty_bytes->empty());
    const auto empty_text{received->read<std::string>()};
    ASSERT_TRUE(empty_text);
    EXPECT_TRUE(empty_text->empty());
    EXPECT_EQ(received->remaining(), 0);
    received = std::unexpected{nori::io::error_code::disconnected};
    EXPECT_TRUE(std::ranges::equal(*bytes, payload));
    const nori::io::endpoint client_endpoint{nori::io::remote_endpoint(client)};
    const nori::io::endpoint server_endpoint{nori::io::remote_endpoint(moved_server)};
    EXPECT_EQ(client_endpoint.address, "127.0.0.1");
    EXPECT_EQ(client_endpoint.port, nori::io::port(acceptor));
    EXPECT_EQ(server_endpoint.address, "127.0.0.1");
    EXPECT_GT(server_endpoint.port, 0);
}

TEST(connection_test, cancelled_connect)
{
    const context_lifetime context_scope;
    const auto context{context_scope.handle()};
    context_runner runner{context, 1};
    std::stop_source stop;
    stop.request_stop();
    std::promise<nori::io::result<nori::io::connection_handle>> promise;
    auto future{promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(context, async_complete(nori::io::async_connect(context, {
                                                                                                .remote = {.address = "127.0.0.1", .port = 1},
                                                                                                .timeout = std::chrono::seconds{2},
                                                                                                .connection = make_connection_options(),
                                                                                            },
                                                               stop.get_token()),
                                                promise)));

    ASSERT_TRUE(wait_until_ready(future));
    const auto result{future.get()};
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), nori::io::error_code::cancelled);
}

TEST(connection_test, copied_packets_keep_independent_contents)
{
    const context_lifetime context_scope;
    const auto context{context_scope.handle()};
    context_runner runner{context, 2};
    asio::io_context peer_context;
    asio::ip::tcp::socket peer{peer_context};
    auto server{make_raw_connection(context, peer)};
    ASSERT_TRUE(server);
    nori::io::opacket original;
    original.write(std::uint8_t{0x10});
    nori::io::opacket copy{original};
    original.write(std::uint8_t{0x20});
    copy.write(std::uint8_t{0x30});

    auto first_send{nori::io::async_send(*server, original)};
    original.write(std::uint8_t{0x40});
    std::promise<nori::io::result<void>> first_promise;
    auto first_future{first_promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(context, async_complete(std::move(first_send), first_promise)));
    ASSERT_TRUE(wait_until_ready(first_future));
    ASSERT_TRUE(first_future.get());

    nori::io::opacket assigned;
    assigned.write(std::uint8_t{0xFF});
    assigned = copy;
    copy.write(std::uint8_t{0x50});
    assigned.write(std::uint8_t{0x60});

    std::promise<nori::io::result<void>> second_promise;
    auto second_future{second_promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(context, async_complete(nori::io::async_send(*server, copy), second_promise)));
    ASSERT_TRUE(wait_until_ready(second_future));
    ASSERT_TRUE(second_future.get());

    std::promise<nori::io::result<void>> third_promise;
    auto third_future{third_promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(context, async_complete(nori::io::async_send(*server, assigned), third_promise)));
    ASSERT_TRUE(wait_until_ready(third_future));
    ASSERT_TRUE(third_future.get());

    std::array<std::uint8_t, 20> wire{};
    std::size_t received{0};
    asio::error_code read_error;
    asio::async_read(peer, asio::buffer(wire), [&](const asio::error_code& error, std::size_t size)
    {
        read_error = error;
        received = size;
    });
    peer_context.run_for(std::chrono::seconds{1});
    ASSERT_EQ(received, wire.size());
    ASSERT_FALSE(read_error);
    const std::array<std::uint8_t, 20> expected{0, 0, 0, 2, 0x10, 0x20, 0, 0, 0, 3, 0x10, 0x30, 0x50, 0, 0, 0, 3, 0x10, 0x30, 0x60};
    EXPECT_EQ(wire, expected);
}

TEST(connection_test, cancel_connect_after_start)
{
    const context_lifetime context_scope;
    const auto context{context_scope.handle()};
    std::stop_source stop;
    std::promise<nori::io::result<nori::io::connection_handle>> promise;
    auto future{promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(context, async_complete(nori::io::async_connect(context, {
                                                                                                .remote = {.address = "localhost", .port = 1},
                                                                                                .timeout = std::chrono::seconds{2},
                                                                                                .connection = make_connection_options(),
                                                                                            },
                                                               stop.get_token()),
                                                promise)));
    ASSERT_TRUE(nori::io::post(context, [&]()
    {
        // 연결 strand의 초기화 뒤 취소 요청을 실행하도록 한 번 더 제출
        EXPECT_TRUE(nori::io::post(context, [&]()
        {
            EXPECT_EQ(future.wait_for(std::chrono::seconds{0}), std::future_status::timeout);
            stop.request_stop();
        }));
    }));
    context_runner runner{context, 1};
    ASSERT_TRUE(wait_until_ready(future));
    const auto result{future.get()};
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), nori::io::error_code::cancelled);
}

TEST(connection_test, receive_cancellation)
{
    const context_lifetime context_scope;
    const auto context{context_scope.handle()};
    context_runner runner{context, 2};
    auto acceptor_result{nori::io::create_acceptor(context, {
                                                                .local = {.address = "127.0.0.1"},
                                                                .connection = make_connection_options(),
                                                            })};
    ASSERT_TRUE(acceptor_result);
    nori::io::acceptor_handle acceptor{acceptor_result->get()};
    auto connections{make_connection_pair(context, acceptor)};
    ASSERT_TRUE(connections);
    auto [client, server]{std::move(*connections)};

    std::promise<nori::io::result<nori::io::ipacket>> promise;
    auto future{promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(context, async_complete(nori::io::async_receive(server), promise)));
    nori::io::close(client);

    ASSERT_TRUE(wait_until_ready(future));
    const auto received{future.get()};
    ASSERT_FALSE(received);
    EXPECT_EQ(received.error(), nori::io::error_code::disconnected);
}

TEST(connection_test, receive_timeout)
{
    const context_lifetime context_scope;
    const auto context{context_scope.handle()};
    context_runner runner{context, 2};
    auto connection_options{make_connection_options()};
    connection_options.receive_timeout = std::chrono::milliseconds{50};
    auto acceptor_result{nori::io::create_acceptor(context, {
                                                                .local = {.address = "127.0.0.1"},
                                                                .connection = connection_options,
                                                            })};
    ASSERT_TRUE(acceptor_result);
    nori::io::acceptor_handle acceptor{acceptor_result->get()};
    auto connections{make_connection_pair(context, acceptor)};
    ASSERT_TRUE(connections);
    auto [_, server]{std::move(*connections)};

    std::promise<nori::io::result<nori::io::ipacket>> promise;
    auto future{promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(context, async_complete(nori::io::async_receive(server), promise)));

    ASSERT_TRUE(wait_until_ready(future));
    const auto received{future.get()};
    ASSERT_FALSE(received);
    EXPECT_EQ(received.error(), nori::io::error_code::timed_out);
}

TEST(connection_test, oversized_packet)
{
    const context_lifetime context_scope;
    const auto context{context_scope.handle()};
    context_runner runner{context, 2};
    auto connection_options{make_connection_options()};
    connection_options.max_packet_size = 8;
    auto acceptor_result{nori::io::create_acceptor(context, {
                                                                .local = {.address = "127.0.0.1"},
                                                                .connection = connection_options,
                                                            })};
    ASSERT_TRUE(acceptor_result);
    nori::io::acceptor_handle acceptor{acceptor_result->get()};
    auto connections{make_connection_pair(context, acceptor)};
    ASSERT_TRUE(connections);
    auto [client, server]{std::move(*connections)};

    std::promise<nori::io::result<nori::io::ipacket>> received_promise;
    auto received_future{received_promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(context, async_complete(nori::io::async_receive(server), received_promise)));

    nori::io::opacket packet;
    ASSERT_TRUE(packet.write(std::string{"payload"}));
    std::promise<nori::io::result<void>> sent_promise;
    auto sent_future{sent_promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(context, async_complete(nori::io::async_send(client, std::move(packet)), sent_promise)));

    ASSERT_TRUE(wait_until_ready(sent_future));
    EXPECT_TRUE(sent_future.get());
    ASSERT_TRUE(wait_until_ready(received_future));
    const auto received{received_future.get()};
    ASSERT_FALSE(received);
    EXPECT_EQ(received.error(), nori::io::error_code::message_too_large);
}

TEST(connection_test, send_queue)
{
    const context_lifetime context_scope;
    const auto context{context_scope.handle()};
    context_runner runner{context, 1};
    auto acceptor_result{nori::io::create_acceptor(context, {
                                                                .local = {.address = "127.0.0.1"},
                                                                .connection = make_connection_options(),
                                                            })};
    ASSERT_TRUE(acceptor_result);
    nori::io::acceptor_handle acceptor{acceptor_result->get()};
    auto client_options{make_connection_options()};
    client_options.max_pending_send_bytes = 4;
    auto connections{make_connection_pair(context, acceptor, client_options)};
    ASSERT_TRUE(connections);
    auto [client, _]{std::move(*connections)};

    nori::io::opacket first_packet;
    nori::io::opacket second_packet;
    std::promise<nori::io::result<void>> first_promise;
    std::promise<nori::io::result<void>> second_promise;
    auto first_future{first_promise.get_future()};
    auto second_future{second_promise.get_future()};

    ASSERT_TRUE(nori::io::co_spawn(context, async_complete(nori::io::async_send(client, std::move(first_packet)), first_promise)));
    ASSERT_TRUE(nori::io::co_spawn(context, async_complete(nori::io::async_send(client, std::move(second_packet)), second_promise)));

    ASSERT_TRUE(wait_until_ready(second_future));
    const auto second{second_future.get()};
    ASSERT_FALSE(second);
    EXPECT_EQ(second.error(), nori::io::error_code::send_queue_full);

    ASSERT_TRUE(wait_until_ready(first_future));
    EXPECT_TRUE(first_future.get());
}

TEST(connection_test, send_order)
{
    const context_lifetime context_scope;
    const auto context{context_scope.handle()};
    context_runner runner{context, 1};
    auto acceptor_result{nori::io::create_acceptor(context, {
                                                                .local = {.address = "127.0.0.1"},
                                                                .connection = make_connection_options(),
                                                            })};
    ASSERT_TRUE(acceptor_result);
    nori::io::acceptor_handle acceptor{acceptor_result->get()};
    auto connections{make_connection_pair(context, acceptor)};
    ASSERT_TRUE(connections);
    auto [client, server]{std::move(*connections)};

    std::promise<received_values_result> received_promise;
    auto received_future{received_promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(context, async_receive_values(server, received_promise)));

    nori::io::opacket first_packet;
    nori::io::opacket second_packet;
    first_packet.write(std::int32_t{1});
    second_packet.write(std::int32_t{2});
    std::promise<nori::io::result<void>> first_promise;
    std::promise<nori::io::result<void>> second_promise;
    auto first_future{first_promise.get_future()};
    auto second_future{second_promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(context, async_complete(nori::io::async_send(client, std::move(first_packet)), first_promise)));
    ASSERT_TRUE(nori::io::co_spawn(context, async_complete(nori::io::async_send(client, std::move(second_packet)), second_promise)));

    ASSERT_TRUE(wait_until_ready(first_future));
    ASSERT_TRUE(wait_until_ready(second_future));
    EXPECT_TRUE(first_future.get());
    EXPECT_TRUE(second_future.get());
    ASSERT_TRUE(wait_until_ready(received_future));
    const auto received{received_future.get()};
    ASSERT_TRUE(received);
    EXPECT_EQ((*received)[0], 1);
    EXPECT_EQ((*received)[1], 2);
}

TEST(connection_test, fragmented_and_coalesced_packets)
{
    const context_lifetime context_scope;
    const auto context{context_scope.handle()};
    context_runner runner{context, 2};
    asio::io_context peer_context;
    asio::ip::tcp::socket peer{peer_context};
    auto server{make_raw_connection(context, peer)};
    ASSERT_TRUE(server);
    std::promise<received_values_result> promise;
    auto future{promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(context, async_receive_values(*server, promise)));

    const std::array<std::uint8_t, 16> wire{0, 0, 0, 4, 0, 0, 0, 42, 0, 0, 0, 4, 0, 0, 0, 43};
    asio::write(peer, asio::buffer(wire.data(), 3));
    EXPECT_EQ(future.wait_for(std::chrono::milliseconds{10}), std::future_status::timeout);
    asio::write(peer, asio::buffer(wire.data() + 3, wire.size() - 3));
    ASSERT_TRUE(wait_until_ready(future));
    const auto values{future.get()};
    ASSERT_TRUE(values);
    EXPECT_EQ((*values)[0], 42);
    EXPECT_EQ((*values)[1], 43);
}

TEST(connection_test, empty_body_keeps_connection_usable)
{
    const context_lifetime context_scope;
    const auto context{context_scope.handle()};
    context_runner runner{context, 2};
    asio::io_context peer_context;
    asio::ip::tcp::socket peer{peer_context};
    auto options{make_connection_options()};
    options.max_packet_size = 1;
    auto server{make_raw_connection(context, peer, options)};
    ASSERT_TRUE(server);
    std::promise<nori::io::result<nori::io::ipacket>> empty_promise;
    auto empty_future{empty_promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(context, async_complete(nori::io::async_receive(*server), empty_promise)));
    const std::array<std::uint8_t, 4> empty_wire{0, 0, 0, 0};
    asio::write(peer, asio::buffer(empty_wire));
    ASSERT_TRUE(wait_until_ready(empty_future));
    auto empty{empty_future.get()};
    ASSERT_TRUE(empty);
    EXPECT_EQ(empty->remaining(), 0);
    const auto missing{empty->read<std::uint8_t>()};
    EXPECT_EQ(missing, std::nullopt);

    std::promise<nori::io::result<nori::io::ipacket>> next_promise;
    auto next_future{next_promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(context, async_complete(nori::io::async_receive(*server), next_promise)));
    const std::array<std::uint8_t, 5> next_wire{0, 0, 0, 1, 42};
    asio::write(peer, asio::buffer(next_wire));
    ASSERT_TRUE(wait_until_ready(next_future));
    auto next{next_future.get()};
    ASSERT_TRUE(next);
    const auto value{next->read<std::uint8_t>()};
    ASSERT_TRUE(value);
    EXPECT_EQ(*value, 42);
    EXPECT_EQ(next->remaining(), 0);
}

TEST(connection_test, oversized_frame_length)
{
    const context_lifetime context_scope;
    const auto context{context_scope.handle()};
    context_runner runner{context, 2};
    asio::io_context peer_context;
    asio::ip::tcp::socket peer{peer_context};
    auto server{make_raw_connection(context, peer)};
    ASSERT_TRUE(server);
    std::promise<nori::io::result<nori::io::ipacket>> promise;
    auto future{promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(context, async_complete(nori::io::async_receive(*server), promise)));
    const std::array<std::uint8_t, 4> header{0xFF, 0xFF, 0xFF, 0xFF};
    asio::write(peer, asio::buffer(header));
    ASSERT_TRUE(wait_until_ready(future));
    const auto result{future.get()};
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), nori::io::error_code::message_too_large);
}

TEST(connection_test, wire_contains_only_body_length_and_body)
{
    enum class message_type : std::uint8_t
    {
        value = 0x7F
    };

    const context_lifetime context_scope;
    const auto context{context_scope.handle()};
    context_runner runner{context, 2};
    asio::io_context peer_context;
    asio::ip::tcp::socket peer{peer_context};
    auto options{make_connection_options()};
    options.max_packet_size = 5;
    options.max_pending_send_bytes = 9;
    auto server{make_raw_connection(context, peer, options)};
    ASSERT_TRUE(server);
    nori::io::opacket packet;
    packet.write(message_type::value);
    packet.write(std::uint32_t{0x01020304});
    std::promise<nori::io::result<void>> sent_promise;
    auto sent_future{sent_promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(context, async_complete(nori::io::async_send(*server, std::move(packet)), sent_promise)));
    ASSERT_TRUE(wait_until_ready(sent_future));
    ASSERT_TRUE(sent_future.get());

    std::promise<nori::io::result<void>> empty_promise;
    auto empty_future{empty_promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(context, async_complete(nori::io::async_send(*server, nori::io::opacket{}), empty_promise)));
    ASSERT_TRUE(wait_until_ready(empty_future));
    ASSERT_TRUE(empty_future.get());

    std::array<std::uint8_t, 13> wire{};
    std::size_t received{0};
    asio::error_code read_error;
    asio::async_read(peer, asio::buffer(wire), [&](const asio::error_code& error, std::size_t size)
    {
        read_error = error;
        received = size;
    });
    peer_context.run_for(std::chrono::seconds{1});
    ASSERT_EQ(received, wire.size());
    ASSERT_FALSE(read_error);
    const std::array<std::uint8_t, 13> expected{0, 0, 0, 5, 0x7F, 1, 2, 3, 4, 0, 0, 0, 0};
    EXPECT_EQ(wire, expected);
}

TEST(connection_test, disconnect_during_packet_body)
{
    const context_lifetime context_scope;
    const auto context{context_scope.handle()};
    context_runner runner{context, 2};
    asio::io_context peer_context;
    asio::ip::tcp::socket peer{peer_context};
    auto server{make_raw_connection(context, peer)};
    ASSERT_TRUE(server);
    std::promise<nori::io::result<nori::io::ipacket>> promise;
    auto future{promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(context, async_complete(nori::io::async_receive(*server), promise)));
    const std::array<std::uint8_t, 5> wire{0, 0, 0, 4, 0};
    asio::write(peer, asio::buffer(wire));
    peer.close();
    ASSERT_TRUE(wait_until_ready(future));
    const auto result{future.get()};
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), nori::io::error_code::disconnected);
}

TEST(connection_test, close_joins_pending_receive)
{
    const context_lifetime context_scope;
    const auto context{context_scope.handle()};
    context_runner runner{context, 2};
    asio::io_context peer_context;
    asio::ip::tcp::socket peer{peer_context};
    auto server{make_raw_connection(context, peer)};
    ASSERT_TRUE(server);
    std::promise<nori::io::result<nori::io::ipacket>> received_promise;
    auto received_future{received_promise.get_future()};
    const auto handle{nori::io::co_spawn(context, async_complete(nori::io::async_receive(*server), received_promise))};
    ASSERT_TRUE(handle);
    nori::io::close(*server);
    std::promise<void> joined_promise;
    auto joined_future{joined_promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(context, async_complete(nori::io::async_join(handle), joined_promise)));
    ASSERT_TRUE(wait_until_ready(joined_future));
    joined_future.get();
    ASSERT_TRUE(wait_until_ready(received_future));
    EXPECT_FALSE(received_future.get());
}

TEST(connection_test, malformed_packet_fields)
{
    const context_lifetime context_scope;
    const auto context{context_scope.handle()};
    context_runner runner{context, 2};
    asio::io_context peer_context;
    asio::ip::tcp::socket peer{peer_context};
    auto server{make_raw_connection(context, peer)};
    ASSERT_TRUE(server);
    std::promise<nori::io::result<nori::io::ipacket>> promise;
    auto future{promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(context, async_complete(nori::io::async_receive(*server), promise)));
    const std::array<std::uint8_t, 10> wire{0, 0, 0, 6, 2, 0, 0, 0, 5, 42};
    asio::write(peer, asio::buffer(wire));
    ASSERT_TRUE(wait_until_ready(future));
    auto packet{future.get()};
    ASSERT_TRUE(packet);
    const auto boolean{packet->read<bool>()};
    EXPECT_EQ(boolean, std::nullopt);
    const auto bytes{packet->read_bytes()};
    EXPECT_EQ(bytes, std::nullopt);
    const auto text{packet->read<std::string>()};
    EXPECT_EQ(text, std::nullopt);
}

TEST(connection_test, send_timeout_fails_queued_work)
{
    const context_lifetime context_scope;
    const auto context{context_scope.handle()};
    context_runner runner{context, 1};
    asio::io_context peer_context;
    asio::ip::tcp::socket peer{peer_context};
    auto options{make_connection_options()};
    options.max_packet_size = 32 * 1024 * 1024;
    options.max_pending_send_bytes = 64 * 1024 * 1024;
    options.send_timeout = std::chrono::milliseconds{50};
    auto server{make_raw_connection(context, peer, options)};
    ASSERT_TRUE(server);
    const std::vector<std::byte> bytes(16 * 1024 * 1024, std::byte{0x42});
    nori::io::opacket first;
    ASSERT_TRUE(first.write(bytes));
    std::promise<nori::io::result<void>> first_promise;
    auto first_future{first_promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(context, async_complete(nori::io::async_send(*server, std::move(first)), first_promise)));
    nori::io::opacket second;
    std::promise<nori::io::result<void>> second_promise;
    auto second_future{second_promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(context, async_complete(nori::io::async_send(*server, std::move(second)), second_promise)));

    ASSERT_TRUE(wait_until_ready(first_future));
    const auto first_result{first_future.get()};
    ASSERT_FALSE(first_result);
    EXPECT_EQ(first_result.error(), nori::io::error_code::timed_out);
    ASSERT_TRUE(wait_until_ready(second_future));
    const auto second_result{second_future.get()};
    ASSERT_FALSE(second_result);
    EXPECT_EQ(second_result.error(), nori::io::error_code::not_connected);
}

TEST(connection_test, destroying_connection_keeps_prepared_receive_safe)
{
    const context_lifetime context_scope;
    const auto ctx{context_scope.handle()};
    context_runner runner{ctx, 1};
    asio::io_context peer_context;
    asio::ip::tcp::socket peer{peer_context};
    const auto server{make_raw_connection(ctx, peer)};
    ASSERT_TRUE(server);
    auto operation{nori::io::async_receive(*server)};
    nori::io::destroy(*server);

    std::promise<nori::io::result<nori::io::ipacket>> promise;
    auto future{promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(ctx, async_complete(std::move(operation), promise)));
    ASSERT_TRUE(wait_until_ready(future));
    const auto result{future.get()};
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), nori::io::error_code::not_connected);
}

TEST(connection_test, accepted_connection_owner_closes_on_reset)
{
    const context_lifetime context_scope;
    const auto ctx{context_scope.handle()};
    context_runner runner{ctx, 1};
    auto listener{nori::io::create_acceptor(ctx, {.local = {.address = "127.0.0.1"}, .connection = make_connection_options()})};
    ASSERT_TRUE(listener);
    std::promise<nori::io::result<nori::io::unique_connection_handle>> promise;
    auto future{promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(ctx, async_complete(nori::io::async_accept(listener->get()), promise)));
    asio::io_context peer_context;
    asio::ip::tcp::socket peer{peer_context};
    peer.connect({asio::ip::address_v4::loopback(), nori::io::port(listener->get())});
    ASSERT_TRUE(wait_until_ready(future));
    auto connection{future.get()};
    ASSERT_TRUE(connection);
    auto operation{nori::io::async_receive(connection->get())};
    connection->reset();
    std::promise<nori::io::result<nori::io::ipacket>> receive_promise;
    auto receive_future{receive_promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(ctx, async_complete(std::move(operation), receive_promise)));
    ASSERT_TRUE(wait_until_ready(receive_future));
    const auto result{receive_future.get()};
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), nori::io::error_code::not_connected);
}

TEST(connection_test, context_destruction_closes_registered_connections)
{
    context_lifetime context_scope;
    const auto ctx{context_scope.handle()};
    asio::io_context peer_context;
    asio::ip::tcp::socket peer{peer_context};
    {
        context_runner runner{ctx, 1};
        const auto server{make_raw_connection(ctx, peer)};
        ASSERT_TRUE(server);
    }
    context_scope.reset();

    peer.non_blocking(true);
    std::array<std::byte, 1> buffer{};
    asio::error_code error;
    peer.read_some(asio::buffer(buffer), error);
    EXPECT_EQ(error, asio::error::eof);
}

TEST(connection_test, context_destruction_releases_blocked_send_before_join)
{
    context_lifetime source_scope;
    const auto source{source_scope.handle()};
    const context_lifetime observer_scope;
    const auto observer{observer_scope.handle()};
    const context_runner observer_runner{observer, 1};
    asio::io_context peer_context;
    asio::ip::tcp::socket peer{peer_context};
    auto resource{std::make_shared<int>(7)};
    const std::weak_ptr<int> lifetime{resource};
    nori::io::task_handle task{};
    {
        const context_runner source_runner{source, 1};
        auto options{make_connection_options()};
        options.max_packet_size = 32 * 1024 * 1024;
        options.max_pending_send_bytes = 64 * 1024 * 1024;
        options.send_timeout = std::chrono::hours{1};
        const auto target{make_raw_connection(source, peer, options)};
        ASSERT_TRUE(target);
        peer.set_option(asio::socket_base::receive_buffer_size{1024});
        const std::vector<std::byte> bytes(16 * 1024 * 1024, std::byte{0x42});
        nori::io::opacket packet;
        ASSERT_TRUE(packet.write(bytes));
        task = nori::io::co_spawn(source, send_with_resource(*target, std::move(packet), std::move(resource)));
        ASSERT_TRUE(task);
        std::array<std::byte, 4> header{};
        asio::error_code error;
        EXPECT_EQ(asio::read(peer, asio::buffer(header), error), header.size());
        ASSERT_FALSE(error);
    }
    EXPECT_FALSE(lifetime.expired());
    std::promise<bool> promise;
    auto future{promise.get_future()};
    ASSERT_TRUE(nori::io::co_spawn(observer, join_released_send(task, lifetime, promise)));
    source_scope.reset();
    ASSERT_TRUE(wait_until_ready(future));
    EXPECT_TRUE(future.get());
}
