#pragma once
#include <nori/core/matrix.hpp>
#include <nori/core/vector.hpp>

namespace nori::core
{
struct transform
{
    vec3 position;
    vec4 rotation;
    vec3 scale;

    static const transform identity;
};

inline constexpr transform transform::identity{
    .position = {0.0f, 0.0f, 0.0f},
    .rotation = {0.0f, 0.0f, 0.0f, 1.0f},
    .scale = {1.0f, 1.0f, 1.0f}
};

[[nodiscard]] vec4 axis_rotation(vec3 axis, float radians);
[[nodiscard]] transform compose_transform(const transform& parent, const transform& relative);
[[nodiscard]] transform relative_to(const transform& world, const transform& parent);
[[nodiscard]] mat4 transform_matrix(const transform& pose);
} // namespace nori::core
