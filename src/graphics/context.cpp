#include <utility>

#include <directx/d3d12.h>
#include <dxgi1_6.h>

#include <nori/core/abort.hpp>

#include "command_manager.hpp"
#include "context.hpp"
#include "descriptor_allocator.hpp"
#include "rendering/renderer.hpp"
#include "resource/buffer_manager.hpp"
#include "resource/pipeline_manager.hpp"
#include "resource/sampler_manager.hpp"
#include "resource/swap_chain_manager.hpp"
#include "resource/texture_manager.hpp"
#include "text/manager.hpp"
#include "transfer/manager.hpp"
#include "upload_manager.hpp"

namespace
{
Microsoft::WRL::ComPtr<IDXGIFactory4> create_dxgi_factory()
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
    Microsoft::WRL::ComPtr<IDXGIFactory4> dxgi_factory;
    if (FAILED(::CreateDXGIFactory2(flags, IID_PPV_ARGS(&dxgi_factory))))
    {
        return {};
    }
    return dxgi_factory;
}

Microsoft::WRL::ComPtr<ID3D12Device> create_device(IDXGIFactory4* dxgi_factory)
{
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    for (std::uint32_t index{0}; dxgi_factory->EnumAdapters1(index, &adapter) != DXGI_ERROR_NOT_FOUND; ++index)
    {
        DXGI_ADAPTER_DESC1 desc{};
        adapter->GetDesc1(&desc);
        Microsoft::WRL::ComPtr<ID3D12Device> device;
        if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
            SUCCEEDED(::D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))))
        {
            return device;
        }
    }
    if (FAILED(dxgi_factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))))
    {
        return {};
    }
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    if (FAILED(::D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))))
    {
        return {};
    }
    return device;
}
} // namespace

namespace nori::graphics
{
struct context::params
{
    Microsoft::WRL::ComPtr<IDXGIFactory4> dxgi_factory;
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    std::unique_ptr<descriptor_allocator> descriptors;
    std::unique_ptr<command_queue> queue;
    std::unique_ptr<upload_manager> uploads;

    std::unique_ptr<resource::buffer_manager> buffers;
    std::unique_ptr<resource::texture_manager> textures;
    std::unique_ptr<resource::sampler_manager> samplers;
    std::unique_ptr<resource::pipeline_manager> pipelines;
    std::unique_ptr<resource::swap_chain_manager> swap_chains;

    std::unique_ptr<transfer::manager> transfers;
    std::unique_ptr<text::manager> texts;
    std::unique_ptr<command_manager> commands;
    std::unique_ptr<rendering::renderer> graphics_renderer;
};

context::~context() noexcept
{
    commands_.reset();
    queue_->discard();
    if (!queue_->wait_idle())
    {
        core::abort("gpu shutdown synchronization failed");
    }
}

std::unique_ptr<context> context::create()
{
    auto dxgi_factory{create_dxgi_factory()};
    if (!dxgi_factory)
    {
        return nullptr;
    }
    auto device{create_device(dxgi_factory.Get())};
    if (!device)
    {
        return nullptr;
    }
    auto descriptors{descriptor_allocator::create(device.Get())};
    if (!descriptors)
    {
        return nullptr;
    }
    auto queue{command_queue::create(device.Get())};
    if (!queue)
    {
        return nullptr;
    }
    auto uploads{std::make_unique<upload_manager>(upload_manager::params{device.Get(), queue.get()})};

    auto buffers{std::make_unique<resource::buffer_manager>(resource::buffer_manager::params{
        .device = device.Get(),
        .uploads = uploads.get(),
        .queue = queue.get(),
    })};
    auto textures{std::make_unique<resource::texture_manager>(resource::texture_manager::params{
        .device = device.Get(),
        .descriptors = descriptors.get(),
        .uploads = uploads.get(),
        .queue = queue.get(),
    })};
    auto samplers{std::make_unique<resource::sampler_manager>(resource::sampler_manager::params{
        .device = device.Get(),
        .descriptors = descriptors.get(),
        .queue = queue.get(),
    })};
    auto pipelines{std::make_unique<resource::pipeline_manager>(resource::pipeline_manager::params{
        .device = device.Get(),
        .queue = queue.get(),
    })};
    auto swap_chains{std::make_unique<resource::swap_chain_manager>(resource::swap_chain_manager::params{
        .dxgi_factory = dxgi_factory.Get(),
        .queue = queue.get(),
        .textures = textures.get(),
    })};

    auto transfers{std::make_unique<transfer::manager>(transfer::manager::params{
        .device = device.Get(),
        .descriptors = descriptors.get(),
        .queue = queue.get(),
        .buffers = buffers.get(),
        .textures = textures.get(),
    })};
    auto texts{text::manager::create({
        .buffers = buffers.get(),
        .textures = textures.get(),
        .samplers = samplers.get(),
        .pipelines = pipelines.get(),
    })};
    if (!texts)
    {
        return nullptr;
    }
    auto commands{std::make_unique<command_manager>()};
    auto graphics_renderer{std::make_unique<rendering::renderer>(rendering::renderer::params{
        .descriptors = descriptors.get(),
        .queue = queue.get(),
        .textures = textures.get(),
        .pipelines = pipelines.get(),
        .buffers = buffers.get(),
        .samplers = samplers.get(),
        .texts = texts.get(),
    })};
    return std::unique_ptr<context>{new context{params{
        .dxgi_factory = std::move(dxgi_factory),
        .device = std::move(device),
        .descriptors = std::move(descriptors),
        .queue = std::move(queue),
        .uploads = std::move(uploads),
        .buffers = std::move(buffers),
        .textures = std::move(textures),
        .samplers = std::move(samplers),
        .pipelines = std::move(pipelines),
        .swap_chains = std::move(swap_chains),
        .transfers = std::move(transfers),
        .texts = std::move(texts),
        .commands = std::move(commands),
        .graphics_renderer = std::move(graphics_renderer),
    }}};
}

ID3D12Device* context::device() const noexcept
{
    return device_.Get();
}

descriptor_allocator* context::descriptors() noexcept
{
    return descriptors_.get();
}

command_queue* context::queue() noexcept
{
    return queue_.get();
}

resource::texture_manager* context::textures() noexcept
{
    return textures_.get();
}

command_manager* context::commands() noexcept
{
    return commands_.get();
}

context::context(params args) :
    dxgi_factory_{std::move(args.dxgi_factory)},
    device_{std::move(args.device)},
    descriptors_{std::move(args.descriptors)},
    queue_{std::move(args.queue)},
    uploads_{std::move(args.uploads)},
    buffers_{std::move(args.buffers)},
    textures_{std::move(args.textures)},
    samplers_{std::move(args.samplers)},
    pipelines_{std::move(args.pipelines)},
    swap_chains_{std::move(args.swap_chains)},
    transfers_{std::move(args.transfers)},
    texts_{std::move(args.texts)},
    commands_{std::move(args.commands)},
    renderer_{std::move(args.graphics_renderer)}
{
}
} // namespace nori::graphics
