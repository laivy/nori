#pragma once
#include <expected>
#include <string_view>

namespace nori::resource
{
enum class error_code
{
    io_failure,
    invalid_format,
    name_conflict,
    invalid_relationship
};

template <class T>
using result = std::expected<T, error_code>;

constexpr std::string_view to_string(error_code error) noexcept
{
    switch (error)
    {
    case error_code::io_failure:
        return "resource I/O failure";
    case error_code::invalid_format:
        return "invalid resource format";
    case error_code::name_conflict:
        return "resource name conflict";
    case error_code::invalid_relationship:
        return "invalid resource relationship";
    }
    return "unknown resource error";
}
} // namespace nori::resource
