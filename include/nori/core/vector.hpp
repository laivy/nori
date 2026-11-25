#pragma once

namespace nori::core
{
class vec2
{
public:
    [[nodiscard]] constexpr float length_squared() const noexcept;
    [[nodiscard]] float length() const noexcept;
    [[nodiscard]] vec2 normalized() const noexcept;

    constexpr bool operator==(const vec2&) const noexcept = default;
    constexpr vec2& operator+=(vec2 rhs) noexcept;
    constexpr vec2& operator-=(vec2 rhs) noexcept;
    constexpr vec2& operator*=(float rhs) noexcept;
    constexpr vec2& operator/=(float rhs) noexcept;

    float x;
    float y;
};

class vec3
{
public:
    [[nodiscard]] constexpr float length_squared() const noexcept;
    [[nodiscard]] float length() const noexcept;
    [[nodiscard]] vec3 normalized() const noexcept;

    constexpr bool operator==(const vec3&) const noexcept = default;
    constexpr vec3& operator+=(vec3 rhs) noexcept;
    constexpr vec3& operator-=(vec3 rhs) noexcept;
    constexpr vec3& operator*=(float rhs) noexcept;
    constexpr vec3& operator/=(float rhs) noexcept;

    float x;
    float y;
    float z;
};

class vec4
{
public:
    [[nodiscard]] constexpr float length_squared() const noexcept;
    [[nodiscard]] float length() const noexcept;
    [[nodiscard]] vec4 normalized() const noexcept;

    constexpr bool operator==(const vec4&) const noexcept = default;
    constexpr vec4& operator+=(vec4 rhs) noexcept;
    constexpr vec4& operator-=(vec4 rhs) noexcept;
    constexpr vec4& operator*=(float rhs) noexcept;
    constexpr vec4& operator/=(float rhs) noexcept;

    float x;
    float y;
    float z;
    float w;
};

[[nodiscard]] constexpr vec2 operator+(vec2 value) noexcept
{
    return value;
}

[[nodiscard]] constexpr vec3 operator+(vec3 value) noexcept
{
    return value;
}

[[nodiscard]] constexpr vec4 operator+(vec4 value) noexcept
{
    return value;
}

[[nodiscard]] constexpr vec2 operator-(vec2 value) noexcept
{
    return {-value.x, -value.y};
}

[[nodiscard]] constexpr vec3 operator-(vec3 value) noexcept
{
    return {-value.x, -value.y, -value.z};
}

[[nodiscard]] constexpr vec4 operator-(vec4 value) noexcept
{
    return {-value.x, -value.y, -value.z, -value.w};
}

constexpr vec2& vec2::operator+=(vec2 rhs) noexcept
{
    x += rhs.x;
    y += rhs.y;
    return *this;
}

constexpr vec3& vec3::operator+=(vec3 rhs) noexcept
{
    x += rhs.x;
    y += rhs.y;
    z += rhs.z;
    return *this;
}

constexpr vec4& vec4::operator+=(vec4 rhs) noexcept
{
    x += rhs.x;
    y += rhs.y;
    z += rhs.z;
    w += rhs.w;
    return *this;
}

constexpr vec2& vec2::operator-=(vec2 rhs) noexcept
{
    x -= rhs.x;
    y -= rhs.y;
    return *this;
}

constexpr vec3& vec3::operator-=(vec3 rhs) noexcept
{
    x -= rhs.x;
    y -= rhs.y;
    z -= rhs.z;
    return *this;
}

constexpr vec4& vec4::operator-=(vec4 rhs) noexcept
{
    x -= rhs.x;
    y -= rhs.y;
    z -= rhs.z;
    w -= rhs.w;
    return *this;
}

constexpr vec2& vec2::operator*=(float rhs) noexcept
{
    x *= rhs;
    y *= rhs;
    return *this;
}

constexpr vec3& vec3::operator*=(float rhs) noexcept
{
    x *= rhs;
    y *= rhs;
    z *= rhs;
    return *this;
}

constexpr vec4& vec4::operator*=(float rhs) noexcept
{
    x *= rhs;
    y *= rhs;
    z *= rhs;
    w *= rhs;
    return *this;
}

constexpr vec2& vec2::operator/=(float rhs) noexcept
{
    x /= rhs;
    y /= rhs;
    return *this;
}

constexpr vec3& vec3::operator/=(float rhs) noexcept
{
    x /= rhs;
    y /= rhs;
    z /= rhs;
    return *this;
}

constexpr vec4& vec4::operator/=(float rhs) noexcept
{
    x /= rhs;
    y /= rhs;
    z /= rhs;
    w /= rhs;
    return *this;
}

[[nodiscard]] constexpr vec2 operator+(vec2 lhs, vec2 rhs) noexcept
{
    return lhs += rhs;
}

[[nodiscard]] constexpr vec3 operator+(vec3 lhs, vec3 rhs) noexcept
{
    return lhs += rhs;
}

[[nodiscard]] constexpr vec4 operator+(vec4 lhs, vec4 rhs) noexcept
{
    return lhs += rhs;
}

[[nodiscard]] constexpr vec2 operator-(vec2 lhs, vec2 rhs) noexcept
{
    return lhs -= rhs;
}

[[nodiscard]] constexpr vec3 operator-(vec3 lhs, vec3 rhs) noexcept
{
    return lhs -= rhs;
}

[[nodiscard]] constexpr vec4 operator-(vec4 lhs, vec4 rhs) noexcept
{
    return lhs -= rhs;
}

[[nodiscard]] constexpr vec2 operator*(vec2 lhs, float rhs) noexcept
{
    return lhs *= rhs;
}

[[nodiscard]] constexpr vec3 operator*(vec3 lhs, float rhs) noexcept
{
    return lhs *= rhs;
}

[[nodiscard]] constexpr vec4 operator*(vec4 lhs, float rhs) noexcept
{
    return lhs *= rhs;
}

[[nodiscard]] constexpr vec2 operator*(float lhs, vec2 rhs) noexcept
{
    return rhs *= lhs;
}

[[nodiscard]] constexpr vec3 operator*(float lhs, vec3 rhs) noexcept
{
    return rhs *= lhs;
}

[[nodiscard]] constexpr vec4 operator*(float lhs, vec4 rhs) noexcept
{
    return rhs *= lhs;
}

[[nodiscard]] constexpr vec2 operator/(vec2 lhs, float rhs) noexcept
{
    return lhs /= rhs;
}

[[nodiscard]] constexpr vec3 operator/(vec3 lhs, float rhs) noexcept
{
    return lhs /= rhs;
}

[[nodiscard]] constexpr vec4 operator/(vec4 lhs, float rhs) noexcept
{
    return lhs /= rhs;
}

[[nodiscard]] constexpr float dot(vec2 lhs, vec2 rhs) noexcept
{
    return lhs.x * rhs.x + lhs.y * rhs.y;
}

[[nodiscard]] constexpr float dot(vec3 lhs, vec3 rhs) noexcept
{
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

[[nodiscard]] constexpr float dot(vec4 lhs, vec4 rhs) noexcept
{
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w;
}

[[nodiscard]] constexpr vec3 cross(vec3 lhs, vec3 rhs) noexcept
{
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x,
    };
}

constexpr float vec2::length_squared() const noexcept
{
    return dot(*this, *this);
}

constexpr float vec3::length_squared() const noexcept
{
    return dot(*this, *this);
}

constexpr float vec4::length_squared() const noexcept
{
    return dot(*this, *this);
}
} // namespace nori::core
