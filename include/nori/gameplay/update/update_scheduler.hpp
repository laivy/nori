#pragma once
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include <nori/core/abort.hpp>
#include <nori/gameplay/update/update_category.hpp>
#include <nori/gameplay/update/update_group.hpp>
#include <nori/gameplay/update/update_handle.hpp>

namespace nori::gameplay
{
class update_scheduler
{
public:
    update_scheduler();

    update_scheduler(const update_scheduler&) = delete;
    update_scheduler& operator=(const update_scheduler&) = delete;

    template <typename T, typename Function>
    requires(std::is_member_function_pointer_v<Function> && std::invocable<Function, T&, float>)
    update_handle add(T& receiver, update_group group, Function function)
    {
        return add(receiver, receiver, group, function);
    }

    template <typename Owner, typename Receiver, typename Function>
    requires(std::is_member_function_pointer_v<Function> && std::invocable<Function, Receiver&, float>)
    update_handle add(Owner& owner, Receiver& receiver, update_group group, Function function)
    {
        if (!function)
        {
            core::abort("cannot register a null update function");
        }

        update_function entry{
            .owner = std::addressof(owner),
            .group = group,
            .callback = [&receiver, function](float delta_seconds)
            {
                std::invoke(function, receiver, delta_seconds);
            },
            .enabled = true,
        };
        return insert(std::move(entry));
    }

    template <typename T>
    std::size_t remove_all(T& owner)
    {
        return remove_all(static_cast<const void*>(std::addressof(owner)));
    }

    void execute_lifecycle(std::move_only_function<void()> callback);

    bool remove(update_handle handle);
    bool set_enabled(update_handle handle, bool enabled);
    [[nodiscard]] bool is_enabled(update_handle handle) const;

    bool add_prerequisite(update_handle target, update_handle prerequisite);
    bool remove_prerequisite(update_handle target, update_handle prerequisite);

    void execute(update_category category, float delta_seconds);
    [[nodiscard]] bool is_executing() const noexcept;

private:
    struct update_function
    {
        const void* owner;
        update_group group;
        std::function<void(float)> callback;

        bool enabled;
        std::vector<std::uint64_t> prerequisites;
    };

    using function_map = std::map<std::uint64_t, update_function>;

    function_map& edit_functions();
    update_handle insert(update_function function);
    [[nodiscard]] const function_map& latest_functions() const;
    [[nodiscard]] const update_function* find(const function_map& functions, update_handle handle) const;
    std::size_t remove_all(const void* owner);
    [[nodiscard]] static std::vector<std::uint64_t> execution_order(const function_map& functions, update_category category);

    std::uint64_t id_;
    std::uint64_t next_id_;
    function_map functions_;
    std::optional<function_map> pending_functions_;
    bool executing_;
    std::size_t lifecycle_depth_;
};
} // namespace nori::gameplay
