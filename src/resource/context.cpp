#include <algorithm>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "codec/writer.hpp"
#include "context.hpp"
#include "property.hpp"

namespace
{
nori::resource::result<void> serialize(const std::filesystem::path& file_path, const nori::resource::hierarchy& hier, nori::resource::detail::node_handle root)
{
    struct task
    {
        nori::resource::detail::node_handle target;
        std::uint32_t depth;
    };

    std::vector<task> tasks;
    for (const auto child : hier.children_of(root) | std::views::reverse)
    {
        tasks.emplace_back(child, 0);
    }

    nori::resource::codec::writer writer{file_path};
    while (!tasks.empty())
    {
        const auto [target, depth]{tasks.back()};
        tasks.pop_back();
        if (const auto res{writer.append(hier.name_of(target), hier.value_of(target), depth)}; !res)
        {
            return res;
        }
        for (const auto child : hier.children_of(target) | std::views::reverse)
        {
            tasks.emplace_back(child, depth + 1);
        }
    }
    return writer.finish();
}
} // namespace

namespace nori::resource
{
context::context(std::filesystem::path root_path) :
    files_{std::move(root_path)}
{
}

handle context::create_root()
{
    std::lock_guard lock{hierarchies_mutex_};

    const detail::hierarchy_handle hid{hierarchies_.emplace()};
    hierarchy& hier{hierarchies_.at(hid)};
    hier.set_identity(hid);

    property prop{.value = empty_t{}};
    const detail::node_handle node{hier.create(std::move(prop))};
    hier.set_root(node);

    return handle{hid, node};
}

handle context::create(handle parent, property prop)
{
    hierarchy& hier{hierarchy_of(parent.hier)};
    const detail::node_handle node{hier.create(std::move(prop))};
    if (!hier.attach(parent.node, node))
    {
        hier.remove(node);
        return {};
    }
    return handle{parent.hier, node};
}

result<handle> context::copy_subtree(handle source, handle destination)
{
    struct entry
    {
        std::size_t parent;
        detail::node_handle source;
        property prop;
        detail::node_handle destination;
    };

    const hierarchy& src_hier{hierarchy_of(source.hier)};
    std::vector<entry> entries;
    entries.push_back(entry{.source = source.node});
    for (std::size_t i{0}; i < entries.size(); ++i)
    {
        const detail::node_handle source_node{entries[i].source};
        entries[i].prop.name = src_hier.name_of(source_node);
        entries[i].prop.value = src_hier.value_of(source_node);
        for (const auto child : src_hier.children_of(source_node))
        {
            entries.push_back(entry{.parent = i, .source = child});
        }
    }

    hierarchy& dst_hier{hierarchy_of(destination.hier)};
    for (auto&& [i, e] : entries | std::views::enumerate)
    {
        const detail::node_handle parent{i == 0 ? destination.node : entries[e.parent].destination};
        if (!parent)
        {
            continue;
        }
        if (const auto res{load_child_list(dst_hier.identity_of(parent))}; !res)
        {
            return std::unexpected{res.error()};
        }
        e.destination = dst_hier.find_child(parent, e.prop.name);
    }

    for (auto&& [i, e] : entries | std::views::enumerate)
    {
        if (e.destination)
        {
            dst_hier.set(e.destination, std::move(e.prop.value));
            continue;
        }
        const detail::node_handle parent{i == 0 ? destination.node : entries[e.parent].destination};
        e.destination = dst_hier.create(std::move(e.prop));
        const auto res{dst_hier.attach(parent, e.destination)};
        assert(res);
    }
    return dst_hier.identity_of(entries.front().destination);
}

result<handle> context::move_subtree(handle source, handle destination)
{
    struct entry
    {
        detail::node_handle parent;
        detail::node_handle source;
        detail::node_handle destination;
    };

    hierarchy& hier{hierarchy_of(destination.hier)};
    std::vector<entry> entries;
    entries.push_back(entry{destination.node, source.node});
    for (std::size_t i{0}; i < entries.size(); ++i)
    {
        const auto [parent, src, _]{entries[i]};
        if (const auto res{load_child_list(hier.identity_of(parent))}; !res)
        {
            return std::unexpected{res.error()};
        }

        const detail::node_handle existing{hier.find_child(parent, hier.name_of(src))};
        entries[i].destination = existing;
        if (existing)
        {
            for (const auto child : hier.children_of(src))
            {
                entries.push_back(entry{existing, child});
            }
        }
    }

    for (const auto& e : entries)
    {
        if (e.destination)
        {
            hier.set(e.destination, std::move(hier.value_of(e.source)));
        }
        else
        {
            const auto res{hier.attach(e.parent, e.source)};
            assert(res);
        }
    }

    for (const auto& e : entries | std::views::reverse)
    {
        if (e.destination)
        {
            hier.remove(e.source);
        }
    }
    const detail::node_handle merged{entries.front().destination};
    return merged ? hier.identity_of(merged) : source;
}

result<handle> context::copy(handle source, handle destination)
{
    if (source.hier == destination.hier)
    {
        if (parent_of(source) == destination)
        {
            return source;
        }
        for (handle current{destination}; current; current = parent_of(current))
        {
            if (current == source)
            {
                return std::unexpected{error_code::invalid_relationship};
            }
        }
    }

    if (const auto res{load(source)}; !res)
    {
        return std::unexpected{res.error()};
    }

    return copy_subtree(source, destination);
}

result<handle> context::move(handle source, handle destination)
{
    const bool is_same_hierarchy{source.hier == destination.hier};
    if (is_same_hierarchy)
    {
        if (parent_of(source) == destination)
        {
            return source;
        }
        for (handle current{destination}; current; current = parent_of(current))
        {
            if (current == source)
            {
                return std::unexpected{error_code::invalid_relationship};
            }
        }
        if (const auto res{load_child_list(destination)}; !res)
        {
            return std::unexpected{res.error()};
        }
        const hierarchy& hier{hierarchy_of(destination.hier)};
        if (const auto existing{hier.find_child(destination.node, hier.name_of(source.node))})
        {
            for (handle current{source}; current; current = parent_of(current))
            {
                if (current.node == existing)
                {
                    return std::unexpected{error_code::invalid_relationship};
                }
            }
        }
    }

    if (const auto res{load(source)}; !res)
    {
        return std::unexpected{res.error()};
    }

    for (handle ancestor{parent_of(source)}; ancestor; ancestor = parent_of(ancestor))
    {
        if (const auto res{load_child_list(ancestor)}; !res)
        {
            return std::unexpected{res.error()};
        }
    }

    if (is_same_hierarchy)
    {
        return move_subtree(source, destination);
    }

    const auto res{copy_subtree(source, destination)};
    if (res)
    {
        remove(source);
    }
    return res;
}

void context::remove(handle target)
{
    hierarchy& hier{hierarchy_of(target.hier)};
    const detail::node_handle node{target.node};
    if (node == hier.root())
    {
        files_.unbind(target);
        std::lock_guard lock{hierarchies_mutex_};
        hierarchies_.erase(target.hier);
    }
    else
    {
        hier.remove(node);
    }
}

result<void> context::reorder(handle target, std::size_t position)
{
    return hierarchy_of(target.hier).reorder(target.node, position);
}

result<void> context::rename(handle target, std::string_view name)
{
    return hierarchy_of(target.hier).rename(target.node, name);
}

std::string context::name_of(handle target) const
{
    const hierarchy& hier{hierarchy_of(target.hier)};
    std::shared_lock lock{hier.lock_nodes_shared()};
    return std::string{hier.name_of(target.node)};
}

path context::path_of(handle target) const
{
    const hierarchy& hier{hierarchy_of(target.hier)};
    std::shared_lock lock{hier.lock_nodes_shared()};

    const handle root{hier.identity_of(hier.root())};
    if (target == root)
    {
        const std::filesystem::path file_path{files_.file_path_of(root)};
        return file_path.empty() ? path{} : path{file_path};
    }

    std::vector<std::string_view> names;
    for (detail::node_handle current{target.node}; current != root.node; current = hier.parent_of(current))
    {
        if (!current)
        {
            return {};
        }
        names.push_back(hier.name_of(current));
    }

    std::string subpath;
    for (const std::string_view name : names | std::views::reverse)
    {
        if (!subpath.empty())
        {
            subpath.push_back(path::separator);
        }
        subpath.append(name);
    }
    const std::filesystem::path file_path{files_.file_path_of(root)};
    if (file_path.empty())
    {
        return path{std::move(subpath)};
    }
    return path{file_path} / path{std::move(subpath)};
}

property_type context::type_of(handle target) const
{
    const hierarchy& hier{hierarchy_of(target.hier)};
    std::shared_lock lock{hier.lock_nodes_shared()};
    return hier.type_of(target.node);
}

result<handle> context::try_get(const path& target)
{
    const auto abs_path{files_.absolute(target.file_path())};
    if (!abs_path)
    {
        return std::unexpected{abs_path.error()};
    }

    if (const handle root{files_.find(*abs_path)})
    {
        return try_get(root, path{target.subpath()});
    }

    const handle root{create_root()};
    hierarchy& hier{hierarchy_of(root.hier)};
    hier.set_source(*abs_path);
    if (const auto res{hier.source()->open()}; !res)
    {
        remove(root);
        return std::unexpected{res.error()};
    }
    hier.unload_children(root.node);

    const auto found{try_get(root, path{target.subpath()})};
    if (!found)
    {
        remove(root);
        return std::unexpected{found.error()};
    }

    const handle bound_root{files_.bind(root, *abs_path)};
    if (root != bound_root)
    {
        remove(root);
        return try_get(bound_root, path{target.subpath()});
    }
    return found;
}

result<handle> context::try_get(handle parent, const path& subpath)
{
    if (subpath.has_file_path())
    {
        return std::unexpected{error_code::invalid_path};
    }

    hierarchy& hier{hierarchy_of(parent.hier)};
    std::optional<loader::reader> reader;
    std::optional<loader::reader::cursor> cursor;
    for (const auto name : subpath_iterator{subpath})
    {
        if (name == ".")
        {
            continue;
        }
        if (name == "..")
        {
            std::shared_lock lock{hier.lock_nodes_shared()};
            const detail::node_handle ancestor{hier.parent_of(parent.node)};
            if (!ancestor)
            {
                return std::unexpected{error_code::invalid_path};
            }
            parent = hier.identity_of(ancestor);
            cursor.reset();
            continue;
        }
        if (!is_valid_property_name(name))
        {
            return std::unexpected{error_code::invalid_path};
        }
        {
            std::shared_lock lock{hier.lock_nodes_shared()};
            if (const detail::node_handle child{hier.find_child(parent.node, name)})
            {
                parent = hier.identity_of(child);
                cursor.reset();
                continue;
            }
            if (hier.is_child_list_complete(parent.node))
            {
                return std::unexpected{error_code::not_found};
            }
        }

        if (!reader)
        {
            auto res{reader_of(parent)};
            if (!res)
            {
                return std::unexpected{res.error()};
            }
            reader.emplace(std::move(*res));
        }
        if (!cursor)
        {
            std::shared_lock lock{hier.lock_nodes_shared()};
            cursor = cursor_for(*reader, parent);
        }
        cursor = cursor->find_child(name);
        if (!cursor)
        {
            return std::unexpected{error_code::not_found};
        }

        std::unique_lock lock{hier.lock_nodes_exclusive()};
        if (const detail::node_handle existing{hier.find_child(parent.node, name)})
        {
            parent = hier.identity_of(existing);
            continue;
        }
        const detail::node_handle child_node{hier.create_placeholder(cursor->name(), cursor->type())};
        const auto attached{hier.attach(parent.node, child_node)};
        assert(attached);
        parent = hier.identity_of(child_node);
    }
    return parent;
}

handle context::root_of(handle target) const
{
    const hierarchy& hier{hierarchy_of(target.hier)};
    std::shared_lock lock{hier.lock_nodes_shared()};
    return hier.identity_of(hier.root());
}

handle context::parent_of(handle target) const
{
    const hierarchy& hier{hierarchy_of(target.hier)};
    std::shared_lock lock{hier.lock_nodes_shared()};
    return hier.identity_of(hier.parent_of(target.node));
}

result<std::vector<handle>> context::children_of(handle target)
{
    const hierarchy& hier{hierarchy_of(target.hier)};
    const auto res{load_child_list(target)};
    if (!res)
    {
        return std::unexpected{res.error()};
    }

    std::shared_lock lock{hier.lock_nodes_shared()};
    const std::span<const detail::node_handle> children{hier.children_of(target.node)};
    std::vector<handle> result;
    result.reserve(children.size());
    for (const detail::node_handle child : children)
    {
        result.push_back(hier.identity_of(child));
    }
    return result;
}

result<std::size_t> context::position_of(handle target)
{
    const handle parent{parent_of(target)};
    if (!parent)
    {
        return std::unexpected{error_code::invalid_relationship};
    }
    if (const auto res{load_child_list(parent)}; !res)
    {
        return std::unexpected{res.error()};
    }
    const hierarchy& hier{hierarchy_of(parent.hier)};
    std::shared_lock lock{hier.lock_nodes_shared()};
    const std::span<const detail::node_handle> siblings{hier.children_of(parent.node)};
    const auto it{std::ranges::find(siblings, target.node)};
    assert(it != siblings.end());
    return static_cast<std::size_t>(it - siblings.begin());
}

result<void> context::save(handle target)
{
    const std::filesystem::path file_path{files_.file_path_of(target)};
    if (file_path.empty())
    {
        return std::unexpected{error_code::invalid_relationship};
    }

    hierarchy& hier{hierarchy_of(target.hier)};
    if (const auto res{serialize(file_path, hier, target.node)}; !res)
    {
        return res;
    }
    hier.set_source(file_path);
    return {};
}

result<void> context::save_as(handle target, const std::filesystem::path& file_path)
{
    if (target != root_of(target))
    {
        return std::unexpected{error_code::invalid_relationship};
    }

    const auto abs_path{files_.absolute(file_path)};
    if (!abs_path)
    {
        return std::unexpected{abs_path.error()};
    }

    if (const handle conflict{files_.find(*abs_path)}; conflict && conflict != target)
    {
        return std::unexpected{error_code::name_conflict};
    }

    hierarchy& hier{hierarchy_of(target.hier)};
    if (const auto res{serialize(*abs_path, hier, target.node)}; !res)
    {
        return res;
    }
    if (files_.bind(target, *abs_path) != target)
    {
        return std::unexpected{error_code::name_conflict};
    }
    hier.set_source(*abs_path);
    return {};
}

result<void> context::load(handle target)
{
    hierarchy& hier{hierarchy_of(target.hier)};
    std::vector<detail::node_handle> materialized{target.node};
    for (std::size_t i{0}; i < materialized.size(); ++i)
    {
        materialized.append_range(hier.children_of(materialized[i]));
    }

    std::unordered_set<detail::node_handle> incomplete;
    for (const auto current : materialized | std::views::reverse)
    {
        if (!hier.is_value_loaded(current) || !hier.is_child_list_complete(current) || incomplete.contains(current))
        {
            incomplete.insert(current);
            if (const detail::node_handle parent{hier.parent_of(current)})
            {
                incomplete.insert(parent);
            }
        }
    }
    if (!incomplete.contains(target.node))
    {
        return {};
    }

    struct entry
    {
        detail::node_handle target;
        std::optional<loader::reader::cursor> cursor;
        std::optional<std::size_t> value_index;
        std::size_t first_child;
        std::size_t child_count;
    };

    auto reader{reader_of(target)};
    if (!reader)
    {
        return std::unexpected{reader.error()};
    }

    std::vector<entry> entries{entry{
        .target = target.node,
        .cursor = cursor_for(*reader, target),
    }};
    std::vector<property::value_type> values;
    for (std::size_t index{0}; index < entries.size(); ++index)
    {
        const entry e{entries[index]};
        if (!e.cursor)
        {
            continue;
        }

        if (!e.target || !hier.is_value_loaded(e.target))
        {
            auto value{e.cursor->value()};
            if (!value)
            {
                return std::unexpected{value.error()};
            }
            entries[index].value_index = values.size();
            values.push_back(std::move(*value));
        }

        entries[index].first_child = entries.size();
        if (e.target && hier.is_child_list_complete(e.target))
        {
            for (const auto child : hier.children_of(e.target))
            {
                if (incomplete.contains(child))
                {
                    const auto cursor{e.cursor->find_child(hier.name_of(child))};
                    assert(cursor);
                    entries.push_back(entry{.target = child, .cursor = cursor});
                }
            }
        }
        else
        {
            for (const auto child : e.cursor->children())
            {
                const detail::node_handle existing{e.target ? hier.find_child(e.target, child.name()) : detail::node_handle{}};
                entries.push_back(entry{
                    .target = existing,
                    .cursor = !existing || incomplete.contains(existing) ? std::optional{child} : std::nullopt,
                });
            }
        }
        entries[index].child_count = entries.size() - entries[index].first_child;
    }

    std::vector<detail::node_handle> ordered;
    for (auto& e : entries)
    {
        assert(e.target);
        if (!e.cursor)
        {
            continue;
        }

        if (e.value_index)
        {
            hier.set(e.target, std::move(values[*e.value_index]));
        }
        if (hier.is_child_list_complete(e.target))
        {
            continue;
        }

        const std::span<entry> children{std::span{entries}.subspan(e.first_child, e.child_count)};
        ordered.clear();
        ordered.reserve(children.size());
        if (!children.empty())
        {
            hier.reserve_children(e.target, children.size());
        }
        for (auto& child : children)
        {
            if (!child.target)
            {
                child.target = hier.create_placeholder(child.cursor->name(), child.cursor->type());
                const auto res{hier.attach(e.target, child.target)};
                assert(res);
            }
            ordered.push_back(child.target);
        }
        hier.complete_children(e.target, ordered);
    }
    return {};
}

result<void> context::unload(handle target)
{
    const handle root{root_of(target)};
    if (target != root && path_of(target).empty())
    {
        return std::unexpected{error_code::invalid_relationship};
    }
    if (files_.file_path_of(root).empty())
    {
        return std::unexpected{error_code::invalid_relationship};
    }
    if (target == root)
    {
        remove(target);
        return {};
    }

    hierarchy& hier{hierarchy_of(target.hier)};
    const detail::node_handle node{target.node};
    hier.unload_value(node);
    hier.unload_children(node);
    return {};
}

result<void> context::load_value(handle target)
{
    hierarchy& hier{hierarchy_of(target.hier)};
    const detail::node_handle node{target.node};
    {
        std::shared_lock lock{hier.lock_nodes_shared()};
        if (hier.is_value_loaded(node))
        {
            return {};
        }
    }

    auto reader{reader_of(target)};
    if (!reader)
    {
        return std::unexpected{reader.error()};
    }
    loader::reader::cursor cursor{reader->root()};
    {
        std::shared_lock lock{hier.lock_nodes_shared()};
        cursor = cursor_for(*reader, target);
    }
    auto value{cursor.value()};
    if (!value)
    {
        return std::unexpected{value.error()};
    }
    {
        std::unique_lock lock{hier.lock_nodes_exclusive()};
        if (!hier.is_value_loaded(node))
        {
            hier.set(node, std::move(*value));
        }
    }
    return {};
}

result<void> context::load_child_list(handle target)
{
    hierarchy& hier{hierarchy_of(target.hier)};
    const detail::node_handle node{target.node};
    {
        std::shared_lock lock{hier.lock_nodes_shared()};
        if (hier.is_child_list_complete(node))
        {
            return {};
        }
    }

    auto reader{reader_of(target)};
    if (!reader)
    {
        return std::unexpected{reader.error()};
    }

    loader::reader::cursor cursor{reader->root()};
    {
        std::shared_lock lock{hier.lock_nodes_shared()};
        cursor = cursor_for(*reader, target);
    }

    const std::vector<loader::reader::cursor> children{cursor.children()};
    std::unique_lock lock{hier.lock_nodes_exclusive()};
    if (hier.is_child_list_complete(node))
    {
        return {};
    }

    std::vector<detail::node_handle> ordered;
    ordered.reserve(children.size());
    for (const auto child : children)
    {
        detail::node_handle existing{hier.find_child(node, child.name())};
        if (!existing)
        {
            existing = hier.create_placeholder(child.name(), child.type());
            const auto res{hier.attach(node, existing)};
            assert(res);
        }
        ordered.push_back(existing);
    }
    hier.complete_children(node, ordered);
    return {};
}

result<loader::reader> context::reader_of(handle target)
{
    const auto source{hierarchy_of(target.hier).source()};
    if (!source)
    {
        return std::unexpected{error_code::invalid_relationship};
    }
    return source->open();
}

loader::reader::cursor context::cursor_for(loader::reader& reader, handle target)
{
    const hierarchy& hier{hierarchy_of(target.hier)};
    detail::node_handle current{target.node};
    std::vector<detail::node_handle> chain;
    while (const detail::node_handle parent{hier.parent_of(current)})
    {
        chain.push_back(current);
        current = parent;
    }

    loader::reader::cursor cursor{reader.root()};
    for (const auto node : chain | std::views::reverse)
    {
        const std::optional<loader::reader::cursor> child{cursor.find_child(hier.name_of(node))};
        assert(child);
        cursor = *child;
    }
    return cursor;
}
} // namespace nori::resource
