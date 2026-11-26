#pragma once

#include <cstdint>

#include <nori/core/vector.hpp>

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
    float friction;
    float restitution;
    float density;
    collision_filter filter;
    bool is_sensor;
};
}
