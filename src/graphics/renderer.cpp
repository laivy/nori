#include <array>
#include <cassert>
#include <cstdint>
#include <utility>
#include <directx/d3dx12.h>
#ifndef NDEBUG
#include <dxgidebug.h>
#endif

#ifndef IMGUI_DISABLE
#include <backends/imgui_impl_dx12.h>
#include <imgui.h>
#include "imgui_context.hpp"
#endif

#include "gpu_timeline.hpp"
#include "pipeline_manager.hpp"
#include "render_assets.hpp"
#include "render_target_manager.hpp"
#include "renderer.hpp"
#include "swap_chain_manager.hpp"
#include "swap_chain_render_target.hpp"
#include "upload_manager.hpp"

namespace nori::graphics
{
renderer::renderer() :
    frame_contexts_{},
    frame_index_{0},
    shutdown_{false},
    shutdown_succeeded_{true}
{
    initialize();
#ifndef IMGUI_DISABLE
    imgui_initialized_ = false;
#endif
}

renderer::~renderer() noexcept
{
    shutdown();
}

bool renderer::shutdown()
{
    if (shutdown_)
    {
        return shutdown_succeeded_;
    }

    shutdown_succeeded_ = wait_idle();
    collect_completed_work();
#ifndef IMGUI_DISABLE
    shutdown_imgui();
#endif
    assets_.reset();
    pipeline_manager_.reset();
    upload_manager_.reset();
    swap_chain_manager_.reset();
    render_target_manager_.reset();
    descriptor_allocator_.reset();
    timeline_.reset();
    command_list_.Reset();
    for (frame_context& frame : frame_contexts_)
    {
        frame.command_allocator.Reset();
        frame.fence_value = 0;
    }
    command_queue_.Reset();
    device_.Reset();
    dxgi_factory_.Reset();
    shutdown_ = true;
    return shutdown_succeeded_;
}

result<swap_chain_handle> renderer::create_swap_chain(swap_chain_specification spec)
{
    return swap_chain_manager_->create(spec);
}

result<void> renderer::resize_swap_chain(swap_chain_handle handle, std::uint32_t width, std::uint32_t height)
{
    if (!wait_idle())
    {
        return std::unexpected{error_code::synchronization_failed};
    }
    return swap_chain_manager_->resize(handle, width, height);
}

result<render_target_handle> renderer::create_render_target(const render_target_specification& spec)
{
    return render_target_manager_->create_target(spec);
}

result<render_texture_handle> renderer::create_render_texture(render_texture_specification spec)
{
    return render_target_manager_->create_texture(spec);
}

result<pipeline_handle> renderer::create_graphics_pipeline(const graphics_pipeline_specification& spec)
{
    return pipeline_manager_->create_pipeline(spec);
}

result<shader_handle> renderer::create_shader(shader_specification spec)
{
    return pipeline_manager_->create_shader(spec);
}

result<void> renderer::destroy(swap_chain_handle handle)
{
    return swap_chain_manager_->destroy(handle);
}

result<void> renderer::destroy(render_target_handle handle)
{
    return render_target_manager_->destroy(handle);
}

result<void> renderer::destroy(render_texture_handle handle)
{
    return render_target_manager_->destroy(handle);
}

result<void> renderer::destroy(pipeline_handle handle)
{
    return pipeline_manager_->destroy(handle);
}

result<void> renderer::destroy(shader_handle handle)
{
    return pipeline_manager_->destroy(handle);
}

result<void> renderer::begin_frame()
{
    frame_context& frame{frame_contexts_.at(frame_index_)};
    if (frame.fence_value != 0 && frame.fence_value > timeline_->completed_value() && !timeline_->wait(frame.fence_value))
    {
        return std::unexpected{error_code::synchronization_failed};
    }
    collect_completed_work();
    if (FAILED(frame.command_allocator->Reset()))
    {
        return std::unexpected{error_code::command_failed};
    }
    if (FAILED(command_list_->Reset(frame.command_allocator.Get(), nullptr)))
    {
        return std::unexpected{error_code::command_failed};
    }
    const std::array<ID3D12DescriptorHeap*, 2> descriptor_heaps{
        descriptor_allocator_->heap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
        descriptor_allocator_->heap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
    };
    command_list_->SetDescriptorHeaps(static_cast<UINT>(descriptor_heaps.size()), descriptor_heaps.data());
    return {};
}

result<void> renderer::begin_pass(swap_chain_handle target)
{
    active_render_target_.reset();
    active_color_formats_ = {render_texture_format::rgba8_unorm};
    return swap_chain_manager_->begin(target, command_list_.Get());
}

result<void> renderer::begin_pass(render_target_handle target)
{
    if (auto result{render_target_manager_->begin(target, command_list_.Get())}; !result)
    {
        return result;
    }
    active_render_target_ = target;
    auto formats{render_target_manager_->color_formats(target)};
    if (!formats)
    {
        active_render_target_.reset();
        return std::unexpected{formats.error()};
    }
    active_color_formats_ = std::move(*formats);
    return {};
}

result<void> renderer::record(const draw_list& list)
{
    for (const fullscreen_draw& draw : list.fullscreen_draws())
    {
        if (auto result{render_fullscreen(draw)}; !result)
        {
            return result;
        }
    }
    for (const model& model : list.models())
    {
        render_model(model);
    }
    for (const quad& quad : list.quads())
    {
        render_quad(quad);
    }
    return {};
}

result<void> renderer::end_pass(swap_chain_handle target)
{
    auto result{swap_chain_manager_->end(target, command_list_.Get())};
    active_color_formats_.clear();
    return result;
}

result<void> renderer::end_pass(render_target_handle target)
{
    auto result{render_target_manager_->end(target, command_list_.Get())};
    active_render_target_.reset();
    active_color_formats_.clear();
    return result;
}

result<void> renderer::end_frame()
{
    if (FAILED(command_list_->Close()))
    {
        return std::unexpected{error_code::command_failed};
    }
    const auto command_lists{std::to_array<ID3D12CommandList*>({command_list_.Get()})};
    command_queue_->ExecuteCommandLists(static_cast<UINT>(command_lists.size()), command_lists.data());
    std::uint64_t fence_value{};
    if (!timeline_->signal(fence_value))
    {
        return std::unexpected{error_code::synchronization_failed};
    }
    frame_contexts_.at(frame_index_).fence_value = fence_value;
    frame_index_ = (frame_index_ + 1) % frames_in_flight;
    return {};
}

result<void> renderer::present(swap_chain_handle handle)
{
    return swap_chain_manager_->present(handle);
}

D3D12_GPU_DESCRIPTOR_HANDLE renderer::resolve_texture_srv(resource::handle handle)
{
    return assets_->resolve_texture_srv(handle);
}

void renderer::release_texture(resource::handle handle)
{
    assets_->release_texture(handle);
}

void renderer::begin_pass(swap_chain_render_target& target, const viewport& viewport)
{
    target.begin(command_list_.Get(), viewport);
}

void renderer::end_pass(swap_chain_render_target& target)
{
    target.end(command_list_.Get());
}

void renderer::render_model(const model& model)
{
    std::ignore = model;
}

void renderer::render_quad(const quad& quad)
{
    std::ignore = quad;
}

result<void> renderer::render_fullscreen(const fullscreen_draw& draw)
{
    if (active_render_target_ && render_target_manager_->is_attachment(*active_render_target_, draw.source))
    {
        return std::unexpected{error_code::invalid_state};
    }
    auto descriptor{render_target_manager_->shader_resource(draw.source)};
    if (!descriptor)
    {
        return std::unexpected{descriptor.error()};
    }
    return pipeline_manager_->bind_fullscreen(draw.pipeline, active_color_formats_, *descriptor, command_list_.Get());
}

void renderer::initialize()
{
    if (!create_dxgi_factory())
    {
        assert(false && "failed to create DXGI factory");
    }
    if (!create_d3d12_device())
    {
        assert(false && "failed to create D3D12 device");
    }
    if (!create_command_objects())
    {
        assert(false && "failed to create command objects");
    }
    create_subsystems();
}

void renderer::collect_completed_work()
{
    if (upload_manager_)
    {
        upload_manager_->collect();
    }
    if (timeline_)
    {
        timeline_->collect();
    }
}

bool renderer::create_dxgi_factory()
{
    UINT flags{0};
#ifndef NDEBUG
    Microsoft::WRL::ComPtr<ID3D12Debug> debug_controller;
    if (SUCCEEDED(::D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller))))
    {
        debug_controller->EnableDebugLayer();
        flags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif
    if (FAILED(::CreateDXGIFactory2(flags, IID_PPV_ARGS(&dxgi_factory_))))
    {
        return false;
    }
    return true;
}

bool renderer::create_d3d12_device()
{
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    for (UINT i{0}; dxgi_factory_->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i)
    {
        DXGI_ADAPTER_DESC1 desc{};
        adapter->GetDesc1(&desc);
        if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
        {
            continue;
        }
        if (SUCCEEDED(::D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_))))
        {
            return true;
        }
    }
    if (FAILED(dxgi_factory_->EnumWarpAdapter(IID_PPV_ARGS(&adapter))))
    {
        return false;
    }
    if (FAILED(::D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_))))
    {
        return false;
    }
    return true;
}

bool renderer::create_command_objects()
{
    D3D12_COMMAND_QUEUE_DESC desc{};
    if (FAILED(device_->CreateCommandQueue(&desc, IID_PPV_ARGS(&command_queue_))))
    {
        return false;
    }
    for (frame_context& frame : frame_contexts_)
    {
        if (FAILED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frame.command_allocator))))
        {
            return false;
        }
    }
    if (FAILED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frame_contexts_.front().command_allocator.Get(), nullptr, IID_PPV_ARGS(&command_list_))))
    {
        return false;
    }
    if (FAILED(command_list_->Close()))
    {
        return false;
    }
    return true;
}

void renderer::create_subsystems()
{
    timeline_ = std::make_unique<gpu_timeline>(device_.Get(), command_queue_.Get());
    upload_manager_ = std::make_unique<upload_manager>(device_.Get(), command_queue_.Get(), timeline_.get());
    descriptor_allocator_ = std::make_unique<descriptor_allocator>(device_.Get());
    swap_chain_manager_ = std::make_unique<swap_chain_manager>(dxgi_factory_.Get(), device_.Get(), command_queue_.Get(), descriptor_allocator_.get(), timeline_.get());
    render_target_manager_ = std::make_unique<render_target_manager>(device_.Get(), descriptor_allocator_.get(), timeline_.get());
    assets_ = std::make_unique<render_assets>(device_.Get(), descriptor_allocator_.get(), upload_manager_.get(), timeline_.get());
    pipeline_manager_ = std::make_unique<pipeline_manager>(device_.Get(), timeline_.get());
}

bool renderer::wait_idle()
{
    assert(timeline_);
    return timeline_->wait_idle();
}

#ifndef IMGUI_DISABLE
result<void> renderer::initialize_imgui()
{
    if (imgui_initialized_)
    {
        return {};
    }
    IMGUI_CHECKVERSION();
    ImGui_ImplDX12_InitInfo info{};
    info.Device = device_.Get();
    info.CommandQueue = command_queue_.Get();
    info.NumFramesInFlight = frames_in_flight;
    info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    info.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    info.UserData = this;
    info.SrvDescriptorHeap = descriptor_allocator_->heap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* init_info, D3D12_CPU_DESCRIPTOR_HANDLE* cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* gpu_handle)
    {
        auto self{static_cast<renderer*>(init_info->UserData)};
        const auto handle{self->descriptor_allocator_->allocate(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)};
        *cpu_handle = handle.cpu;
        *gpu_handle = handle.gpu;
    };
    info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* init_info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE)
    {
        auto self{static_cast<renderer*>(init_info->UserData)};
        self->descriptor_allocator_->free(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, cpu_handle);
    };
    if (!::ImGui_ImplDX12_Init(&info))
    {
        return std::unexpected{error_code::backend_initialization_failed};
    }
    imgui_initialized_ = true;
    return {};
}

result<void> renderer::begin_imgui()
{
    assert(imgui_initialized_);
    ::ImGui_ImplDX12_NewFrame();
    return {};
}

result<void> renderer::render_imgui()
{
    assert(imgui_initialized_);
    ::ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), command_list_.Get());
    return {};
}

void renderer::shutdown_imgui() noexcept
{
    if (!imgui_initialized_)
    {
        return;
    }
    ::ImGui_ImplDX12_Shutdown();
    imgui_initialized_ = false;
}

#endif
} // namespace nori::graphics
