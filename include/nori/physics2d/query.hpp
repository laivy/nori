#pragma once

#include <nori/core/vector.hpp>
#include <nori/physics2d/collider_handle.hpp>

namespace nori::physics2d
{
struct ray_hit
{
    collider_handle collider;
    core::vec2 point;
    core::vec2 normal;
    float fraction;
};
}
