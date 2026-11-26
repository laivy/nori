#include <cstdlib>
#include <iostream>
#include <iterator>
#include <print>
#include <string>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

#include "command/command.hpp"
#include "protocol.hpp"

#ifdef _WIN32
#include <Windows.h>
#endif

namespace
{
[[nodiscard]] std::string_view to_string(nori::cli::error_code code) noexcept
{
    switch (code)
    {
    case nori::cli::error_code::invalid_request:
    {
        return "invalid_request";
    }
    case nori::cli::error_code::unknown_method:
    {
        return "unknown_method";
    }
    case nori::cli::error_code::invalid_argument:
    {
        return "invalid_argument";
    }
    case nori::cli::error_code::not_found:
    {
        return "not_found";
    }
    case nori::cli::error_code::already_exists:
    {
        return "already_exists";
    }
    case nori::cli::error_code::type_mismatch:
    {
        return "type_mismatch";
    }
    case nori::cli::error_code::io_error:
    {
        return "io_error";
    }
    case nori::cli::error_code::invalid_format:
    {
        return "invalid_format";
    }
    case nori::cli::error_code::unsupported_format:
    {
        return "unsupported_format";
    }
    case nori::cli::error_code::unsupported_platform:
    {
        return "unsupported_platform";
    }
    case nori::cli::error_code::internal_error:
    {
        return "internal_error";
    }
    default:
        std::unreachable();
    }
}
} // namespace

namespace nori::cli
{
result<nlohmann::ordered_json> execute(const nlohmann::json& request)
{
    if (!request.is_object())
    {
        return std::unexpected{error_code::invalid_request};
    }
    if (!request.contains("method") || !request.at("method").is_string())
    {
        return std::unexpected{error_code::invalid_request};
    }
    if (!request.contains("params") || !request.at("params").is_object())
    {
        return std::unexpected{error_code::invalid_request};
    }
    for (const auto& [name, _] : request.items())
    {
        if (name != "method" && name != "params")
        {
            return std::unexpected{error_code::invalid_request};
        }
    }

    return command::dispatch(
        request.at("method").get_ref<const std::string&>(),
        request.at("params")
    );
}

nlohmann::ordered_json make_response(result<nlohmann::ordered_json> value)
{
    nlohmann::ordered_json response(nlohmann::ordered_json::object());
    if (value)
    {
        response["result"] = std::move(*value);
    }
    else
    {
        response["error"] = nlohmann::ordered_json{{"code", to_string(value.error())}};
    }
    return response;
}

int run()
{
#ifdef _WIN32
    ::SetConsoleOutputCP(CP_UTF8);
#endif

    const std::string input{std::istreambuf_iterator<char>{std::cin}, std::istreambuf_iterator<char>{}};
    const nlohmann::json request(nlohmann::json::parse(input, nullptr, false));
    if (request.is_discarded())
    {
        std::println("{}", make_response(result<nlohmann::ordered_json>{std::unexpected{error_code::invalid_request}}).dump());
        return EXIT_FAILURE;
    }

    result<nlohmann::ordered_json> res{execute(request)};
    const bool success{res.has_value()};
    std::println("{}", make_response(std::move(res)).dump());
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
} // namespace nori::cli
