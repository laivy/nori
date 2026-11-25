#pragma once
#include <expected>
#include <string_view>

namespace nori::resource
{
enum class error_code
{
    not_found,
    invalid_path,
    type_mismatch,
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
    case error_code::not_found:
    {
        return "resource not found";
    }
    case error_code::invalid_path:
    {
        return "invalid resource path";
    }
    case error_code::type_mismatch:
    {
        return "resource type mismatch";
    }
    case error_code::io_failure:
    {
        return "resource I/O failure";
    }
    case error_code::invalid_format:
    {
        return "invalid resource format";
    }
    case error_code::name_conflict:
    {
        return "resource name conflict";
    }
    case error_code::invalid_relationship:
    {
        return "invalid resource relationship";
    }
    default:
        return "unknown resource error";
    }
}
} // namespace nori::resource
