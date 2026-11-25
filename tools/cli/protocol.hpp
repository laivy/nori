#pragma once
#include <expected>

#include <nlohmann/json.hpp>

namespace nori::cli
{
enum class error_code
{
    invalid_request,
    unknown_method,
    invalid_argument,
    not_found,
    already_exists,
    type_mismatch,
    io_error,
    invalid_format,
    unsupported_format,
    unsupported_platform,
    internal_error
};

template <class T>
using result = std::expected<T, error_code>;

[[nodiscard]] result<nlohmann::ordered_json> execute(const nlohmann::json& request);
[[nodiscard]] nlohmann::ordered_json make_response(result<nlohmann::ordered_json> value);

int run();
} // namespace nori::cli
