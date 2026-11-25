#include <optional>
#include <string_view>
#include <tuple>
#include <utility>

#include <nlohmann/json.hpp>

#include <nori/resource.hpp>

#include "command.hpp"
#include "common.hpp"

namespace nori::cli::command
{
namespace
{
bool is_ancestor_or_self(resource::handle ancestor, resource::handle target)
{
    for (resource::handle current{target}; current; current = resource::parent_of(current))
    {
        if (current == ancestor)
        {
            return true;
        }
    }
    return false;
}

result<resource::handle> find_deepest(const resource::path& target)
{
    if (!target.has_file_path())
    {
        return std::unexpected{error_code::invalid_argument};
    }

    const auto document{resource::try_get(resource::path{target.file_path()})};
    if (!document)
    {
        return std::unexpected{to_error_code(document.error())};
    }

    resource::handle current{*document};
    for (const std::string_view name : resource::subpath_iterator{target})
    {
        const auto found{resource::try_get(current, name)};
        if (!found)
        {
            if (found.error() == resource::error_code::not_found)
            {
                return current;
            }
            return std::unexpected{to_error_code(found.error())};
        }
        current = *found;
    }
    return current;
}

result<resource::handle> find_destination(resource::handle source, const resource::path& destination_path)
{
    if (const auto existing{resource::try_get(destination_path)})
    {
        if (resource::parent_of(source) == *existing)
        {
            return resource::handle{};
        }
        if (is_ancestor_or_self(source, *existing))
        {
            return std::unexpected{error_code::invalid_argument};
        }
        if (const auto res{prepare_edit(*existing)}; !res)
        {
            return std::unexpected{res.error()};
        }
        return *existing;
    }
    else
    {
        if (existing.error() != resource::error_code::not_found)
        {
            return std::unexpected{to_error_code(existing.error())};
        }

        const auto ancestor{find_deepest(destination_path)};
        if (!ancestor)
        {
            if (ancestor.error() != error_code::not_found)
            {
                return std::unexpected{ancestor.error()};
            }
        }
        else if (is_ancestor_or_self(source, *ancestor))
        {
            return std::unexpected{error_code::invalid_argument};
        }

        const auto created{create(destination_path)};
        if (!created)
        {
            return std::unexpected{created.error()};
        }
        if (is_ancestor_or_self(source, *created))
        {
            return std::unexpected{error_code::invalid_argument};
        }
        return *created;
    }
}
} // namespace

std::optional<copy_args> parse_copy_args(const nlohmann::json& params)
{
    if (!has_valid_param_keys(params, {"source", "destination"}))
    {
        return std::nullopt;
    }
    if (!params.at("source").is_string() || !params.at("destination").is_string())
    {
        return std::nullopt;
    }
    return copy_args{
        resource::path{params.at("source").get_ref<const std::string&>()},
        resource::path{params.at("destination").get_ref<const std::string&>()}
    };
}

std::optional<move_args> parse_move_args(const nlohmann::json& params)
{
    if (!has_valid_param_keys(params, {"source", "destination"}))
    {
        return std::nullopt;
    }
    if (!params.at("source").is_string() || !params.at("destination").is_string())
    {
        return std::nullopt;
    }
    return move_args{
        resource::path{params.at("source").get_ref<const std::string&>()},
        resource::path{params.at("destination").get_ref<const std::string&>()}
    };
}

result<nlohmann::ordered_json> copy(const copy_args& args)
{
    if (args.source.has_file_path() && !args.source.has_subpath())
    {
        return std::unexpected{error_code::invalid_argument};
    }

    const auto source{resource::try_get(args.source)};
    if (!source)
    {
        return std::unexpected{to_error_code(source.error())};
    }
    if (*source == resource::root_of(*source))
    {
        return std::unexpected{error_code::invalid_argument};
    }

    if (const auto res{prepare_edit(*source)}; !res)
    {
        return std::unexpected{res.error()};
    }
    const auto destination{find_destination(*source, args.destination)};
    if (!destination)
    {
        return std::unexpected{destination.error()};
    }
    if (!*destination)
    {
        return nlohmann::ordered_json(nullptr);
    }

    if (const auto res{resource::copy(*source, *destination)}; !res)
    {
        return std::unexpected{to_error_code(res.error())};
    }
    if (const result<void> res{save(*destination, args.destination.file_path())}; !res)
    {
        return std::unexpected{res.error()};
    }
    return nlohmann::ordered_json(nullptr);
}

result<nlohmann::ordered_json> move(const move_args& args)
{
    if (args.source.has_file_path() && !args.source.has_subpath())
    {
        return std::unexpected{error_code::invalid_argument};
    }

    const auto source{resource::try_get(args.source)};
    if (!source)
    {
        return std::unexpected{to_error_code(source.error())};
    }
    if (*source == resource::root_of(*source))
    {
        return std::unexpected{error_code::invalid_argument};
    }

    if (const auto res{prepare_edit(*source)}; !res)
    {
        return std::unexpected{res.error()};
    }
    const auto destination{find_destination(*source, args.destination)};
    if (!destination)
    {
        return std::unexpected{destination.error()};
    }
    if (!*destination)
    {
        return nlohmann::ordered_json(nullptr);
    }

    const resource::handle source_root{resource::root_of(*source)};
    const resource::handle destination_root{resource::root_of(*destination)};
    if (const auto res{resource::move(*source, *destination)}; !res)
    {
        return std::unexpected{to_error_code(res.error())};
    }

    if (source_root == destination_root)
    {
        if (const auto res{resource::save(source_root)}; !res)
        {
            return std::unexpected{to_error_code(res.error())};
        }
    }
    else
    {
        if (const result<void> res{save(*destination, args.destination.file_path())}; !res)
        {
            return std::unexpected{res.error()};
        }
        if (const auto res{resource::save(source_root)}; !res)
        {
            return std::unexpected{to_error_code(res.error())};
        }
    }
    return nlohmann::ordered_json(nullptr);
}
} // namespace nori::cli::command
