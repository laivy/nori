#include <asio/error.hpp>

#include "error.hpp"

namespace nori::io
{
error_code to_error_code(const asio::error_code& ec, error_code fallback)
{
    if (ec == asio::error::timed_out)
    {
        return error_code::timed_out;
    }
    if (ec == asio::error::connection_refused || ec == asio::error::host_unreachable)
    {
        return error_code::connection_failed;
    }
    if (ec == asio::error::address_in_use)
    {
        return error_code::address_in_use;
    }
    if (ec == asio::error::operation_aborted)
    {
        return error_code::cancelled;
    }
    if (ec == asio::error::eof || ec == asio::error::connection_reset || ec == asio::error::connection_aborted)
    {
        return error_code::disconnected;
    }
    return fallback;
}
} // namespace nori::io
