#pragma once
#include <expected>
#include <string_view>

namespace nori::io
{
enum class error_code
{
    invalid_state,
    invalid_address,
    address_in_use,
    resolve_failed,
    connection_failed,
    not_connected,
    timed_out,
    cancelled,
    disconnected,
    message_too_large,
    send_queue_full,
    io_failure
};

template <typename T>
using result = std::expected<T, error_code>;

constexpr std::string_view to_string(error_code code) noexcept
{
    switch (code)
    {
    case error_code::invalid_state:
    {
        return "invalid state";
    }
    case error_code::invalid_address:
    {
        return "invalid address";
    }
    case error_code::address_in_use:
    {
        return "address in use";
    }
    case error_code::resolve_failed:
    {
        return "resolve failed";
    }
    case error_code::connection_failed:
    {
        return "connection failed";
    }
    case error_code::not_connected:
    {
        return "not connected";
    }
    case error_code::timed_out:
    {
        return "timed out";
    }
    case error_code::cancelled:
    {
        return "cancelled";
    }
    case error_code::disconnected:
    {
        return "disconnected";
    }
    case error_code::message_too_large:
    {
        return "message too large";
    }
    case error_code::send_queue_full:
    {
        return "send queue full";
    }
    case error_code::io_failure:
    {
        return "I/O failure";
    }
    default:
        return "unknown error";
    }
}
} // namespace nori::io
