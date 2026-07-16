#pragma once
#ifndef IMGUI_DISABLE

namespace nori::graphics
{
class renderer;
}

namespace nori::graphics::imgui
{
struct context
{
    renderer* renderer_instance{};
};

void set_current_context(const context& context);
const context& current_context();
} // namespace nori::graphics::imgui
#endif
