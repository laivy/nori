#pragma once

#include <nori/core/vector.hpp>

#include "render_target_signature.hpp"

namespace nori::graphics::resource
{
struct render_target_info
{
    render_target_signature signature;
    core::vec2 size;
};
} // namespace nori::graphics::resource
