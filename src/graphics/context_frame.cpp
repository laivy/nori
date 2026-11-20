#include "command_manager.hpp"
#include "context.hpp"
#include "rendering/renderer.hpp"
#include "resource/buffer_manager.hpp"
#include "resource/swap_chain_manager.hpp"

#ifdef NORI_ENABLE_IMGUI
#include "imgui/imgui_context.hpp"
#endif

namespace nori::graphics
{
bool context::begin_frame()
{
    if (!swap_chains_->wait_for_present_slots())
    {
        return false;
    }
    if (!queue_->begin_frame())
    {
        return false;
    }
    buffers_->begin_frame();
    return true;
}

render_pass_handle context::begin_render_pass(const render_pass_desc& desc)
{
    return commands_->begin_render_pass(desc);
}

bool context::end_render_pass(render_pass_handle handle)
{
    const auto [desc, buffers]{commands_->end_render_pass(handle)};
    return renderer_->render(desc, buffers);
}

#ifdef NORI_ENABLE_IMGUI
bool context::begin_imgui_render_pass(imgui::context& ui, const render_pass_desc& desc)
{
    const resource::render_target_info info{renderer_->make_render_target_info(desc)};
    renderer_->begin_rendering(desc, info);
    ui.begin_frame(info.size);
    return true;
}

bool context::end_imgui_render_pass(imgui::context& ui)
{
    ui.end_frame(renderer_->command_list());
    renderer_->end_rendering();
    return true;
}
#endif

bool context::end_frame()
{
    commands_->end_frame();
    const auto queue{queue_.get()};
    if (!queue->submit())
    {
        return false;
    }
#ifdef NORI_ENABLE_IMGUI
    if (imgui::render_platform_windows() && queue->signal() == 0)
    {
        return false;
    }
#endif
    queue->end_frame();
    return true;
}

bool context::present(swap_chain_handle handle)
{
    return queue_->submit() && swap_chains_->present(handle);
}

std::uint64_t context::submit()
{
    return queue_->submit().value_or(0);
}

bool context::poll(std::uint64_t fence_value)
{
    return queue_->poll(fence_value);
}

void context::wait(std::uint64_t fence_value)
{
    if (queue_->wait(fence_value))
    {
        queue_->collect();
    }
}
} // namespace nori::graphics
