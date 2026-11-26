#pragma once

#include <nori/core/vector.hpp>
#include <nori/physics2d/collider_handle.hpp>

namespace nori::physics2d
{
struct contact_point
{
    collider_handle self;
    collider_handle other;
    core::vec2 point;
    core::vec2 normal;
    core::vec2 relative_velocity;
    float separation;
    float normal_impulse;
    float total_normal_impulse;
};

enum class contact_type
{
    contact,
    sensor
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
    contact_type type;
    contact_phase phase;
};
} // namespace nori::physics2d
