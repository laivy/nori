#include "imgui_context.hpp"
#ifndef IMGUI_DISABLE

namespace
{
nori::graphics::imgui::context active_context{};
} // namespace

namespace nori::graphics::imgui
{
void set_current_context(const context& context)
{
    active_context = context;
}

const context& current_context()
{
    return active_context;
}
} // namespace nori::graphics::imgui
#endif
