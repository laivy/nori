#pragma once

#include <nori/core/vector.hpp>
#include <nori/physics2d/collider_handle.hpp>

namespace nori::physics2d
{
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
    // Contact pairs are unordered; for sensor events first is the sensor and second is the visitor.
    // End events may refer to colliders whose bodies have already been destroyed.
    collider_handle first;
    collider_handle second;
    contact_type type;
    contact_phase phase;
};
} // namespace nori::physics2d
