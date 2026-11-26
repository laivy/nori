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

    // The receiver must outlive all registered executions, including the current snapshot.
    // Unregister its updates before destroying the receiver.
    template <typename T, typename Function>
    requires(std::is_member_function_pointer_v<Function> && std::invocable<Function, T&, float>)
    update_handle register_update(T& instance, update_group group, Function function)
    {
        return register_update(instance, instance, group, function);
    }

    template <typename Owner, typename Receiver, typename Function>
    requires(std::is_member_function_pointer_v<Function> && std::invocable<Function, Receiver&, float>)
    update_handle register_update(Owner& owner, Receiver& receiver, update_group group, Function function)
    {
        if (!function)
        {
            core::abort("cannot register a null update function");
        }
        return insert({.owner = std::addressof(owner), .callback = std::make_shared<callback_type>([pointer = std::addressof(receiver), function](float delta_seconds)
        {
            std::invoke(function, *pointer, delta_seconds);
        }),
            .group = group});
    }

    template <typename T>
    std::size_t unregister_updates(T& instance)
    {
        return unregister_updates_for_owner(std::addressof(instance));
    }

    template <typename T>
    [[nodiscard]] bool owns_update(T& instance, update_handle handle) const
    {
        return owns_update_for_owner(std::addressof(instance), handle);
    }

    void execute_lifecycle(std::move_only_function<void()> function);

    bool unregister_update(update_handle handle);
    bool set_update_enabled(update_handle handle, bool enabled);
    [[nodiscard]] bool is_update_enabled(update_handle handle) const;

    bool add_prerequisite(update_handle target, update_handle prerequisite);
    bool remove_prerequisite(update_handle target, update_handle prerequisite);

    void execute(update_category category, float delta_seconds);
    [[nodiscard]] bool is_executing() const noexcept;
    [[nodiscard]] bool is_update_in_snapshot(update_handle handle) const;

private:
    using callback_type = std::move_only_function<void(float)>;

    struct update_function
    {
        const void* owner;
        std::shared_ptr<callback_type> callback;
        update_group group;
        bool enabled{true};
        std::vector<std::uint64_t> prerequisites{};
    };

    using function_map = std::map<std::uint64_t, update_function>;

    std::size_t unregister_updates_for_owner(const void* owner);
    [[nodiscard]] bool owns_update_for_owner(const void* owner, update_handle handle) const;
    update_handle insert(update_function function);
    function_map& edit_functions();
    void commit_changes();
    [[nodiscard]] update_function* find(update_handle handle);
    [[nodiscard]] const update_function* find(update_handle handle) const;
    [[nodiscard]] static std::vector<std::uint64_t> execution_order(const function_map& functions, update_category category);

    std::uint64_t id_;
    std::uint64_t next_id_{1};
    function_map functions_;
    std::optional<function_map> pending_functions_;
    bool executing_{};
    std::size_t lifecycle_depth_{};
};
} // namespace nori::gameplay
