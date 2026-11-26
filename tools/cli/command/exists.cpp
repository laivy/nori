#include <optional>

#include <nlohmann/json.hpp>

#include <nori/resource.hpp>

#include "command.hpp"
#include "common.hpp"

namespace nori::cli::command
{
std::optional<exists_args> parse_exists_args(const nlohmann::json& params)
{
    if (!has_valid_param_keys(params, {"path"}))
    {
        return std::nullopt;
    }
    if (!params.at("path").is_string())
    {
        return std::nullopt;
    }
    return exists_args{resource::path{params.at("path").get_ref<const std::string&>()}};
}

result<nlohmann::ordered_json> exists(const exists_args& args)
{
    const auto handle{resource::try_get(args.path)};
    if (handle)
    {
        return nlohmann::ordered_json(true);
    }
    if (handle.error() == resource::error_code::not_found)
    {
        return nlohmann::ordered_json(false);
    }
    return std::unexpected{to_error_code(handle.error())};
}
} // namespace nori::cli::command
