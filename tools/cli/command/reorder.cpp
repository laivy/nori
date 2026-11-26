#include <algorithm>
#include <optional>
#include <vector>

#include <nlohmann/json.hpp>

#include <nori/resource.hpp>

#include "command.hpp"
#include "common.hpp"

namespace nori::cli::command
{
std::optional<reorder_args> parse_reorder_args(const nlohmann::json& params)
{
    if (!has_valid_param_keys(params, {"path", "position"}))
    {
        return std::nullopt;
    }
    if (!params.at("path").is_string())
    {
        return std::nullopt;
    }
    const auto position{parse_position(params.at("position"))};
    if (!position)
    {
        return std::nullopt;
    }
    return reorder_args{resource::path{params.at("path").get_ref<const std::string&>()}, *position};
}

result<nlohmann::ordered_json> reorder(const reorder_args& args)
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
    const resource::handle parent{resource::parent_of(*target)};
    const auto siblings{resource::children_of(parent)};
    if (!siblings)
    {
        return std::unexpected{to_error_code(siblings.error())};
    }
    const auto it{std::ranges::find(*siblings, *target)};
    if (it != siblings->end() && static_cast<std::size_t>(it - siblings->begin()) == args.position)
    {
        return nlohmann::ordered_json(nullptr);
    }
    if (const auto res{prepare_edit(*target)}; !res)
    {
        return std::unexpected{res.error()};
    }
    if (const auto res{resource::reorder(*target, args.position)}; !res)
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
