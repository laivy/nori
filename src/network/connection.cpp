#include <array>
#include <bit>
#include <expected>
#include <format>
#include <limits>
#include <utility>
#include <vector>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>
#include "connection.hpp"

namespace nori::network
{
connection::connection(specification spec) :
    id_{spec.id},
    socket_{std::move(spec.socket)},
    strand_{socket_.get_executor()},
    max_receive_packet_size_{spec.max_receive_packet_size},
    read_timer_{socket_.get_executor()},
    read_timeout_{spec.read_timeout},
    read_timed_out_{false}
{
}

connection::~connection()
{
    close();
}

boost::asio::awaitable<boost::system::error_code> connection::async_send_packet(out_packet packet)
{
    const std::span<const std::byte> payload{packet.bytes()};
    if (payload.size() > std::numeric_limits<packet_size_type>::max())
    {
        co_return boost::asio::error::message_size;
    }

    packet_size_type packet_size{static_cast<packet_size_type>(payload.size())};
    if constexpr (std::endian::native == std::endian::little)
    {
        packet_size = std::byteswap(packet_size);
    }

    const std::array<boost::asio::const_buffer, 2> buffers{
        boost::asio::buffer(&packet_size, sizeof(packet_size)),
        boost::asio::buffer(payload),
    };

    boost::system::error_code ec;
    co_await boost::asio::async_write(
        socket_,
        buffers,
        boost::asio::bind_executor(strand_, boost::asio::redirect_error(boost::asio::use_awaitable, ec))
    );
    co_return ec;
}

template <typename MutableBuffers>
boost::asio::awaitable<boost::system::error_code> connection::async_read_with_timeout(const MutableBuffers& buffers)
{
    read_timed_out_ = false;
    read_timer_.expires_after(read_timeout_);
    read_timer_.async_wait(
        boost::asio::bind_executor(
            strand_,
            [this](const boost::system::error_code& ec)
            {
                if (ec)
                {
                    return;
                }
                read_timed_out_ = true;
                close();
            }
        )
    );

    boost::system::error_code ec;
    co_await boost::asio::async_read(
        socket_,
        buffers,
        boost::asio::bind_executor(strand_, boost::asio::redirect_error(boost::asio::use_awaitable, ec))
    );
    read_timer_.cancel();

    if (read_timed_out_)
    {
        co_return boost::asio::error::timed_out;
    }
    co_return ec;
}

boost::asio::awaitable<std::expected<in_packet, boost::system::error_code>> connection::async_receive_packet()
{
    packet_size_type packet_size{};
    boost::system::error_code ec{
        co_await async_read_with_timeout(boost::asio::buffer(&packet_size, sizeof(packet_size)))
    };
    if (ec)
    {
        co_return std::unexpected{ec};
    }

    if constexpr (std::endian::native == std::endian::little)
    {
        packet_size = std::byteswap(packet_size);
    }

    if (packet_size < sizeof(packet_type) || packet_size > max_receive_packet_size_)
    {
        co_return std::unexpected{boost::asio::error::message_size};
    }

    packet_type type{};
    std::vector<std::byte> body(packet_size - sizeof(packet_type));
    const std::array<boost::asio::mutable_buffer, 2> buffers{
        boost::asio::buffer(&type, sizeof(type)),
        boost::asio::buffer(body),
    };
    ec = co_await async_read_with_timeout(buffers);
    if (ec)
    {
        co_return std::unexpected{ec};
    }

    if constexpr (std::endian::native == std::endian::little)
    {
        type = std::byteswap(type);
    }

    co_return in_packet{type, std::move(body)};
}

void connection::close()
{
    boost::system::error_code ec;
    std::ignore = socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    std::ignore = socket_.close(ec);
}

client_id connection::id() const
{
    return id_;
}

std::string connection::remote_endpoint() const
{
    boost::system::error_code ec;
    const boost::asio::ip::tcp::endpoint endpoint{socket_.remote_endpoint(ec)};
    if (ec)
    {
        return {};
    }
    return std::format("{}:{}", endpoint.address().to_string(), endpoint.port());
}

boost::asio::ip::tcp::socket& connection::socket()
{
    return socket_;
}

bool connection::is_open() const
{
    return socket_.is_open();
}
} // namespace nori::network
