#include <memory>
#include <utility>

#include <nori/core/abort.hpp>
#include <nori/graphics/imgui.hpp>

#include "context.hpp"
#include "imgui_context.hpp"
#include "resource/texture_manager.hpp"

namespace
{
std::unique_ptr<nori::graphics::imgui::context> instance;
} // namespace

namespace nori::graphics::imgui
{
bool initialize(HWND hwnd)
{
    const auto active{ctx()};
    const auto commands{active->queue()};
    if (!commands->wait_idle())
    {
        return false;
    }
    commands->collect();
    auto created{context::create({hwnd, active->device(), commands->native(), active->descriptors()})};
    if (!created)
    {
        return false;
    }
    instance = std::move(created);
    return true;
}

void shutdown() noexcept
{
    if (!instance)
    {
        return;
    }
    const auto commands{ctx()->queue()};
    if (!commands->wait_idle())
    {
        core::abort("imgui shutdown gpu synchronization failed");
    }
    commands->collect();
    instance.reset();
}

bool process_window_message(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param)
{
    return instance->process_window_message(hwnd, message, w_param, l_param);
}

bool begin_render_pass(const graphics::render_pass_desc& desc)
{
    return ctx()->begin_imgui_render_pass(*instance, desc);
}

bool end_render_pass()
{
    return ctx()->end_imgui_render_pass(*instance);
}

ImTextureID texture_id(texture_handle handle) noexcept
{
    return ctx()->textures()->srv(handle).ptr;
}

bool render_platform_windows()
{
    return instance && instance->render_platform_windows();
}
} // namespace nori::graphics::imgui
