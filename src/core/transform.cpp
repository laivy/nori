#include <cmath>

#include <nori/core/transform.hpp>

namespace
{
nori::core::vec3 product(nori::core::vec3 a, nori::core::vec3 b)
{
    return {a.x * b.x, a.y * b.y, a.z * b.z};
}

nori::core::vec3 quotient(nori::core::vec3 a, nori::core::vec3 b)
{
    return {a.x / b.x, a.y / b.y, a.z / b.z};
}

nori::core::vec4 product(nori::core::vec4 a, nori::core::vec4 b)
{
    const nori::core::vec3 x{a.x, a.y, a.z};
    const nori::core::vec3 y{b.x, b.y, b.z};
    const auto v{x * b.w + y * a.w + cross(x, y)};
    return {v.x, v.y, v.z, a.w * b.w - x.x * y.x - x.y * y.y - x.z * y.z};
}

nori::core::vec3 rotate(nori::core::vec4 rotation, nori::core::vec3 v)
{
    const nori::core::vec3 axis{rotation.x, rotation.y, rotation.z};
    const auto offset{cross(axis, v) * 2.0f};
    return v + offset * rotation.w + cross(axis, offset);
}
} // namespace

namespace nori::core
{
vec4 axis_rotation(vec3 axis, float radians)
{
    const auto sine{std::sin(radians * 0.5f)};
    return {axis.x * sine, axis.y * sine, axis.z * sine, std::cos(radians * 0.5f)};
}

transform compose_transform(const transform& parent, const transform& relative)
{
    return {
        parent.position + rotate(parent.rotation, product(parent.scale, relative.position)),
        product(parent.rotation, relative.rotation),
        product(parent.scale, relative.scale)
    };
}

transform relative_to(const transform& world, const transform& parent)
{
    const vec4 inverse{-parent.rotation.x, -parent.rotation.y, -parent.rotation.z, parent.rotation.w};
    return {
        quotient(rotate(inverse, world.position - parent.position), parent.scale),
        product(inverse, world.rotation),
        quotient(world.scale, parent.scale)
    };
}

mat4 transform_matrix(const transform& pose)
{
    const auto x{rotate(pose.rotation, {pose.scale.x, 0.0f, 0.0f})};
    const auto y{rotate(pose.rotation, {0.0f, pose.scale.y, 0.0f})};
    const auto z{rotate(pose.rotation, {0.0f, 0.0f, pose.scale.z})};
    return {
        x.x, x.y, x.z, 0.0f,
        y.x, y.y, y.z, 0.0f,
        z.x, z.y, z.z, 0.0f,
        pose.position.x, pose.position.y, pose.position.z, 1.0f
    };
}
} // namespace nori::core
