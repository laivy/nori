#include <optional>

#include <nlohmann/json.hpp>

#include <nori/resource.hpp>

#include "command.hpp"
#include "common.hpp"

namespace nori::cli::command
{
std::optional<get_args> parse_get_args(const nlohmann::json& params)
{
    if (!has_valid_param_keys(params, {"path"}))
    {
        return std::nullopt;
    }
    if (!params.at("path").is_string())
    {
        return std::nullopt;
    }
    return get_args{resource::path{params.at("path").get_ref<const std::string&>()}};
}

result<nlohmann::ordered_json> get(const get_args& args)
{
    const auto handle{resource::try_get(args.path)};
    if (!handle)
    {
        return std::unexpected{to_error_code(handle.error())};
    }
    if (*handle == resource::root_of(*handle))
    {
        return std::unexpected{error_code::type_mismatch};
    }
    return to_json(*handle);
}
} // namespace nori::cli::command
