#include <cstddef>
#include <format>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include <nori/resource.hpp>

#include "command.hpp"
#include "common.hpp"

namespace
{
std::string child_path(std::string_view parent, std::string_view name)
{
    if (parent == "/")
    {
        return std::format("/{}", name);
    }
    return std::format("{}/{}", parent, name);
}

std::unordered_map<std::string, nori::resource::handle> index_children(const std::vector<nori::resource::handle>& children)
{
    std::unordered_map<std::string, nori::resource::handle> index;
    index.reserve(children.size());
    for (const nori::resource::handle child : children)
    {
        index.emplace(nori::resource::name_of(child), child);
    }
    return index;
}

void append_replace(nlohmann::ordered_json& changes, std::string_view path, nlohmann::ordered_json before, nlohmann::ordered_json after)
{
    changes.push_back(nlohmann::ordered_json{
        {"op", "replace"},
        {"path", path},
        {"before", std::move(before)},
        {"after", std::move(after)}
    });
}

nori::cli::result<void> append_subtree(nlohmann::ordered_json& changes, nori::resource::handle handle, std::string path, std::string_view operation, std::string_view snapshot_name)
{
    nlohmann::ordered_json change{{"op", operation}, {"path", path}};
    change[std::string{snapshot_name}] = nori::cli::command::to_json(handle);
    changes.push_back(std::move(change));
    const auto children{nori::resource::children_of(handle)};
    if (!children)
    {
        return std::unexpected{nori::cli::command::to_error_code(children.error())};
    }
    for (const nori::resource::handle child : *children)
    {
        if (const auto res{append_subtree(changes, child, child_path(path, nori::resource::name_of(child)), operation, snapshot_name)}; !res)
        {
            return res;
        }
    }
    return {};
}

bool same_value(nori::resource::handle before, nori::resource::handle after)
{
    const nori::resource::property_type type{nori::resource::type_of(before)};
    if (type == nori::resource::property_type::binary)
    {
        return std::ranges::equal(
            nori::resource::get<nori::resource::binary_view>(before).data,
            nori::resource::get<nori::resource::binary_view>(after).data
        );
    }
    if (type == nori::resource::property_type::image)
    {
        const auto lhs{nori::resource::get<nori::resource::image_view>(before)};
        const auto rhs{nori::resource::get<nori::resource::image_view>(after)};
        return lhs.format == rhs.format &&
            lhs.width == rhs.width &&
            lhs.height == rhs.height &&
            lhs.mip_levels == rhs.mip_levels &&
            std::ranges::equal(lhs.data, rhs.data);
    }
    return nori::cli::command::to_json(before) == nori::cli::command::to_json(after);
}

void append_reorders(nlohmann::ordered_json& changes, const std::vector<nori::resource::handle>& before_children, const std::vector<nori::resource::handle>& after_children, std::string_view path)
{
    std::unordered_map<std::string, std::size_t> after_position;
    after_position.reserve(after_children.size());
    for (const auto [i, child] : std::views::enumerate(after_children))
    {
        after_position.emplace(nori::resource::name_of(child), static_cast<std::size_t>(i));
    }

    std::vector<std::string> names;
    std::vector<std::size_t> before_positions;
    std::vector<std::size_t> after_positions;
    names.reserve(before_children.size());
    before_positions.reserve(before_children.size());
    after_positions.reserve(before_children.size());
    for (const auto [i, child] : std::views::enumerate(before_children))
    {
        const std::string name{nori::resource::name_of(child)};
        if (const auto it{after_position.find(name)}; it != after_position.end())
        {
            names.push_back(name);
            before_positions.push_back(static_cast<std::size_t>(i));
            after_positions.push_back(it->second);
        }
    }

    const std::size_t count{names.size()};
    std::vector<std::size_t> lengths(count, 1);
    std::vector<std::size_t> predecessors(count, count);
    std::size_t best{0};
    for (std::size_t i{0}; i < count; ++i)
    {
        for (std::size_t j{0}; j < i; ++j)
        {
            if (after_positions[j] < after_positions[i] && lengths[j] + 1 > lengths[i])
            {
                lengths[i] = lengths[j] + 1;
                predecessors[i] = j;
            }
        }
        if (lengths[i] > lengths[best])
        {
            best = i;
        }
    }

    std::vector<bool> ordered(count, false);
    if (count > 0)
    {
        for (std::size_t i{best}; i < count; i = predecessors[i])
        {
            ordered[i] = true;
            if (predecessors[i] >= count)
            {
                break;
            }
        }
    }

    for (std::size_t i{0}; i < count; ++i)
    {
        if (!ordered[i] && before_positions[i] != after_positions[i])
        {
            changes.push_back(nlohmann::ordered_json{
                {"op", "reorder"},
                {"path", child_path(path, names[i])},
                {"before", before_positions[i]},
                {"after", after_positions[i]}
            });
        }
    }
}

nori::cli::result<void> append_changes(nlohmann::ordered_json& changes, nori::resource::handle before, nori::resource::handle after, std::string_view path)
{
    const nori::resource::path before_path{nori::resource::path_of(before)};
    const nori::resource::path after_path{nori::resource::path_of(after)};
    const bool before_document{before_path.has_file_path() && !before_path.has_subpath()};
    const bool after_document{after_path.has_file_path() && !after_path.has_subpath()};
    if (before_document != after_document)
    {
        append_replace(changes, path, nori::cli::command::to_json(before), nori::cli::command::to_json(after));
    }
    else if (!before_document)
    {
        const nori::resource::property_type before_type{nori::resource::type_of(before)};
        const nori::resource::property_type after_type{nori::resource::type_of(after)};
        if (before_type != after_type || !same_value(before, after))
        {
            append_replace(changes, path, nori::cli::command::to_json(before), nori::cli::command::to_json(after));
        }
    }

    const auto before_children{nori::resource::children_of(before)};
    if (!before_children)
    {
        return std::unexpected{nori::cli::command::to_error_code(before_children.error())};
    }
    const auto after_children{nori::resource::children_of(after)};
    if (!after_children)
    {
        return std::unexpected{nori::cli::command::to_error_code(after_children.error())};
    }
    const auto before_index{index_children(*before_children)};
    const auto after_index{index_children(*after_children)};

    for (const nori::resource::handle before_child : *before_children)
    {
        const std::string name{nori::resource::name_of(before_child)};
        const std::string current_path{child_path(path, name)};
        if (const auto it{after_index.find(name)}; it != after_index.end())
        {
            if (const auto res{append_changes(changes, before_child, it->second, current_path)}; !res)
            {
                return res;
            }
        }
        else
        {
            if (const auto res{append_subtree(changes, before_child, current_path, "remove", "before")}; !res)
            {
                return res;
            }
        }
    }
    for (const nori::resource::handle after_child : *after_children)
    {
        const std::string name{nori::resource::name_of(after_child)};
        if (!before_index.contains(name))
        {
            if (const auto res{append_subtree(changes, after_child, child_path(path, name), "add", "after")}; !res)
            {
                return res;
            }
        }
    }
    append_reorders(changes, *before_children, *after_children, path);
    return {};
}
} // namespace

namespace nori::cli::command
{
std::optional<diff_args> parse_diff_args(const nlohmann::json& params)
{
    if (!has_valid_param_keys(params, {"before", "after"}))
    {
        return std::nullopt;
    }
    if (!params.at("before").is_string() || !params.at("after").is_string())
    {
        return std::nullopt;
    }
    return diff_args{
        resource::path{params.at("before").get_ref<const std::string&>()},
        resource::path{params.at("after").get_ref<const std::string&>()}
    };
}

result<nlohmann::ordered_json> diff(const diff_args& args)
{
    const auto before{resource::try_get(args.before)};
    if (!before)
    {
        return std::unexpected{to_error_code(before.error())};
    }
    const auto after{resource::try_get(args.after)};
    if (!after)
    {
        return std::unexpected{to_error_code(after.error())};
    }
    const bool before_document{*before == resource::root_of(*before)};
    const bool after_document{*after == resource::root_of(*after)};
    if (before_document != after_document)
    {
        return std::unexpected{error_code::type_mismatch};
    }

    nlohmann::ordered_json changes(nlohmann::ordered_json::array());
    if (const auto res{append_changes(changes, *before, *after, "/")}; !res)
    {
        return std::unexpected{res.error()};
    }
    return changes;
}
} // namespace nori::cli::command
