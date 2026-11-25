#pragma once
#include <cassert>
#include <concepts>
#include <cstddef>
#include <filesystem>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nori/core/slot_map.hpp>
#include <nori/resource/error_code.hpp>
#include <nori/resource/handle.hpp>
#include <nori/resource/path.hpp>
#include <nori/resource/value.hpp>

#include "file_registry.hpp"
#include "hierarchy.hpp"
#include "loader.hpp"

namespace nori::resource
{
class context
{
public:
    explicit context(std::filesystem::path root_path);

    handle create_root();
    handle create(handle parent, property prop);
    result<handle> copy(handle source, handle destination);
    result<handle> move(handle source, handle destination);
    void remove(handle target);

    result<void> reorder(handle target, std::size_t position);
    result<void> rename(handle target, std::string_view name);

    template <typename T>
    requires std::assignable_from<property::value_type&, T&&>
    void set(handle target, T&& value)
    {
        hierarchy& hier{hierarchy_of(target.hier)};
        assert(target.node != hier.root());
        hier.set(target.node, std::forward<T>(value));
    }

    [[nodiscard]] std::string name_of(handle target) const;
    [[nodiscard]] path path_of(handle target) const;
    [[nodiscard]] property_type type_of(handle target) const;

    result<handle> try_get(const path& target);
    result<handle> try_get(handle parent, const path& subpath);
    [[nodiscard]] handle root_of(handle target) const;
    [[nodiscard]] handle parent_of(handle target) const;
    result<std::vector<handle>> children_of(handle target);
    result<std::size_t> position_of(handle target);

    template <typename T>
    result<T> try_get(handle target)
    {
        const hierarchy& hier{hierarchy_of(target.hier)};
        assert(target.node != hier.root());
        if (const auto res{load_value(target)}; !res)
        {
            return std::unexpected{res.error()};
        }
        std::shared_lock lock{hier.lock_nodes_shared()};
        return hier.try_get<T>(target.node);
    }

    result<void> save(handle target);
    result<void> save_as(handle target, const std::filesystem::path& file_path);

    result<void> load(handle target);
    result<void> unload(handle target);

private:
    [[nodiscard]] decltype(auto) hierarchy_of(this auto& self, detail::hierarchy_handle owner)
    {
        std::lock_guard lock{self.hierarchies_mutex_};
        return self.hierarchies_.at(owner);
    }

    result<handle> copy_subtree(handle source, handle destination);
    result<handle> move_subtree(handle source, handle destination);
    result<void> load_value(handle target);
    result<void> load_child_list(handle target);

    result<loader::reader> reader_of(handle target);
    loader::reader::cursor cursor_for(loader::reader& reader, handle target);

    file_registry files_;
    mutable std::mutex hierarchies_mutex_;
    core::slot_map<detail::hierarchy_handle, hierarchy> hierarchies_;
};
} // namespace nori::resource
