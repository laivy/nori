#pragma once

#include <cstddef>
#include <cstdint>

#include <nori/core/vector.hpp>
#include <nori/physics2d/body_handle.hpp>

namespace nori::physics2d
{
struct collision_filter
{
    std::uint64_t category_bits;
    std::uint64_t mask_bits;
};

inline constexpr collision_filter default_collision_filter{1, ~std::uint64_t{0}};

struct box_collider_desc
{
    core::vec2 half_extent;
    core::vec2 center;
    float density;
    float friction;
    float restitution;
    bool is_sensor;
    collision_filter filter;
};

struct collider_handle
{
    [[nodiscard]] bool operator==(const collider_handle&) const noexcept = default;

    body_handle body;
    std::size_t index;
};

struct contact_point
{
    collider_handle self;
    collider_handle other;
    // World-space contact anchor on self.
    core::vec2 point;
    // Unit normal pointing from other to self.
    core::vec2 normal;
    // Current point velocity of self minus that of other.
    core::vec2 relative_velocity;
    // Negative for penetration; speculative contacts can have positive separation.
    float separation;
    float normal_impulse;
    // Accumulated normal impulse across substeps, including restitution.
    float total_normal_impulse;
};

enum class contact_kind
{
    contact,
    trigger
};
enum class contact_phase
{
    begin,
    end
};

struct contact_event
{
    collider_handle first;
    collider_handle second;
    contact_kind kind;
    contact_phase phase;
};

struct ray_hit
{
    collider_handle collider;
    core::vec2 point;
    core::vec2 normal;
    float fraction;
};
} // namespace nori::physics2d
