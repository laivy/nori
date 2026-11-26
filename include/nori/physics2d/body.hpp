#pragma once

#include <cstdint>
#include <optional>

#include <nori/core/handle.hpp>
#include <nori/core/vector.hpp>

namespace nori::physics2d::detail
{
struct body_tag;
}

namespace nori::physics2d
{
struct body_handle
{
    [[nodiscard]] bool operator==(const body_handle&) const noexcept = default;

    std::uint64_t scene_id;
    core::basic_handle<detail::body_tag> slot;
};

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
};
} // namespace nori::physics2d
