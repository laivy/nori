#ifndef IMGUI_DISABLE
#include <imgui.h>
#include "context.hpp"
#include "imgui_accessor.hpp"
#include "imgui_context.hpp"
#include "renderer.hpp"

namespace nori::graphics::imgui
{
result<void> initialize(HWND hwnd)
{
    if (nori::graphics::context* const value{nori::graphics::context::instance()})
    {
        return value->initialize_imgui(hwnd);
    }
    return std::unexpected{error_code::not_initialized};
}

result<void> render()
{
    if (nori::graphics::context* const value{nori::graphics::context::instance()})
    {
        return value->render_imgui();
    }
    return std::unexpected{error_code::not_initialized};
}

void image(resource::handle handle, core::float2 size, core::float2 uv0, core::float2 uv1)
{
    const ImVec2 imgui_size{size.x, size.y};
    renderer* renderer_instance{current_context().renderer_instance};
    if (!handle || !renderer_instance)
    {
        ImGui::Dummy(imgui_size);
        return;
    }
    const D3D12_GPU_DESCRIPTOR_HANDLE texture_srv{renderer_instance->resolve_texture_srv(handle)};
    if (texture_srv.ptr == 0)
    {
        ImGui::Dummy(imgui_size);
        return;
    }
    ImGui::Image(
        static_cast<ImTextureID>(texture_srv.ptr),
        imgui_size,
        ImVec2{uv0.x, uv0.y},
        ImVec2{uv1.x, uv1.y}
    );
}

void invalidate_image(resource::handle handle)
{
    renderer* renderer_instance{current_context().renderer_instance};
    if (!handle || !renderer_instance)
    {
        return;
    }
    renderer_instance->release_texture(handle);
}
} // namespace nori::graphics::imgui
#endif
