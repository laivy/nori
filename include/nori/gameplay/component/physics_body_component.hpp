#pragma once
#include <optional>
#include <vector>

#include <nori/gameplay/component/component.hpp>
#include <nori/physics2d/body.hpp>
#include <nori/physics2d/body_handle.hpp>
#include <nori/physics2d/collider.hpp>
#include <nori/physics2d/contact.hpp>

namespace nori::gameplay
{
struct physics_body_config
{
    physics2d::body_type type;
    physics2d::body_velocity velocity;
    bool lock_rotation;
    std::optional<float> mass_override;
    std::vector<physics2d::box_collider_desc> colliders;
};

class physics_body_component final : public core::rtti<physics_body_component, component>
{
public:
    physics_body_component(object& owner, physics_body_config config);

    [[nodiscard]] physics2d::body_handle body_handle() const;
    [[nodiscard]] update_handle pre_physics_update() const;
    [[nodiscard]] std::optional<physics2d::body_velocity> velocity() const;
    bool set_velocity(physics2d::body_velocity value);
    // Returns physics manifold data; pending or in-progress teleports suppress contacts.
    [[nodiscard]] std::vector<physics2d::contact_point> contacts() const;
    bool teleport(physics2d::body_transform pose, bool preserve_velocity = false);

protected:
    void on_register() override;
    void on_unregister() override;

private:
    struct teleport_request
    {
        physics2d::body_transform pose;
        bool preserve_velocity;
    };

    void on_pre_physics_update(float delta_seconds);
    void on_post_physics_update(float delta_seconds);
    void apply_pending_teleport();

    physics_body_config config_;
    physics2d::body_handle body_{};
    update_handle pre_physics_update_{};
    std::optional<teleport_request> pending_teleport_;
    physics2d::body_transform static_pose_{};
    bool teleported_{};
};
} // namespace nori::gameplay
