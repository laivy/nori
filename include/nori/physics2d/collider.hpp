#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include <nori/core/vector.hpp>
#include <nori/physics2d/body.hpp>

namespace nori::physics2d
{
struct collision_filter
{
    std::uint64_t category_bits;
    std::uint64_t mask_bits;
};

inline constexpr collision_filter default_collision_filter{1, ~std::uint64_t{0}};

struct box_collider_settings
{
    float density;
    float friction;
    float restitution;
    bool is_sensor;
    collision_filter filter;
};

inline constexpr box_collider_settings default_box_collider_settings{1.0f, 0.6f, 0.0f, false, default_collision_filter};

struct box_collider_desc
{
    body_handle body;
    core::vec2 half_extent;
    core::vec2 center;
    std::optional<box_collider_settings> settings;
};

struct box_collider_config
{
    core::vec2 half_extent;
    core::vec2 center;
    std::optional<box_collider_settings> settings;
};

struct collider_ref
{
    body_handle body;
    std::size_t index;
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
    collider_ref first;
    collider_ref second;
    contact_kind kind;
    contact_phase phase;
};

struct ray_hit
{
    collider_ref collider;
    core::vec2 point;
    core::vec2 normal;
    float fraction;
};
} // namespace nori::physics2d
