#include <cassert>
#ifndef IMGUI_DISABLE
#include <backends/imgui_impl_win32.h>
#include <imgui.h>
#include "imgui_context.hpp"
#endif
#include "context.hpp"

namespace nori::graphics
{
context::context() :
    frame_active_{false},
    pass_active_{false},
    shutdown_{false},
    shutdown_succeeded_{true}
{
#ifndef IMGUI_DISABLE
    imgui_initialized_ = false;
    imgui_rendered_ = false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io{ImGui::GetIO()};
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;
    if (const auto result{renderer_.initialize_imgui()}; !result)
    {
        assert(false && "failed to initialize imgui");
    }
    imgui::set_current_context(imgui::context{.renderer_instance = &renderer_});
#endif
}

context::~context() noexcept
{
    shutdown();
}

bool context::shutdown()
{
    if (shutdown_)
    {
        return shutdown_succeeded_;
    }

    shutdown_succeeded_ = renderer_.shutdown();
#ifndef IMGUI_DISABLE
    imgui::set_current_context({});
    if (imgui_initialized_)
    {
        ::ImGui_ImplWin32_Shutdown();
        imgui_initialized_ = false;
    }
    if (ImGui::GetCurrentContext())
    {
        ImGui::DestroyContext();
    }
#endif
    active_swap_chain_.reset();
    active_render_target_.reset();
    frame_active_ = false;
    pass_active_ = false;
    shutdown_ = true;
    return shutdown_succeeded_;
}

result<swap_chain_handle> context::create_swap_chain(swap_chain_specification spec)
{
    if (frame_active_)
    {
        return std::unexpected{error_code::invalid_state};
    }
    return renderer_.create_swap_chain(spec);
}

result<void> context::resize_swap_chain(swap_chain_handle handle, std::uint32_t width, std::uint32_t height)
{
    if (frame_active_)
    {
        return std::unexpected{error_code::invalid_state};
    }
    return renderer_.resize_swap_chain(handle, width, height);
}

result<render_target_handle> context::create_render_target(const render_target_specification& spec)
{
    if (frame_active_)
    {
        return std::unexpected{error_code::invalid_state};
    }
    return renderer_.create_render_target(spec);
}

result<render_texture_handle> context::create_render_texture(render_texture_specification spec)
{
    if (frame_active_)
    {
        return std::unexpected{error_code::invalid_state};
    }
    return renderer_.create_render_texture(spec);
}

result<pipeline_handle> context::create_graphics_pipeline(const graphics_pipeline_specification& spec)
{
    if (frame_active_)
    {
        return std::unexpected{error_code::invalid_state};
    }
    return renderer_.create_graphics_pipeline(spec);
}

result<shader_handle> context::create_shader(shader_specification spec)
{
    if (frame_active_)
    {
        return std::unexpected{error_code::invalid_state};
    }
    return renderer_.create_shader(spec);
}

result<void> context::destroy(swap_chain_handle handle)
{
    if (frame_active_ || (active_swap_chain_ && *active_swap_chain_ == handle))
    {
        return std::unexpected{error_code::resource_in_use};
    }
    return renderer_.destroy(handle);
}

result<void> context::destroy(render_target_handle handle)
{
    if (frame_active_)
    {
        return std::unexpected{error_code::invalid_state};
    }
    return renderer_.destroy(handle);
}

result<void> context::destroy(render_texture_handle handle)
{
    if (frame_active_)
    {
        return std::unexpected{error_code::invalid_state};
    }
    return renderer_.destroy(handle);
}

result<void> context::destroy(pipeline_handle handle)
{
    if (frame_active_)
    {
        return std::unexpected{error_code::invalid_state};
    }
    return renderer_.destroy(handle);
}

result<void> context::destroy(shader_handle handle)
{
    if (frame_active_)
    {
        return std::unexpected{error_code::invalid_state};
    }
    return renderer_.destroy(handle);
}

result<void> context::begin_frame()
{
    if (frame_active_)
    {
        return std::unexpected{error_code::invalid_state};
    }
    if (auto result{renderer_.begin_frame()}; !result)
    {
        return result;
    }
#ifndef IMGUI_DISABLE
    if (imgui_initialized_)
    {
        if (auto result{renderer_.begin_imgui()}; !result)
        {
            return result;
        }
        ::ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }
    imgui_rendered_ = false;
#endif
    frame_active_ = true;
    return {};
}

result<draw_list> context::begin_pass(swap_chain_handle target)
{
    if (!frame_active_ || pass_active_)
    {
        return std::unexpected{error_code::invalid_state};
    }
    if (auto result{renderer_.begin_pass(target)}; !result)
    {
        return std::unexpected{result.error()};
    }
    active_swap_chain_ = target;
    pass_active_ = true;
    return {};
}

result<draw_list> context::begin_pass(render_target_handle target)
{
    if (!frame_active_ || pass_active_)
    {
        return std::unexpected{error_code::invalid_state};
    }
    if (auto result{renderer_.begin_pass(target)}; !result)
    {
        return std::unexpected{result.error()};
    }
    active_render_target_ = target;
    pass_active_ = true;
    return {};
}

result<void> context::end_pass(const draw_list& list)
{
    if (!pass_active_)
    {
        return std::unexpected{error_code::invalid_state};
    }
    if (auto result{renderer_.record(list)}; !result)
    {
        return result;
    }
#ifndef IMGUI_DISABLE
    if (active_swap_chain_ && imgui_rendered_)
    {
        if (auto result{renderer_.render_imgui()}; !result)
        {
            return result;
        }
    }
#endif
    if (active_swap_chain_)
    {
        if (auto result{renderer_.end_pass(*active_swap_chain_)}; !result)
        {
            return result;
        }
    }
    else
    {
        if (auto result{renderer_.end_pass(*active_render_target_)}; !result)
        {
            return result;
        }
    }
    active_swap_chain_.reset();
    active_render_target_.reset();
    pass_active_ = false;
    return {};
}

result<void> context::end_frame()
{
    if (!frame_active_ || pass_active_)
    {
        return std::unexpected{error_code::invalid_state};
    }
#ifndef IMGUI_DISABLE
    if (imgui_initialized_ && !imgui_rendered_)
    {
        ImGui::EndFrame();
    }
#endif
    if (auto result{renderer_.end_frame()}; !result)
    {
        return result;
    }
    frame_active_ = false;
    return {};
}

result<void> context::present(swap_chain_handle handle)
{
    if (frame_active_)
    {
        return std::unexpected{error_code::invalid_state};
    }
    return renderer_.present(handle);
}

#ifndef IMGUI_DISABLE
result<void> context::initialize_imgui(HWND hwnd)
{
    if (!hwnd)
    {
        return std::unexpected{error_code::invalid_argument};
    }
    if (imgui_initialized_)
    {
        return std::unexpected{error_code::invalid_state};
    }
    if (!::ImGui_ImplWin32_Init(hwnd))
    {
        return std::unexpected{error_code::backend_initialization_failed};
    }
    imgui_initialized_ = true;
    return {};
}

result<void> context::render_imgui()
{
    if (!imgui_initialized_ || !frame_active_ || !pass_active_ || !active_swap_chain_)
    {
        return std::unexpected{error_code::invalid_state};
    }
    ImGui::Render();
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
    imgui_rendered_ = true;
    return {};
}
#endif
} // namespace nori::graphics
