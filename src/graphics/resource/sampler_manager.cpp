#include <utility>

#include "command_queue.hpp"
#include "resource/sampler_manager.hpp"

namespace
{
constexpr D3D12_TEXTURE_ADDRESS_MODE to_address_mode(nori::graphics::sampler_address_mode value)
{
    switch (value)
    {
    case nori::graphics::sampler_address_mode::wrap:
    {
        return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    }
    case nori::graphics::sampler_address_mode::mirror:
    {
        return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    }
    case nori::graphics::sampler_address_mode::clamp:
    {
        return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    }
    case nori::graphics::sampler_address_mode::border:
    {
        return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    }
    default:
        std::unreachable();
    }
}

constexpr D3D12_FILTER_TYPE to_filter_type(nori::graphics::sampler_filter value)
{
    switch (value)
    {
    case nori::graphics::sampler_filter::point:
    {
        return D3D12_FILTER_TYPE_POINT;
    }
    case nori::graphics::sampler_filter::linear:
    {
        return D3D12_FILTER_TYPE_LINEAR;
    }
    case nori::graphics::sampler_filter::anisotropic:
    default:
        std::unreachable();
    }
}
} // namespace

namespace nori::graphics::resource
{
sampler::sampler(descriptor_allocator* descriptors, descriptor_handle handle) :
    descriptor{handle},
    descriptors_{descriptors}
{
}

sampler::~sampler()
{
    reset();
}

sampler::sampler(sampler&& other) noexcept :
    descriptor{std::exchange(other.descriptor, {})},
    descriptors_{std::exchange(other.descriptors_, nullptr)}
{
}

sampler& sampler::operator=(sampler&& other) noexcept
{
    if (this != &other)
    {
        reset();
        descriptor = std::exchange(other.descriptor, {});
        descriptors_ = std::exchange(other.descriptors_, nullptr);
    }
    return *this;
}

void sampler::reset() noexcept
{
    if (descriptors_)
    {
        descriptors_->free(descriptor);
    }
    descriptor = {};
    descriptors_ = nullptr;
}

sampler_manager::sampler_manager(params args) :
    device_{args.device},
    descriptors_{args.descriptors},
    queue_{args.queue}
{
}

sampler_manager::~sampler_manager() = default;

sampler_handle sampler_manager::create(sampler_desc desc)
{
    const bool anisotropic{desc.min_filter == sampler_filter::anisotropic || desc.mag_filter == sampler_filter::anisotropic || desc.mip_filter == sampler_filter::anisotropic};

    sampler value{descriptors_, descriptors_->allocate(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)};
    if (value.descriptor.cpu.ptr == 0 || value.descriptor.gpu.ptr == 0)
    {
        return {};
    }
    const D3D12_SAMPLER_DESC native_desc{
        .Filter = anisotropic ? D3D12_FILTER_ANISOTROPIC : D3D12_ENCODE_BASIC_FILTER(to_filter_type(desc.min_filter), to_filter_type(desc.mag_filter), to_filter_type(desc.mip_filter), D3D12_FILTER_REDUCTION_TYPE_STANDARD),
        .AddressU = to_address_mode(desc.address_u),
        .AddressV = to_address_mode(desc.address_v),
        .AddressW = to_address_mode(desc.address_w),
        .MaxAnisotropy = anisotropic ? desc.maximum_anisotropy : 1,
        .ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
        .MinLOD = desc.minimum_lod,
        .MaxLOD = desc.maximum_lod,
    };
    device_->CreateSampler(&native_desc, value.descriptor.cpu);
    return samplers_.emplace(std::move(value));
}

void sampler_manager::destroy(sampler_handle handle)
{
    queue_->release(std::move(samplers_.at(handle)));
    samplers_.erase(handle);
}

D3D12_GPU_DESCRIPTOR_HANDLE sampler_manager::descriptor(sampler_handle handle) const
{
    return samplers_.at(handle).descriptor.gpu;
}
} // namespace nori::graphics::resource
