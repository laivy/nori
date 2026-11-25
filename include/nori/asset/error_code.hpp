#pragma once
#include <expected>
#include <string>
#include <string_view>

namespace nori::asset
{
enum class error_code
{
    io_failure,
    invalid_format,
    unsupported_format,
    unsupported_platform
};

struct failure
{
    error_code code;
    std::string diagnostics;
};

template <typename T>
using result = std::expected<T, failure>;

constexpr std::string_view to_string(error_code code) noexcept
{
    switch (code)
    {
    case error_code::io_failure:
    {
        return "asset I/O failure";
    }
    case error_code::invalid_format:
    {
        return "invalid asset format";
    }
    case error_code::unsupported_format:
    {
        return "unsupported asset format";
    }
    case error_code::unsupported_platform:
    {
        return "unsupported asset platform";
    }
    default:
        break;
    }
    return "unknown asset error";
}
} // namespace nori::asset
