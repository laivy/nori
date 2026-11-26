#pragma once
#include <concepts>
#include <type_traits>

#include <nori/core/rtti.hpp>
#include <nori/gameplay/update/update_group.hpp>
#include <nori/gameplay/update/update_handle.hpp>
#include <nori/gameplay/update/update_scheduler.hpp>

namespace nori::gameplay
{
class object;

class component : public core::rtti<component>
{
    friend class object;

public:
    explicit component(object& owner);
    virtual ~component() = default;

    component(const component&) = delete;
    component& operator=(const component&) = delete;
    component(component&&) = delete;
    component& operator=(component&&) = delete;

    [[nodiscard]] object& owner();
    [[nodiscard]] const object& owner() const;
    [[nodiscard]] bool is_registered() const noexcept;

protected:
    virtual void on_register()
    {
    }

    virtual void on_unregister()
    {
    }

    virtual void on_component_removed(const component& value) noexcept;

    template <typename Self, typename Function>
    requires(std::is_member_function_pointer_v<Function> && std::invocable<Function, Self&, float>)
    update_handle register_update(this Self& instance, update_group group, Function function)
    {
        auto& owner{static_cast<component&>(instance)};
        return owner.registration_scheduler().register_update(owner, instance, group, function);
    }

    bool unregister_update(update_handle handle);
    bool set_update_enabled(update_handle handle, bool enabled);
    [[nodiscard]] bool is_update_enabled(update_handle handle) const;
    bool add_update_prerequisite(update_handle target, update_handle prerequisite);
    bool remove_update_prerequisite(update_handle target, update_handle prerequisite);

private:
    void register_component();
    void unregister_component();
    [[nodiscard]] update_scheduler* active_scheduler() const;
    update_scheduler& registration_scheduler();

    object& owner_;
    bool registered_{};
};
} // namespace nori::gameplay
