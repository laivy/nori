#include <filesystem>
#include <optional>

#include <nlohmann/json.hpp>

#include <nori/resource.hpp>

#include "command.hpp"
#include "common.hpp"

namespace nori::cli::command
{
std::optional<del_args> parse_del_args(const nlohmann::json& params)
{
    if (!has_valid_param_keys(params, {"path"}))
    {
        return std::nullopt;
    }
    if (!params.at("path").is_string())
    {
        return std::nullopt;
    }
    return del_args{resource::path{params.at("path").get_ref<const std::string&>()}};
}

result<nlohmann::ordered_json> del(const del_args& args)
{
    const auto target{resource::try_get(args.path)};
    if (!target)
    {
        return std::unexpected{to_error_code(target.error())};
    }

    const resource::handle root{resource::root_of(*target)};
    if (*target == root)
    {
        const std::filesystem::path file_path{resource::path_of(root).file_path()};
        resource::remove(root);
        std::error_code error;
        if (!std::filesystem::remove(file_path, error) || error)
        {
            return std::unexpected{error_code::io_error};
        }
        return nlohmann::ordered_json(nullptr);
    }

    if (const auto res{prepare_edit(root)}; !res)
    {
        return std::unexpected{res.error()};
    }
    resource::remove(*target);
    if (const auto res{resource::save(root)}; !res)
    {
        return std::unexpected{to_error_code(res.error())};
    }
    return nlohmann::ordered_json(nullptr);
}
} // namespace nori::cli::command
