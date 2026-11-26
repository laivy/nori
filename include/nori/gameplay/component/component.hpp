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
    static constexpr bool is_unique{false};

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
    virtual void on_register();
    virtual void on_unregister();

    virtual void on_component_removed(const component& comp) noexcept;

    template <typename Self, typename Function>
    requires(std::is_member_function_pointer_v<Function> && std::invocable<Function, Self&, float>)
    update_handle register_update(this Self& receiver, update_group group, Function function)
    {
        auto& owner{static_cast<component&>(receiver)};
        return owner.registration_scheduler().add(owner, receiver, group, function);
    }

private:
    void register_component();
    void unregister_component();
    update_scheduler& registration_scheduler();

    object& owner_;
    bool registered_{};
};
} // namespace nori::gameplay
