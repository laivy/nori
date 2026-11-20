#pragma once
#include <nori/core/vector.hpp>

namespace nori::graphics
{
struct line_draw_desc
{
    core::vec2 start;
    core::vec2 end;
    float width;
    core::vec4 color;
};
} // namespace nori::graphics
