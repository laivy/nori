#include <algorithm>
#include <cassert>
#include <format>
#include <ranges>
#include <unordered_set>
#include "context.hpp"
#include "deserialize.hpp"
#include "serialize.hpp"

namespace nori::resource
{
context::context(runtime::specification spec) :
    mount_path_{std::move(spec.mount_path)}
{
}

handle context::create(std::string_view path, const property::value_type& value)
{
    std::string key{path};
    if (auto it{cache_.find(key)}; it != cache_.end())
    {
        return it->second;
    }

    const std::size_t pos = [path]()
    {
        const std::size_t ext_pos{path.rfind(extension)};
        if (ext_pos == std::string_view::npos)
        {
            return path.rfind(delimiter);
        }

        const std::size_t start_pos{ext_pos + extension.size()};
        const std::size_t last_pos{path.rfind(delimiter)};
        if (last_pos == std::string_view::npos || start_pos > last_pos)
        {
            return std::string_view::npos;
        }
        return last_pos;
    }();
    const handle parent = [this, path, pos]() -> handle
    {
        if (pos == std::string_view::npos)
        {
            return {};
        }
        const std::string parentpath{path.substr(0, pos)};
        if (auto it{cache_.find(parentpath)}; it != cache_.end())
        {
            return it->second;
        }
        return create(parentpath, std::monostate{});
    }();

    resource_node node{.state = resource_state::deserialized};
    if (pos == std::string::npos)
    {
        node.prop = property{.name = key};
    }
    else
    {
        node.prop = property{.name = key.substr(pos + 1)};
    }
    node.prop.value = value;
    const handle resource_handle{nodes_.emplace(std::move(node))};
    cache_.emplace(std::move(key), resource_handle);
    if (parent)
    {
        resolve(resource_handle).parent = parent;
        resolve(parent).children.push_back(resource_handle);
    }
    return resource_handle;
}

void context::remove(handle target)
{
    assert(is_valid(target));

    std::vector<handle> nodes_to_remove;
    [this, &nodes_to_remove](this auto&& self, handle current) -> void
    {
        nodes_to_remove.push_back(current);
        for (const handle child : resolve(current).children)
        {
            self(child);
        }
    }(target);

    std::string path{get_path(target)};
    [this](this auto&& self, handle current, std::string& path) -> void
    {
        cache_.erase(path);
        const resource_node& node{resolve(current)};
        const std::size_t size{path.size()};
        for (const handle child : node.children)
        {
            const resource_node& childnode{resolve(child)};
            path.push_back(delimiter);
            path.append(childnode.prop.name);
            self(child, path);
            path.resize(size);
        }
    }(target, path);

    if (const handle parent{resolve(target).parent}; parent)
    {
        std::erase(resolve(parent).children, target);
    }

    for (const handle current : nodes_to_remove)
    {
        nodes_.erase(current);
    }
}

result<void> context::set_parent(handle child, handle parent)
{
    assert(is_valid(child));
    assert(is_valid(parent));
    if (child == parent)
    {
        return std::unexpected{error_code::invalid_relationship};
    }

    resource_node& child_node{resolve(child)};
    resource_node& parent_node{resolve(parent)};
    if (child_node.parent == parent)
    {
        return {};
    }
    for (handle current{parent}; current; current = resolve(current).parent)
    {
        if (current == child)
        {
            return std::unexpected{error_code::invalid_relationship};
        }
    }

    const std::string old_path{get_path(child)};
    const std::string new_path{std::format("{}{}{}", get_path(parent), delimiter, child_node.prop.name)};
    std::vector<std::pair<std::string, std::string>> changes;
    [this, &changes, &old_path, &new_path](this auto&& self, handle current) -> void
    {
        const std::string path{get_path(current)};
        std::string replacement{path};
        replacement.replace(0, old_path.size(), new_path);
        changes.emplace_back(path, std::move(replacement));
        for (const handle descendant : resolve(current).children)
        {
            self(descendant);
        }
    }(child);

    std::unordered_set<std::string> old_keys;
    old_keys.reserve(changes.size());
    for (const auto& [old_key, new_key] : changes)
    {
        old_keys.insert(old_key);
    }
    for (const auto& [old_key, new_key] : changes)
    {
        if (cache_.contains(new_key) && !old_keys.contains(new_key))
        {
            return std::unexpected{error_code::name_conflict};
        }
    }
    std::vector<decltype(cache_)::node_type> cache_nodes;
    cache_nodes.reserve(changes.size());
    for (const auto& [old_key, new_key] : changes)
    {
        auto cache_node{cache_.extract(old_key)};
        assert(cache_node);
        cache_nodes.push_back(std::move(cache_node));
    }
    for (std::size_t i{0}; i < changes.size(); ++i)
    {
        cache_nodes[i].key() = changes[i].second;
        const auto inserted{cache_.insert(std::move(cache_nodes[i]))};
        assert(inserted.inserted);
    }

    if (const handle old_parent{child_node.parent}; old_parent)
    {
        std::erase(resolve(old_parent).children, child);
    }
    child_node.parent = parent;
    parent_node.children.push_back(child);
    return {};
}

result<void> context::set_name(handle target, std::string_view name)
{
    assert(is_valid(target));
    resource_node& node{resolve(target)};
    if (node.prop.name == name)
    {
        return {};
    }

    const std::string oldpath{get_path(target)};
    const std::string newpath = [this, name, &node]()
    {
        std::string path;
        if (!node.parent)
        {
            path = name;
        }
        else
        {
            path = std::format("{}{}{}", get_path(node.parent), delimiter, name);
        }
        return path;
    }();
    std::vector<std::pair<std::string, std::string>> changes;
    [this, &changes, &oldpath, &newpath](this auto&& self, handle current) -> void
    {
        const std::string path{get_path(current)};
        std::string replacement{path};
        assert(replacement.starts_with(oldpath));
        replacement.replace(0, oldpath.size(), newpath);
        changes.emplace_back(path, std::move(replacement));
        const resource_node& node{resolve(current)};
        for (const handle child : node.children)
        {
            self(child);
        }
    }(target);

    std::unordered_set<std::string> old_keys;
    old_keys.reserve(changes.size());
    for (const auto& [old_key, new_key] : changes)
    {
        old_keys.insert(old_key);
    }
    for (const auto& [old_key, new_key] : changes)
    {
        if (cache_.contains(new_key) && !old_keys.contains(new_key))
        {
            return std::unexpected{error_code::name_conflict};
        }
    }
    std::vector<decltype(cache_)::node_type> cache_nodes;
    cache_nodes.reserve(changes.size());
    for (const auto& [old_key, new_key] : changes)
    {
        auto cache_node{cache_.extract(old_key)};
        assert(cache_node);
        cache_nodes.push_back(std::move(cache_node));
    }
    for (std::size_t i{0}; i < changes.size(); ++i)
    {
        cache_nodes[i].key() = changes[i].second;
        const auto inserted{cache_.insert(std::move(cache_nodes[i]))};
        assert(inserted.inserted);
    }

    node.prop.name = name;
    return {};
}

handle context::get(std::string_view path)
{
    const std::string key{path};
    if (cache_.contains(key))
    {
        const handle target{cache_.at(key)};
        const resource_node& node{resolve(target)};
        switch (node.state)
        {
        case resource_state::none:
        {
            return {};
        }
        case resource_state::deserialized:
        {
            return target;
        }
        case resource_state::placeholder:
        {
            break;
        }
        default:
            assert(false && "Invalid node state");
            return {};
        }
    }

    const std::string resolved_path{resolve_path(path)};
    auto deserialized{deserialize(resolved_path)};
    if (!deserialized || deserialized->properties.empty())
    {
        return {};
    }

    deserialized_data& result{*deserialized};
    auto load_property = [this, &result](std::string_view path, std::uint32_t index) -> handle
    {
        std::vector<std::pair<std::string, std::uint32_t>> pending{{std::string{path}, index}};
        handle root{};
        while (!pending.empty())
        {
            auto [current_path, current_index]{std::move(pending.back())};
            pending.pop_back();
            const property& prop{result.properties.at(current_index)};
            handle current{};
            if (const auto it{cache_.find(current_path)}; it != cache_.end())
            {
                current = it->second;
            }
            else
            {
                current = create(current_path, prop.value);
            }
            assert(current);
            set_value(current, prop.value);
            if (!root)
            {
                root = current;
            }

            const std::vector<std::uint32_t>& children{result.child_indices.at(current_index)};
            for (const std::uint32_t child_index : children | std::views::reverse)
            {
                const property& child_prop{result.properties.at(child_index)};
                pending.emplace_back(
                    std::format("{}{}{}", current_path, delimiter, child_prop.name),
                    child_index
                );
            }
        }
        return root;
    };

    if (!path.ends_with(extension))
    {
        return load_property(path, result.top_level_prop_indices.front());
    }

    const handle root{create(path, std::monostate{})};
    if (!root)
    {
        assert(false && "Failed to create a root property while deserializing");
        return {};
    }

    for (const std::uint32_t index : result.top_level_prop_indices)
    {
        const property& prop{result.properties.at(index)};
        const std::string child_path{std::format("{}{}{}", path, delimiter, prop.name)};
        const handle child{load_property(child_path, index)};
        if (!child)
        {
            return {};
        }
    }
    return root;
}

handle context::get(handle parent, std::string_view path)
{
    assert(is_valid(parent));
    if (path.empty())
    {
        return parent;
    }
    if (path.front() == delimiter)
    {
        path.remove_prefix(1);
    }
    return get(std::format("{}{}{}", get_path(parent), delimiter, path));
}

handle context::get_parent(handle target) const
{
    assert(is_valid(target));
    const resource_node& node{resolve(target)};
    if (!node.parent)
    {
        return {};
    }
    return node.parent;
}

std::vector<handle> context::get_children(handle target) const
{
    assert(is_valid(target));
    const resource_node& node{resolve(target)};
    return node.children;
}

std::string context::get_name(handle target) const
{
    assert(is_valid(target));
    const resource_node& node{resolve(target)};
    return node.prop.name;
}

type context::get_type(handle target) const
{
    assert(is_valid(target));
    const resource_node& node{resolve(target)};
    return std::visit(
        [](const auto& arg)
        {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::monostate>)
            {
                return type::folder;
            }
            else if constexpr (std::is_same_v<T, std::int32_t>)
            {
                return type::int32;
            }
            else if constexpr (std::is_same_v<T, std::int64_t>)
            {
                return type::int64;
            }
            else if constexpr (std::is_same_v<T, float>)
            {
                return type::float32;
            }
            else if constexpr (std::is_same_v<T, std::string>)
            {
                return type::string;
            }
            else if constexpr (std::is_same_v<T, image_asset>)
            {
                return type::image;
            }
            else
            {
                return type::none;
            }
        },
        node.prop.value
    );
}

result<void> context::serialize(const std::filesystem::path& path, handle target)
{
    assert(is_valid(target));

    std::vector<handle> properties;
    std::unordered_map<handle, std::uint32_t> indices;
    std::vector<std::pair<property::value_type, std::streamoff>> values;
    auto add_value = [&values](const property::value_type& value)
    {
        if (!std::ranges::contains(values | std::views::elements<0>, value))
        {
            values.emplace_back(value, 0);
        }
    };
    auto get_value_position = [&values](const property::value_type& value)
    {
        const auto it{std::ranges::find(values, value, &std::pair<property::value_type, std::streamoff>::first)};
        assert(it != values.end());
        return it->second;
    };
    [this, &properties, &indices, &add_value](this auto&& self, handle current) -> void
    {
        const resource_node& node{resolve(current)};
        for (const handle child : node.children)
        {
            const resource_node& childnode{resolve(child)};
            properties.push_back(child);
            indices.emplace(child, static_cast<std::uint32_t>(properties.size() - 1));
            add_value(childnode.prop.name);
            add_value(childnode.prop.value);
            self(child);
        }
    }(target);

    constexpr std::size_t header_section_size = []()
    {
        std::size_t size{0};
        size += sizeof(file::header_data::signature);
        size += sizeof(file::header_data::version);
        return size;
    }();

    const std::size_t metadata_section_size = [this, &properties]()
    {
        std::size_t size{0};
        size += sizeof(std::uint32_t);
        for (const auto& prop : properties)
        {
            size += sizeof(file::hierarchy_entry::name_pos);
            size += sizeof(std::uint32_t);
            size += sizeof(std::uint32_t) * resolve(prop).children.size();
        }
        return size;
    }();

    const std::size_t chunk_section_size = [&properties]()
    {
        std::size_t size{0};
        size += (sizeof(file::chunk_entry::name_pos) + sizeof(file::chunk_entry::value_pos)) * properties.size();
        return size;
    }();

    std::vector<std::byte> binary;
    std::streamoff cursor{
        static_cast<std::streamoff>(header_section_size + metadata_section_size + chunk_section_size)
    };
    for (auto& [value, position] : values)
    {
        std::visit(
            [&binary, &cursor, &value, &position](const auto& arg)
            {
                position = cursor;

                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::monostate>)
                {
                    const std::uint8_t type{static_cast<std::uint8_t>(value.index())};
                    const std::byte* type_data{reinterpret_cast<const std::byte*>(&type)};
                    binary.insert(binary.end(), type_data, type_data + sizeof(type));
                    cursor += sizeof(type);
                }
                else if constexpr (std::is_same_v<T, std::string>)
                {
                    const std::uint8_t type{static_cast<std::uint8_t>(value.index())};
                    const std::byte* type_data{reinterpret_cast<const std::byte*>(&type)};
                    binary.insert(binary.end(), type_data, type_data + sizeof(type));
                    cursor += sizeof(type);

                    const std::uint32_t size{static_cast<std::uint32_t>(arg.size())};
                    const std::byte* size_data{reinterpret_cast<const std::byte*>(&size)};
                    binary.insert(binary.end(), size_data, size_data + sizeof(size));
                    cursor += sizeof(size);

                    const std::byte* data{reinterpret_cast<const std::byte*>(arg.data())};
                    binary.insert(binary.end(), data, data + arg.size());
                    cursor += arg.size();
                }
                else if constexpr (std::is_same_v<T, image_asset>)
                {
                    const std::uint8_t type{static_cast<std::uint8_t>(value.index())};
                    const std::byte* type_data{reinterpret_cast<const std::byte*>(&type)};
                    binary.insert(binary.end(), type_data, type_data + sizeof(type));
                    cursor += sizeof(type);

                    const std::uint8_t format{static_cast<std::uint8_t>(arg.img.format)};
                    const std::byte* format_data{reinterpret_cast<const std::byte*>(&format)};
                    binary.insert(binary.end(), format_data, format_data + sizeof(format));
                    cursor += sizeof(format);

                    const std::byte* width_data{reinterpret_cast<const std::byte*>(&arg.img.width)};
                    binary.insert(binary.end(), width_data, width_data + sizeof(arg.img.width));
                    cursor += sizeof(arg.img.width);

                    const std::byte* height_data{reinterpret_cast<const std::byte*>(&arg.img.height)};
                    binary.insert(binary.end(), height_data, height_data + sizeof(arg.img.height));
                    cursor += sizeof(arg.img.height);

                    const std::uint64_t size{static_cast<std::uint64_t>(arg.data.size())};
                    const std::byte* size_data{reinterpret_cast<const std::byte*>(&size)};
                    binary.insert(binary.end(), size_data, size_data + sizeof(size));
                    cursor += sizeof(size);

                    binary.insert(binary.end(), arg.data.begin(), arg.data.end());
                    cursor += static_cast<std::streamoff>(arg.data.size());
                }
                else
                {
                    const std::uint8_t type{static_cast<std::uint8_t>(value.index())};
                    const std::byte* type_data{reinterpret_cast<const std::byte*>(&type)};
                    binary.insert(binary.end(), type_data, type_data + sizeof(type));
                    cursor += sizeof(type);

                    const std::byte* data{reinterpret_cast<const std::byte*>(&arg)};
                    binary.insert(binary.end(), data, data + sizeof(T));
                    cursor += sizeof(T);
                }
            },
            value
        );
    }

    std::vector<file::hierarchy_entry> hierarchies;
    hierarchies.reserve(properties.size());
    for (const handle current : properties)
    {
        const resource_node& node{resolve(current)};
        file::hierarchy_entry& hierarchy{hierarchies.emplace_back()};
        hierarchy.name_pos = get_value_position(node.prop.name);
        hierarchy.child_indices.reserve(node.children.size());
        for (const handle child : node.children)
        {
            hierarchy.child_indices.push_back(indices.at(child));
        }
    }

    return serializer{}
        .path(path)
        .header({.signature = file::signature, .version = file::version})
        .hierarchies(hierarchies)
        .chunks(
            [this, &properties, &get_value_position]()
            {
                std::vector<file::chunk_entry> chunks;
                chunks.reserve(properties.size());
                for (const handle current : properties)
                {
                    const resource_node& node{resolve(current)};
                    file::chunk_entry chunk{};
                    chunk.name_pos = get_value_position(node.prop.name);
                    chunk.value_pos = get_value_position(node.prop.value);
                    chunks.push_back(chunk);
                }
                return chunks;
            }()
        )
        .binary(binary)
        .execute();
}

bool context::is_valid(handle target) const
{
    if (!nodes_.contains(target))
    {
        return false;
    }
    const resource_node& node{nodes_.get(target)};
    switch (node.state)
    {
    case resource_state::none:
        return false;
    default:
        break;
    }
    return true;
}

std::string context::get_path(handle target) const
{
    std::vector<std::string_view> names;
    handle current{target};
    do
    {
        const resource_node& node{nodes_.get(current)};
        names.emplace_back(node.prop.name);
        current = node.parent;
    } while (current);
    return std::views::reverse(names) | std::views::join_with(delimiter) | std::ranges::to<std::string>();
}

std::string context::resolve_path(std::string_view path) const
{
    if (mount_path_.empty())
    {
        return std::string{path};
    }

    const std::size_t extension_pos{path.rfind(extension)};
    if (extension_pos == std::string_view::npos)
    {
        return std::string{path};
    }

    // 절대 경로는 mount_path_를 붙이지 않고 그대로 반환
    const std::filesystem::path file_path{path.substr(0, extension_pos + extension.size())};
    if (file_path.is_absolute())
    {
        return std::string{path};
    }

    const std::string_view sub_path{path.substr(extension_pos + extension.size())};
    return (mount_path_ / file_path).generic_string() + std::string{sub_path};
}
} // namespace nori::resource
