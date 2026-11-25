#include <algorithm>
#include <cassert>
#include <ranges>
#include <utility>

#include "hierarchy.hpp"

namespace nori::resource
{
hierarchy::hierarchy() :
    identity_{},
    root_{}
{
}

void hierarchy::set_identity(detail::hierarchy_handle identity)
{
    identity_ = identity;
}

handle hierarchy::identity_of(detail::node_handle target) const
{
    return target ? handle{identity_, target} : handle{};
}

void hierarchy::set_root(detail::node_handle root)
{
    root_ = root;
}

detail::node_handle hierarchy::root() const
{
    return root_;
}

void hierarchy::set_source(const std::filesystem::path& file_path)
{
    source_.emplace(file_path);
}

loader* hierarchy::source()
{
    return source_ ? &*source_ : nullptr;
}

std::shared_lock<std::shared_mutex> hierarchy::lock_nodes_shared() const
{
    return std::shared_lock{nodes_mutex_};
}

std::unique_lock<std::shared_mutex> hierarchy::lock_nodes_exclusive()
{
    return std::unique_lock{nodes_mutex_};
}

detail::node_handle hierarchy::create(property prop)
{
    return nodes_.emplace(std::make_unique<node>(node{
        .name = std::move(prop.name),
        .value = std::move(prop.value),
    }));
}

detail::node_handle hierarchy::create(std::string_view name, property_type type)
{
    return nodes_.emplace(std::make_unique<node>(node{
        .name = std::string{name},
        .value = type,
    }));
}

detail::node_handle hierarchy::create_placeholder(std::string_view name, property_type type)
{
    const detail::node_handle created{create(name, type)};
    nodes_.at(created)->children.unload();
    return created;
}

void hierarchy::remove(detail::node_handle target)
{
    remove_children(target);
    if (const detail::node_handle parent{parent_of(target)})
    {
        nodes_.at(parent)->children.erase(target, name_of(target));
    }
    nodes_.erase(target);
}

void hierarchy::remove_children(detail::node_handle target)
{
    std::vector<detail::node_handle> subtree{std::from_range, children_of(target)};
    for (std::size_t i{0}; i < subtree.size(); ++i)
    {
        subtree.append_range(children_of(subtree[i]));
    }
    nodes_.at(target)->children.clear();
    for (const auto descendant : subtree)
    {
        nodes_.at(descendant)->children.clear();
    }
    for (const auto descendant : subtree | std::views::reverse)
    {
        nodes_.erase(descendant);
    }
}

result<void> hierarchy::attach(detail::node_handle parent, detail::node_handle child)
{
    auto& child_node{*nodes_.at(child)};
    if (parent == child_node.parent)
    {
        return {};
    }

    if (parent == child)
    {
        return std::unexpected{error_code::invalid_relationship};
    }

    if (!child_node.children.handles().empty())
    {
        for (detail::node_handle current{parent}; current; current = parent_of(current))
        {
            if (current == child)
            {
                return std::unexpected{error_code::invalid_relationship};
            }
        }
    }

    if (!nodes_.at(parent)->children.append(child, child_node.name))
    {
        return std::unexpected{error_code::name_conflict};
    }

    if (child_node.parent)
    {
        nodes_.at(child_node.parent)->children.erase(child, child_node.name);
    }

    child_node.parent = parent;
    return {};
}

result<void> hierarchy::reorder(detail::node_handle target, std::size_t position)
{
    const detail::node_handle parent{parent_of(target)};
    if (!parent)
    {
        return std::unexpected{error_code::invalid_relationship};
    }

    nodes_.at(parent)->children.reorder(target, position);
    return {};
}

result<void> hierarchy::rename(detail::node_handle target, std::string_view name)
{
    assert(target != root_);
    std::string& current_name{nodes_.at(target)->name};
    if (current_name == name)
    {
        return {};
    }
    if (const detail::node_handle parent{parent_of(target)})
    {
        if (find_child(parent, name))
        {
            return std::unexpected{error_code::name_conflict};
        }
        nodes_.at(parent)->children.rename(current_name, name);
    }
    else
    {
        current_name = name;
    }
    return {};
}

std::string_view hierarchy::name_of(detail::node_handle target) const
{
    assert(target != root_);
    return nodes_.at(target)->name;
}

property_type hierarchy::type_of(detail::node_handle target) const
{
    assert(target != root_);
    const auto& value{nodes_.at(target)->value};
    if (const auto type{std::get_if<property_type>(&value)})
    {
        return *type;
    }
    return nori::resource::type_of(std::get<property::value_type>(value));
}

property::value_type& hierarchy::value_of(detail::node_handle target)
{
    return std::get<property::value_type>(nodes_.at(target)->value);
}

const property::value_type& hierarchy::value_of(detail::node_handle target) const
{
    return std::get<property::value_type>(nodes_.at(target)->value);
}

detail::node_handle hierarchy::parent_of(detail::node_handle target) const
{
    return nodes_.at(target)->parent;
}

std::span<const detail::node_handle> hierarchy::children_of(detail::node_handle target) const
{
    return nodes_.at(target)->children.handles();
}

detail::node_handle hierarchy::find_child(detail::node_handle parent, std::string_view name) const
{
    return nodes_.at(parent)->children.find(name);
}

void hierarchy::unload_value(detail::node_handle target)
{
    const property_type type{type_of(target)};
    nodes_.at(target)->value = type;
}

void hierarchy::unload_children(detail::node_handle target)
{
    remove_children(target);
    nodes_.at(target)->children.unload();
}

void hierarchy::reserve_children(detail::node_handle parent, std::size_t count)
{
    nodes_.at(parent)->children.reserve(count);
}

void hierarchy::complete_children(detail::node_handle parent, std::span<const detail::node_handle> order)
{
    nodes_.at(parent)->children.complete(order);
}

bool hierarchy::is_value_loaded(detail::node_handle target) const
{
    return std::holds_alternative<property::value_type>(nodes_.at(target)->value);
}

bool hierarchy::is_child_list_complete(detail::node_handle target) const
{
    return nodes_.at(target)->children.is_complete();
}

hierarchy::collection::collection() noexcept :
    is_complete_{true}
{
}

void hierarchy::collection::reserve(std::size_t count)
{
    assert(count > 0);
    if (!data_)
    {
        data_ = std::make_unique<data>();
    }
    data_->handles.reserve(count);
    data_->names.reserve(count);
}

bool hierarchy::collection::append(detail::node_handle child, std::string_view name)
{
    if (!data_)
    {
        data_ = std::make_unique<data>();
    }

    if (!data_->names.try_emplace(name, child).second)
    {
        return false;
    }
    data_->handles.push_back(child);
    return true;
}

void hierarchy::collection::erase(detail::node_handle child, std::string_view name)
{
    data_->names.erase(name);
    std::erase(data_->handles, child);
    if (data_->handles.empty())
    {
        data_.reset();
    }
}

void hierarchy::collection::clear()
{
    data_.reset();
    is_complete_ = true;
}

void hierarchy::collection::unload()
{
    data_.reset();
    is_complete_ = false;
}

void hierarchy::collection::complete(std::span<const detail::node_handle> order)
{
    assert(std::ranges::is_permutation(order, handles()));
    if (data_ && order.data() != data_->handles.data())
    {
        std::ranges::copy(order, data_->handles.begin());
    }
    is_complete_ = true;
}

void hierarchy::collection::reorder(detail::node_handle child, std::size_t position)
{
    std::vector<detail::node_handle>& handles{data_->handles};
    const auto it{std::ranges::find(handles, child)};
    assert(it != handles.end());

    const std::size_t current{static_cast<std::size_t>(it - handles.begin())};
    if (current < position)
    {
        std::rotate(it, it + 1, handles.begin() + static_cast<std::ptrdiff_t>(position) + 1);
    }
    else if (current > position)
    {
        std::rotate(handles.begin() + static_cast<std::ptrdiff_t>(position), it, it + 1);
    }
}

void hierarchy::collection::rename(std::string& current_name, std::string_view new_name)
{
    const auto it{data_->names.find(current_name)};
    assert(it != data_->names.end());
    const detail::node_handle child{it->second};
    data_->names.erase(it);
    current_name = new_name;
    data_->names.emplace(current_name, child);
}

detail::node_handle hierarchy::collection::find(std::string_view name) const
{
    if (!data_)
    {
        return {};
    }
    const auto it{data_->names.find(name)};
    if (it == data_->names.end())
    {
        return {};
    }
    return it->second;
}

std::span<const detail::node_handle> hierarchy::collection::handles() const
{
    if (!data_)
    {
        return {};
    }
    return data_->handles;
}

bool hierarchy::collection::is_complete() const
{
    return is_complete_;
}
} // namespace nori::resource
