#pragma once

#include <optional>
#include <span>

#include <nori/core/vector.hpp>
#include <nori/physics2d/body_handle.hpp>
#include <nori/physics2d/collider.hpp>

namespace nori::physics2d
{
enum class body_type
{
    static_body,
    kinematic_body,
    dynamic_body
};

struct body_transform
{
    core::vec2 position;
    float angle_radians;
};

struct body_velocity
{
    core::vec2 linear;
    float angular_radians_per_second;
};

struct body_desc
{
    body_type type;
    body_transform transform;
    body_velocity velocity;
    bool lock_rotation;
    std::optional<float> mass_override;
    std::span<const box_collider_desc> colliders;
};
} // namespace nori::physics2d
