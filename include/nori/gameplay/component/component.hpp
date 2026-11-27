#pragma once
#include <nori/core/rtti.hpp>

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

    virtual void on_fixed_update(float delta_seconds);
    virtual void on_update(float delta_seconds);
    virtual void on_post_update(float delta_seconds);

    virtual void on_component_removed(const component& comp) noexcept;

private:
    object& owner_;
    bool registered_{};
};
} // namespace nori::gameplay
