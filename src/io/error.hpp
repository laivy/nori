#pragma once
#include <asio/error_code.hpp>

#include <nori/io/error_code.hpp>

namespace nori::io
{
[[nodiscard]] error_code to_error_code(const asio::error_code& ec, error_code fallback = error_code::io_failure);
} // namespace nori::io
