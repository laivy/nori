#pragma once
#include <cassert>
#include <concepts>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include <nori/core/singleton.hpp>
#include <nori/core/slot_map.hpp>
#include <nori/resource/error_code.hpp>
#include <nori/resource/handle.hpp>
#include "runtime.hpp"
#include "format.hpp"
#include "path.hpp"
#include "type.hpp"

namespace nori::resource
{
class context : public core::singleton<context>
{
private:
    enum class resource_state
    {
        none,
        deserialized,
        placeholder
    };

    struct resource_node
    {
        resource_state state;
        property prop;
        handle parent;
        std::vector<handle> children;
    };

public:
    explicit context(runtime::specification spec);

    handle create(std::string_view path, const property::value_type& value);
    void remove(handle target);

    result<void> set_parent(handle child, handle parent);
    result<void> set_name(handle target, std::string_view name);

    template <typename T>
    requires std::assignable_from<property::value_type&, T&&>
    void set_value(handle target, T&& value)
    {
        resource_node& node{resolve(target)};
        node.prop.value = std::forward<T>(value);
    }

    handle get(std::string_view path);
    handle get(handle parent, std::string_view path);
    handle get_parent(handle target) const;
    std::vector<handle> get_children(handle target) const;

    std::string get_name(handle target) const;
    type get_type(handle target) const;

    template <typename T>
    const T& get_value(handle target) const
    {
        const resource_node& node{resolve(target)};
        if constexpr (std::is_same_v<T, image>)
        {
            const image_asset* value{std::get_if<image_asset>(&node.prop.value)};
            return value->img;
        }
        else
        {
            const T* value{std::get_if<T>(&node.prop.value)};
            return *value;
        }
    }

    result<void> serialize(const std::filesystem::path& path, handle target);

private:
    bool is_valid(handle target) const;

    decltype(auto) resolve(this auto& self, handle target)
    {
        assert(self.is_valid(target));
        return (self.nodes_.get(target));
    }

    std::string get_path(handle target) const;
    std::string resolve_path(std::string_view path) const;

private:
    std::filesystem::path mount_path_;
    core::slot_map<handle, resource_node> nodes_;
    std::unordered_map<std::string, handle> cache_;
};
} // namespace nori::resource
