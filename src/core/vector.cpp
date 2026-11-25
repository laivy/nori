#include <cmath>

#include <nori/core/vector.hpp>

namespace nori::core
{
float vec2::length() const noexcept
{
    return std::sqrt(length_squared());
}

vec2 vec2::normalized() const noexcept
{
    const float len{length()};
    return len == 0.0f ? vec2{} : *this / len;
}

float vec3::length() const noexcept
{
    return std::sqrt(length_squared());
}

vec3 vec3::normalized() const noexcept
{
    const float len{length()};
    return len == 0.0f ? vec3{} : *this / len;
}

float vec4::length() const noexcept
{
    return std::sqrt(length_squared());
}

vec4 vec4::normalized() const noexcept
{
    const float len{length()};
    return len == 0.0f ? vec4{} : *this / len;
}
} // namespace nori::core
