#include <cstddef>
#include <optional>

#include <nlohmann/json.hpp>

#include <nori/resource.hpp>

#include "command.hpp"
#include "common.hpp"

namespace
{
nori::cli::result<nlohmann::ordered_json> make_tree(nori::resource::handle handle, std::optional<std::size_t> depth)
{
    nlohmann::ordered_json node(nlohmann::ordered_json::object());
    const nori::resource::path path{nori::resource::path_of(handle)};
    const bool document{path.has_file_path() && !path.has_subpath()};
    if (!document)
    {
        node["name"] = nori::resource::name_of(handle);
        node.update(nori::cli::command::to_json(handle));
    }

    const auto children{nori::resource::children_of(handle)};
    if (!children)
    {
        return std::unexpected{nori::cli::command::to_error_code(children.error())};
    }
    if (children->empty())
    {
        return node;
    }
    if (depth == 0)
    {
        node["truncated"] = true;
        return node;
    }

    nlohmann::ordered_json values(nlohmann::ordered_json::array());
    const auto child_depth{depth.transform([](std::size_t value)
    {
        return value - 1;
    })};
    for (const nori::resource::handle child : *children)
    {
        auto child_node{make_tree(child, child_depth)};
        if (!child_node)
        {
            return std::unexpected{child_node.error()};
        }
        values.push_back(std::move(*child_node));
    }
    node["children"] = std::move(values);
    return node;
}
} // namespace

namespace nori::cli::command
{
std::optional<tree_args> parse_tree_args(const nlohmann::json& params)
{
    if (!has_valid_param_keys(params, {"path"}, {"depth"}))
    {
        return std::nullopt;
    }
    if (!params.at("path").is_string())
    {
        return std::nullopt;
    }

    std::optional<std::size_t> depth;
    if (params.contains("depth"))
    {
        const auto parsed{parse_position(params.at("depth"))};
        if (!parsed)
        {
            return std::nullopt;
        }
        depth = *parsed;
    }
    return tree_args{resource::path{params.at("path").get_ref<const std::string&>()}, depth};
}

result<nlohmann::ordered_json> tree(const tree_args& args)
{
    const auto handle{resource::try_get(args.path)};
    if (!handle)
    {
        return std::unexpected{to_error_code(handle.error())};
    }
    return make_tree(*handle, args.depth);
}
} // namespace nori::cli::command
