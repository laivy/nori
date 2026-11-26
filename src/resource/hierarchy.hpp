#pragma once
#include <concepts>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <nori/core/slot_map.hpp>
#include <nori/resource/error_code.hpp>
#include <nori/resource/handle.hpp>

#include "loader.hpp"
#include "property.hpp"

namespace nori::resource
{
class hierarchy
{
public:
    hierarchy();

    hierarchy(const hierarchy&) = delete;
    hierarchy& operator=(const hierarchy&) = delete;

    void set_identity(detail::hierarchy_handle identity);
    [[nodiscard]] handle identity_of(detail::node_handle target) const;

    void set_root(detail::node_handle root);
    [[nodiscard]] detail::node_handle root() const;

    void set_source(const std::filesystem::path& file_path);
    [[nodiscard]] loader* source();

    [[nodiscard]] std::shared_lock<std::shared_mutex> lock_nodes_shared() const;
    [[nodiscard]] std::unique_lock<std::shared_mutex> lock_nodes_exclusive();

    detail::node_handle create(property prop);
    detail::node_handle create(std::string_view name, property_type type);
    detail::node_handle create_placeholder(std::string_view name, property_type type);

    void remove(detail::node_handle target);
    void remove_children(detail::node_handle target);

    result<void> attach(detail::node_handle parent, detail::node_handle child);
    result<void> reorder(detail::node_handle target, std::size_t position);
    result<void> rename(detail::node_handle target, std::string_view name);

    template <typename T>
    requires std::assignable_from<property::value_type&, T&&>
    void set(detail::node_handle target, T&& value);

    [[nodiscard]] std::string_view name_of(detail::node_handle target) const;
    [[nodiscard]] property_type type_of(detail::node_handle target) const;
    [[nodiscard]] property::value_type& value_of(detail::node_handle target);
    [[nodiscard]] const property::value_type& value_of(detail::node_handle target) const;

    [[nodiscard]] detail::node_handle parent_of(detail::node_handle target) const;
    [[nodiscard]] std::span<const detail::node_handle> children_of(detail::node_handle target) const;

    [[nodiscard]] detail::node_handle find_child(detail::node_handle parent, std::string_view name) const;

    template <typename T>
    result<T> try_get(detail::node_handle target) const;

    void unload_value(detail::node_handle target);
    void unload_children(detail::node_handle target);
    void reserve_children(detail::node_handle parent, std::size_t count);
    void complete_children(detail::node_handle parent, std::span<const detail::node_handle> order);

    [[nodiscard]] bool is_value_loaded(detail::node_handle target) const;
    [[nodiscard]] bool is_child_list_complete(detail::node_handle target) const;

private:
    class collection
    {
    public:
        collection() noexcept;

        collection(const collection&) = delete;
        collection(collection&& other) noexcept = default;
        collection& operator=(const collection&) = delete;
        collection& operator=(collection&& other) noexcept = default;

        void reserve(std::size_t count);
        bool append(detail::node_handle child, std::string_view name);
        void erase(detail::node_handle child, std::string_view name);
        void clear();
        void unload();
        void complete(std::span<const detail::node_handle> order);

        void reorder(detail::node_handle child, std::size_t position);
        void rename(std::string& current_name, std::string_view new_name);

        [[nodiscard]] detail::node_handle find(std::string_view name) const;

        [[nodiscard]] std::span<const detail::node_handle> handles() const;
        [[nodiscard]] bool is_complete() const;

    private:
        struct data
        {
            std::vector<detail::node_handle> handles;
            std::unordered_map<std::string_view, detail::node_handle> names;
        };

        std::unique_ptr<data> data_;
        bool is_complete_;
    };

    struct node
    {
        std::string name;
        std::variant<property_type, property::value_type> value;

        detail::node_handle parent;
        collection children;
    };

    std::optional<loader> source_;
    detail::hierarchy_handle identity_;
    detail::node_handle root_;
    mutable std::shared_mutex nodes_mutex_;
    core::slot_map<detail::node_handle, std::unique_ptr<node>> nodes_;
};

template <typename T>
requires std::assignable_from<property::value_type&, T&&>
void hierarchy::set(detail::node_handle target, T&& value)
{
    auto& destination{nodes_.at(target)->value};
    if (const auto loaded{std::get_if<property::value_type>(&destination)})
    {
        *loaded = std::forward<T>(value);
    }
    else
    {
        destination.emplace<property::value_type>(std::forward<T>(value));
    }
}

template <typename T>
result<T> hierarchy::try_get(detail::node_handle target) const
{
    const property::value_type& source{value_of(target)};
    if constexpr (std::is_same_v<T, image_view>)
    {
        const auto value{std::get_if<image>(&source)};
        if (!value)
        {
            return std::unexpected{error_code::type_mismatch};
        }
        return image_view{value->format, value->width, value->height, value->mip_levels, value->data};
    }
    else if constexpr (std::is_same_v<T, binary_view>)
    {
        const auto value{std::get_if<binary>(&source)};
        if (!value)
        {
            return std::unexpected{error_code::type_mismatch};
        }
        return binary_view{value->data};
    }
    else
    {
        const auto value{std::get_if<T>(&source)};
        if (!value)
        {
            return std::unexpected{error_code::type_mismatch};
        }
        return *value;
    }
}
} // namespace nori::resource
