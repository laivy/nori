#pragma once
#include <directx/d3d12.h>

#include <nori/core/slot_map.hpp>
#include <nori/graphics/sampler.hpp>

#include "descriptor_allocator.hpp"

namespace nori::graphics
{
class command_queue;
} // namespace nori::graphics

namespace nori::graphics::resource
{
class sampler
{
public:
    sampler(descriptor_allocator* descriptors, descriptor_handle handle);
    ~sampler();

    sampler(const sampler&) = delete;
    sampler(sampler&& other) noexcept;
    sampler& operator=(const sampler&) = delete;
    sampler& operator=(sampler&& other) noexcept;

    descriptor_handle descriptor;

private:
    void reset() noexcept;

    descriptor_allocator* descriptors_;
};

class sampler_manager
{
public:
    struct params
    {
        ID3D12Device* device;
        descriptor_allocator* descriptors;
        command_queue* queue;
    };

    explicit sampler_manager(params args);
    ~sampler_manager();

    sampler_handle create(sampler_desc desc);
    void destroy(sampler_handle handle);
    D3D12_GPU_DESCRIPTOR_HANDLE descriptor(sampler_handle handle) const;

private:
    ID3D12Device* const device_;
    descriptor_allocator* const descriptors_;
    command_queue* const queue_;
    core::slot_map<sampler_handle, sampler> samplers_;
};
} // namespace nori::graphics::resource
