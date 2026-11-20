#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_win32.h>
#include <imgui.h>

#include "config.hpp"
#include "descriptor_allocator.hpp"
#include "imgui_context.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);

namespace nori::graphics::imgui
{
context::~context() noexcept
{
    ::ImGui_ImplDX12_Shutdown();
    ::ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

std::unique_ptr<context> context::create(params args)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    if (!::ImGui_ImplWin32_Init(args.hwnd))
    {
        ImGui::DestroyContext();
        return nullptr;
    }

    ImGui_ImplDX12_InitInfo init_info{};
    init_info.Device = args.device;
    init_info.CommandQueue = args.queue;
    init_info.NumFramesInFlight = graphics::frames_in_flight;
    init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    init_info.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    init_info.UserData = args.descriptors;
    init_info.SrvDescriptorHeap = args.descriptors->heap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* cpu, D3D12_GPU_DESCRIPTOR_HANDLE* gpu)
    {
        const auto allocator{static_cast<descriptor_allocator*>(info->UserData)};
        const descriptor_handle handle{allocator->allocate(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)};
        IM_ASSERT(handle.cpu.ptr != 0);
        *cpu = handle.cpu;
        *gpu = handle.gpu;
    };
    init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE)
    {
        const auto allocator{static_cast<descriptor_allocator*>(info->UserData)};
        allocator->free(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, cpu);
    };
    ::ImGui_ImplDX12_Init(&init_info);
    return std::unique_ptr<context>{new context};
}

bool context::process_window_message(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param)
{
    return ::ImGui_ImplWin32_WndProcHandler(hwnd, message, w_param, l_param) != 0;
}

void context::begin_frame(core::vec2 render_target_size)
{
    render_target_size_ = render_target_size;
    ::ImGui_ImplDX12_NewFrame();
    ::ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void context::end_frame(ID3D12GraphicsCommandList* command_list)
{
    ImGui::Render();
    const auto data{ImGui::GetDrawData()};
    const ImVec2 framebuffer_scale{data->FramebufferScale};
    if (data->DisplaySize.x > 0.0f && data->DisplaySize.y > 0.0f)
    {
        data->FramebufferScale = {render_target_size_.x / data->DisplaySize.x, render_target_size_.y / data->DisplaySize.y};
    }
    ::ImGui_ImplDX12_RenderDrawData(data, command_list);
    data->FramebufferScale = framebuffer_scale;
    is_rendered_ = true;
}

bool context::render_platform_windows()
{
    if (!is_rendered_)
    {
        return false;
    }
    is_rendered_ = false;
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
    return true;
}

context::context() :
    is_rendered_{false},
    render_target_size_{}
{
}
} // namespace nori::graphics::imgui
