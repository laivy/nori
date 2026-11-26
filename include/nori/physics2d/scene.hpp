#pragma once

#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <nori/core/vector.hpp>
#include <nori/physics2d/body.hpp>
#include <nori/physics2d/body_handle.hpp>
#include <nori/physics2d/collider.hpp>
#include <nori/physics2d/collider_handle.hpp>
#include <nori/physics2d/contact.hpp>
#include <nori/physics2d/query.hpp>

namespace nori::physics2d
{
class scene
{
public:
    scene();
    ~scene();

    scene(const scene&) = delete;
    scene(scene&&) = delete;
    scene& operator=(const scene&) = delete;
    scene& operator=(scene&&) = delete;

    void step(float delta_seconds);
    bool set_gravity(core::vec2 gravity);

    // Returns an invalid handle on failure.
    [[nodiscard]] body_handle create_body(const body_desc& desc);
    bool destroy_body(body_handle body);
    [[nodiscard]] bool contains(body_handle body) const noexcept;

    // Returns an invalid handle on failure. Colliders live until their owning body is destroyed.
    [[nodiscard]] collider_handle add_box_collider(body_handle body, const box_collider_desc& desc);
    [[nodiscard]] bool contains(collider_handle collider) const noexcept;
    // Returns the contact manifold data from the physics simulation, not a live overlap query.
    [[nodiscard]] std::vector<contact_point> contacts_of(body_handle body) const;
    // Returns the closest hit along origin + translation, if any.
    [[nodiscard]] std::optional<ray_hit> cast_ray_closest(core::vec2 origin, core::vec2 translation, collision_filter filter = default_collision_filter) const;
    // The returned view is valid until the next successful step or scene destruction.
    [[nodiscard]] std::span<const contact_event> contact_events() const;

    [[nodiscard]] std::optional<body_transform> transform_of(body_handle body) const;
    // Moves the body and wakes it and its touching bodies where applicable.
    // Contact manifolds are refreshed by a subsequent simulation step.
    bool set_transform(body_handle body, body_transform transform);
    [[nodiscard]] std::optional<body_velocity> velocity_of(body_handle body) const;
    bool set_velocity(body_handle body, body_velocity velocity);

    bool apply_force_to_center(body_handle body, core::vec2 force);
    bool apply_linear_impulse_to_center(body_handle body, core::vec2 impulse);

private:
    class impl;
    std::unique_ptr<impl> impl_;
};
} // namespace nori::physics2d
