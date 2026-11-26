#pragma once

#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <nori/core/vector.hpp>
#include <nori/physics2d/body.hpp>
#include <nori/physics2d/collider.hpp>

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

    [[nodiscard]] body_handle create_body(const body_desc& desc, std::span<const box_collider_config> colliders = {});
    bool destroy_body(body_handle body);
    [[nodiscard]] bool contains(body_handle body) const noexcept;

    bool add_box_collider(const box_collider_desc& desc);
    [[nodiscard]] std::vector<contact_point> contacts_of(body_handle body) const;
    [[nodiscard]] std::optional<ray_hit> cast_ray(core::vec2 origin, core::vec2 translation, collision_filter filter = default_collision_filter) const;
    [[nodiscard]] std::span<const contact_event> contact_events() const;

    [[nodiscard]] std::optional<body_transform> transform_of(body_handle body) const;
    bool set_transform(body_handle body, body_transform transform);
    [[nodiscard]] std::optional<body_velocity> velocity_of(body_handle body) const;
    bool set_velocity(body_handle body, body_velocity velocity);

    bool apply_force(body_handle body, core::vec2 force);
    bool apply_linear_impulse(body_handle body, core::vec2 impulse);

private:
    class impl;
    std::unique_ptr<impl> impl_;
};
} // namespace nori::physics2d
