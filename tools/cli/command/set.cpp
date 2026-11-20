#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include <nlohmann/json.hpp>

#include <nori/resource.hpp>

#include "command.hpp"
#include "common.hpp"

namespace
{
std::optional<nori::resource::property_type> parse_type(std::string_view input)
{
    if (input == "empty")
    {
        return nori::resource::property_type::empty;
    }
    if (input == "bool")
    {
        return nori::resource::property_type::boolean;
    }
    if (input == "int32")
    {
        return nori::resource::property_type::int32;
    }
    if (input == "int64")
    {
        return nori::resource::property_type::int64;
    }
    if (input == "float")
    {
        return nori::resource::property_type::float32;
    }
    if (input == "string")
    {
        return nori::resource::property_type::string;
    }
    return std::nullopt;
}

std::optional<std::int64_t> parse_signed_integer(const nlohmann::json& input)
{
    if (input.is_number_unsigned())
    {
        const auto parsed{input.get<std::uint64_t>()};
        if (std::in_range<std::int64_t>(parsed))
        {
            return static_cast<std::int64_t>(parsed);
        }
        return std::nullopt;
    }
    if (input.is_number_integer())
    {
        return input.get<std::int64_t>();
    }
    return std::nullopt;
}

std::optional<nori::cli::command::set_args::value_type> parse_value(nori::resource::property_type type, const nlohmann::json* input)
{
    if (type == nori::resource::property_type::empty)
    {
        return input ? std::nullopt : std::optional<nori::cli::command::set_args::value_type>{nori::resource::empty};
    }
    if (!input)
    {
        return std::nullopt;
    }

    switch (type)
    {
    case nori::resource::property_type::boolean:
    {
        if (input->is_boolean())
        {
            return input->get<bool>();
        }
        break;
    }
    case nori::resource::property_type::int32:
    {
        const auto parsed{parse_signed_integer(*input)};
        if (parsed && std::in_range<std::int32_t>(*parsed))
        {
            return static_cast<std::int32_t>(*parsed);
        }
        break;
    }
    case nori::resource::property_type::int64:
    {
        if (const auto parsed{parse_signed_integer(*input)})
        {
            return *parsed;
        }
        break;
    }
    case nori::resource::property_type::float32:
    {
        if (input->is_number())
        {
            const double parsed{input->get<double>()};
            if (std::isfinite(parsed) && parsed >= -std::numeric_limits<float>::max() && parsed <= std::numeric_limits<float>::max())
            {
                return static_cast<float>(parsed);
            }
        }
        break;
    }
    case nori::resource::property_type::string:
    {
        if (input->is_string())
        {
            return input->get_ref<const std::string&>();
        }
        break;
    }
    default:
        break;
    }
    return std::nullopt;
}
} // namespace

namespace nori::cli::command
{
std::optional<set_args> parse_set_args(const nlohmann::json& params)
{
    if (!has_valid_param_keys(params, {"path", "type"}, {"value"}))
    {
        return std::nullopt;
    }
    if (!params.at("path").is_string() || !params.at("type").is_string())
    {
        return std::nullopt;
    }

    const auto type{parse_type(params.at("type").get_ref<const std::string&>())};
    if (!type)
    {
        return std::nullopt;
    }
    const nlohmann::json* const input{params.contains("value") ? &params.at("value") : nullptr};
    const auto parsed{parse_value(*type, input)};
    if (!parsed)
    {
        return std::nullopt;
    }
    return set_args{resource::path{params.at("path").get_ref<const std::string&>()}, *type, *parsed};
}

result<nlohmann::ordered_json> set(const set_args& args)
{
    if (args.path.has_file_path() && !args.path.has_subpath())
    {
        return std::unexpected{error_code::invalid_argument};
    }
    if (is_root_path(args.path))
    {
        return std::unexpected{error_code::invalid_argument};
    }

    const auto target{create(args.path)};
    if (!target)
    {
        return std::unexpected{target.error()};
    }
    if (*target == resource::root_of(*target))
    {
        return std::unexpected{error_code::invalid_argument};
    }

    std::visit([target = *target](const auto& item)
    {
        resource::set(target, item);
    }, args.value);
    if (const result<void> res{save(*target, args.path.file_path())}; !res)
    {
        return std::unexpected{res.error()};
    }
    return nlohmann::ordered_json(nullptr);
}
} // namespace nori::cli::command
