#include <optional>

#include <nlohmann/json.hpp>

#include <nori/resource.hpp>

#include "command.hpp"
#include "common.hpp"

namespace nori::cli::command
{
std::optional<rename_args> parse_rename_args(const nlohmann::json& params)
{
    if (!has_valid_param_keys(params, {"path", "name"}))
    {
        return std::nullopt;
    }
    if (!params.at("path").is_string() || !params.at("name").is_string())
    {
        return std::nullopt;
    }
    return rename_args{
        resource::path{params.at("path").get_ref<const std::string&>()},
        params.at("name").get_ref<const std::string&>()
    };
}

result<nlohmann::ordered_json> rename(const rename_args& args)
{
    if (args.path.has_file_path() && !args.path.has_subpath())
    {
        return std::unexpected{error_code::invalid_argument};
    }
    const auto target{resource::try_get(args.path)};
    if (!target)
    {
        return std::unexpected{to_error_code(target.error())};
    }
    if (*target == resource::root_of(*target))
    {
        return std::unexpected{error_code::invalid_argument};
    }
    if (!resource::is_valid_property_name(args.name))
    {
        return std::unexpected{error_code::invalid_argument};
    }
    if (resource::name_of(*target) == args.name)
    {
        return nlohmann::ordered_json(nullptr);
    }
    if (const auto res{prepare_edit(*target)}; !res)
    {
        return std::unexpected{res.error()};
    }
    if (const auto res{resource::rename(*target, args.name)}; !res)
    {
        return std::unexpected{to_error_code(res.error())};
    }
    if (const auto res{resource::save(resource::root_of(*target))}; !res)
    {
        return std::unexpected{to_error_code(res.error())};
    }
    return nlohmann::ordered_json(nullptr);
}
} // namespace nori::cli::command
