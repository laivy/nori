#pragma once
#include <nori/core/vector.hpp>

namespace nori::graphics
{
struct rect_draw_desc
{
    core::vec2 position;
    core::vec2 size;
    core::vec4 color;
};
} // namespace nori::graphics
