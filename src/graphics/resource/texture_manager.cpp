#include <algorithm>
#include <bit>
#include <cstddef>
#include <optional>
#include <utility>

#include <directx/d3dx12.h>

#include <nori/resource/operations.hpp>

#include "command_queue.hpp"
#include "format.hpp"
#include "resource/texture_manager.hpp"
#include "upload_manager.hpp"

namespace
{
constexpr bool is_block_compressed(nori::graphics::texture_format format)
{
    return format == nori::graphics::texture_format::bc5_unorm ||
        format == nori::graphics::texture_format::bc7_unorm ||
        format == nori::graphics::texture_format::bc7_unorm_srgb;
}

constexpr std::optional<nori::graphics::texture_format> to_texture_format(nori::resource::image_format format)
{
    switch (format)
    {
    case nori::resource::image_format::rgba8_unorm:
    {
        return nori::graphics::texture_format::rgba8_unorm;
    }
    case nori::resource::image_format::bc5_unorm:
    {
        return nori::graphics::texture_format::bc5_unorm;
    }
    case nori::resource::image_format::bc7_unorm:
    {
        return nori::graphics::texture_format::bc7_unorm;
    }
    case nori::resource::image_format::bc7_unorm_srgb:
    {
        return nori::graphics::texture_format::bc7_unorm_srgb;
    }
    default:
        return std::nullopt;
    }
}

struct mip_layout
{
    std::uint32_t row_size;
    std::uint32_t row_count;
    std::size_t size;
};

constexpr mip_layout get_mip_layout(nori::graphics::texture_format format, std::uint32_t width, std::uint32_t height)
{
    const bool compressed{is_block_compressed(format)};
    const std::uint32_t row_size{compressed ? ((width + 3) / 4) * 16 : width * 4};
    const std::uint32_t row_count{compressed ? (height + 3) / 4 : height};
    return {row_size, row_count, static_cast<std::size_t>(row_size) * row_count};
}
} // namespace

namespace nori::graphics::resource
{
texture::texture(descriptor_allocator* descriptors) :
    output_handle{},
    srv_handle{},
    format{},
    usage{},
    width{0},
    height{0},
    mip_levels{0},
    sample_count{0},
    descriptors_{descriptors}
{
}

texture::~texture()
{
    reset();
}

texture::texture(texture&& other) noexcept :
    resource{std::move(other.resource)},
    output_handle{std::exchange(other.output_handle, {})},
    srv_handle{std::exchange(other.srv_handle, {})},
    mip_output_handles{std::move(other.mip_output_handles)},
    mip_srv_handles{std::move(other.mip_srv_handles)},
    format{other.format},
    usage{other.usage},
    states{std::move(other.states)},
    width{other.width},
    height{other.height},
    mip_levels{other.mip_levels},
    sample_count{other.sample_count},
    descriptors_{std::exchange(other.descriptors_, nullptr)}
{
}

texture& texture::operator=(texture&& other) noexcept
{
    if (this != &other)
    {
        reset();
        resource = std::move(other.resource);
        output_handle = std::exchange(other.output_handle, {});
        srv_handle = std::exchange(other.srv_handle, {});
        mip_output_handles = std::move(other.mip_output_handles);
        mip_srv_handles = std::move(other.mip_srv_handles);
        format = other.format;
        usage = other.usage;
        states = std::move(other.states);
        width = other.width;
        height = other.height;
        mip_levels = other.mip_levels;
        sample_count = other.sample_count;
        descriptors_ = std::exchange(other.descriptors_, nullptr);
    }
    return *this;
}

void texture::reset() noexcept
{
    if (descriptors_)
    {
        descriptors_->free(output_handle);
        descriptors_->free(srv_handle);
        for (const auto& handle : mip_output_handles)
        {
            descriptors_->free(handle);
        }
        for (const auto& handle : mip_srv_handles)
        {
            descriptors_->free(handle);
        }
    }
    output_handle = {};
    srv_handle = {};
    mip_output_handles.clear();
    mip_srv_handles.clear();
    resource.Reset();
    descriptors_ = nullptr;
}

texture_manager::texture_manager(params args) :
    device_{args.device},
    descriptors_{args.descriptors},
    uploads_{args.uploads},
    queue_{args.queue}
{
}

texture_manager::~texture_manager() = default;

result<texture_handle> texture_manager::create(texture_desc desc)
{
    const bool depth{is_depth_format(desc.format)};
    const bool is_render_target{(desc.usage & texture_usage::render_target) != texture_usage::none};
    const bool is_depth_stencil{(desc.usage & texture_usage::depth_stencil) != texture_usage::none};
    const bool is_shader_resource{(desc.usage & texture_usage::shader_resource) != texture_usage::none};

    auto tex{std::make_unique<texture>(descriptors_)};
    tex->format = desc.format;
    tex->usage = desc.usage;
    const D3D12_RESOURCE_STATES initial_state{is_shader_resource ? D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COMMON};
    tex->states = resource_states{initial_state, desc.mip_levels};
    tex->width = desc.width;
    tex->height = desc.height;
    tex->mip_levels = desc.mip_levels;
    tex->sample_count = desc.sample_count;

    const DXGI_FORMAT resource_format{to_resource_dxgi_format(desc.format)};
    const CD3DX12_HEAP_PROPERTIES heap{D3D12_HEAP_TYPE_DEFAULT};
    const D3D12_RESOURCE_FLAGS flags{[is_render_target, is_depth_stencil]()
    {
        D3D12_RESOURCE_FLAGS flags{D3D12_RESOURCE_FLAG_NONE};
        if (is_render_target)
        {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        }
        if (is_depth_stencil)
        {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        }
        return flags;
    }()};
    const auto resource_desc{CD3DX12_RESOURCE_DESC::Tex2D(resource_format, desc.width, desc.height, 1, desc.mip_levels, desc.sample_count, 0, flags)};
    D3D12_CLEAR_VALUE clear{};
    const D3D12_CLEAR_VALUE* optimized_clear{};
    if (is_render_target)
    {
        clear.Format = resource_format;
        optimized_clear = &clear;
    }
    else if (is_depth_stencil)
    {
        clear.Format = to_dxgi_format(desc.format);
        clear.DepthStencil = {.Depth = 1.0f};
        optimized_clear = &clear;
    }
    if (FAILED(device_->CreateCommittedResource(
            &heap,
            D3D12_HEAP_FLAG_NONE,
            &resource_desc,
            initial_state,
            optimized_clear,
            IID_PPV_ARGS(&tex->resource)
        )))
    {
        return std::unexpected{error_code::resource_creation_failed};
    }

    if (is_render_target || is_depth_stencil)
    {
        tex->output_handle = descriptors_->allocate(depth ? D3D12_DESCRIPTOR_HEAP_TYPE_DSV : D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        if (tex->output_handle.cpu.ptr == 0)
        {
            return std::unexpected{error_code::descriptor_allocation_failed};
        }
    }

    if (is_shader_resource)
    {
        tex->srv_handle = descriptors_->allocate(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        if (tex->srv_handle.cpu.ptr == 0)
        {
            return std::unexpected{error_code::descriptor_allocation_failed};
        }
        tex->mip_srv_handles.reserve(desc.mip_levels);
        for (std::uint16_t mip{0}; mip < desc.mip_levels; ++mip)
        {
            const descriptor_handle handle{descriptors_->allocate(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)};
            if (handle.cpu.ptr == 0)
            {
                return std::unexpected{error_code::descriptor_allocation_failed};
            }
            tex->mip_srv_handles.push_back(handle);
        }
    }

    if (is_render_target && desc.mip_levels > 1)
    {
        tex->mip_output_handles.reserve(desc.mip_levels - 1);
        for (std::uint16_t mip{1}; mip < desc.mip_levels; ++mip)
        {
            const descriptor_handle handle{descriptors_->allocate(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)};
            if (handle.cpu.ptr == 0)
            {
                return std::unexpected{error_code::descriptor_allocation_failed};
            }
            tex->mip_output_handles.push_back(handle);
        }
    }

    if (is_render_target)
    {
        device_->CreateRenderTargetView(tex->resource.Get(), nullptr, tex->output_handle.cpu);
        for (std::uint16_t mip{1}; mip < desc.mip_levels; ++mip)
        {
            const D3D12_RENDER_TARGET_VIEW_DESC view{
                .Format = to_dxgi_format(desc.format),
                .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
                .Texture2D = {.MipSlice = mip}
            };
            device_->CreateRenderTargetView(tex->resource.Get(), &view, tex->mip_output_handles[mip - 1].cpu);
        }
    }
    else if (is_depth_stencil)
    {
        const D3D12_DEPTH_STENCIL_VIEW_DESC view{
            .Format = to_dxgi_format(desc.format),
            .ViewDimension = desc.sample_count > 1 ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D
        };
        device_->CreateDepthStencilView(tex->resource.Get(), &view, tex->output_handle.cpu);
    }

    if (is_shader_resource)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
        srv.Format = to_srv_dxgi_format(desc.format);
        srv.ViewDimension = desc.sample_count > 1 ? D3D12_SRV_DIMENSION_TEXTURE2DMS : D3D12_SRV_DIMENSION_TEXTURE2D;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        if (desc.sample_count == 1)
        {
            srv.Texture2D.MipLevels = desc.mip_levels;
        }
        device_->CreateShaderResourceView(tex->resource.Get(), &srv, tex->srv_handle.cpu);
        for (std::uint16_t mip{0}; mip < desc.mip_levels; ++mip)
        {
            if (desc.sample_count == 1)
            {
                srv.Texture2D.MostDetailedMip = mip;
                srv.Texture2D.MipLevels = 1;
            }
            device_->CreateShaderResourceView(tex->resource.Get(), &srv, tex->mip_srv_handles[mip].cpu);
        }
    }
    return textures_.emplace(std::move(*tex));
}

result<texture_handle> texture_manager::create(nori::resource::handle source)
{
    const auto image{nori::resource::try_get<nori::resource::image_view>(source)};
    if (!image)
    {
        return std::unexpected{image.error() == nori::resource::error_code::type_mismatch ? error_code::invalid_argument : error_code::resource_creation_failed};
    }

    const auto format{to_texture_format(image->format)};
    if (!format)
    {
        return std::unexpected{error_code::unsupported};
    }
    if (image->width == 0 || image->height == 0 || image->width > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION || image->height > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION || image->mip_levels == 0 || image->mip_levels > static_cast<std::uint32_t>(std::bit_width(std::max(image->width, image->height))))
    {
        return std::unexpected{error_code::invalid_argument};
    }

    std::size_t expected_size{};
    std::uint32_t mip_width{image->width};
    std::uint32_t mip_height{image->height};
    for (std::uint32_t mip{0}; mip < image->mip_levels; ++mip)
    {
        expected_size += get_mip_layout(*format, mip_width, mip_height).size;
        mip_width = std::max(1u, mip_width / 2);
        mip_height = std::max(1u, mip_height / 2);
    }
    if (image->data.size() != expected_size)
    {
        return std::unexpected{error_code::invalid_argument};
    }

    const texture_desc desc{
        .format = *format,
        .width = image->width,
        .height = image->height,
        .mip_levels = static_cast<std::uint16_t>(image->mip_levels),
        .sample_count = 1,
        .usage = texture_usage::shader_resource | texture_usage::copy_destination
    };
    const auto handle{create(desc)};
    if (!handle)
    {
        return std::unexpected{handle.error()};
    }

    texture& tex{textures_.at(*handle)};
    std::size_t offset{};
    mip_width = image->width;
    mip_height = image->height;
    for (std::uint32_t mip{0}; mip < image->mip_levels; ++mip)
    {
        const mip_layout layout{get_mip_layout(*format, mip_width, mip_height)};
        const auto mip_data{image->data.subspan(offset, layout.size)};
        if (!uploads_->upload_texture(
                tex.resource.Get(),
                mip,
                0,
                0,
                mip_width,
                mip_height,
                layout.row_size,
                layout.row_count,
                mip_data,
                layout.row_size,
                tex.states,
                D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
                {}
            ))
        {
            destroy(*handle);
            return std::unexpected{error_code::command_failed};
        }
        offset += layout.size;
        mip_width = std::max(1u, mip_width / 2);
        mip_height = std::max(1u, mip_height / 2);
    }
    return *handle;
}

result<void> texture_manager::update(texture_handle handle, texture_region region, texture_data data)
{
    texture& tex{at(handle)};

    const std::uint32_t pixel_size{bytes_per_pixel(tex.format)};
    const bool compressed{is_block_compressed(tex.format)};
    const std::uint32_t row_pitch{compressed ? std::max(1u, (region.width + 3u) / 4u) * pixel_size : region.width * pixel_size};
    const std::uint32_t row_count{compressed ? std::max(1u, (region.height + 3u) / 4u) : region.height};

    const D3D12_RESOURCE_STATES final_state{(tex.usage & texture_usage::shader_resource) != texture_usage::none ? D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COMMON};
    if (!uploads_->upload_texture(tex.resource.Get(), region.mip_level, region.x, region.y, region.width, region.height, row_pitch, row_count, data.data, data.row_pitch, tex.states, final_state, {}))
    {
        return std::unexpected{error_code::command_failed};
    }
    return {};
}

void texture_manager::destroy(texture_handle handle)
{
    queue_->release(std::move(textures_.at(handle)));
    textures_.erase(handle);
}

result<texture_handle> texture_manager::create_back_buffer(Microsoft::WRL::ComPtr<ID3D12Resource> buffer)
{
    const D3D12_RESOURCE_DESC desc{buffer->GetDesc()};

    texture tex{descriptors_};
    tex.resource = std::move(buffer);
    tex.format = texture_format::rgba8_unorm;
    tex.usage = texture_usage::render_target | texture_usage::copy_source | texture_usage::copy_destination;
    tex.states = resource_states{D3D12_RESOURCE_STATE_PRESENT, desc.MipLevels};
    tex.width = static_cast<std::uint32_t>(desc.Width);
    tex.height = desc.Height;
    tex.mip_levels = desc.MipLevels;
    tex.sample_count = desc.SampleDesc.Count;
    tex.output_handle = descriptors_->allocate(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    if (tex.output_handle.cpu.ptr == 0)
    {
        return std::unexpected{error_code::descriptor_allocation_failed};
    }
    device_->CreateRenderTargetView(tex.resource.Get(), nullptr, tex.output_handle.cpu);
    return textures_.emplace(std::move(tex));
}

texture texture_manager::release_back_buffer(texture_handle handle)
{
    texture tex{std::move(textures_.at(handle))};
    textures_.erase(handle);
    return tex;
}

texture& texture_manager::at(texture_handle handle)
{
    return textures_.at(handle);
}

D3D12_GPU_DESCRIPTOR_HANDLE texture_manager::srv(texture_handle handle) const
{
    const auto& tex{at(handle)};
    return tex.srv_handle.gpu;
}

D3D12_GPU_DESCRIPTOR_HANDLE texture_manager::srv(texture_handle handle, std::uint16_t mip_level) const
{
    const texture& tex{at(handle)};
    return tex.mip_srv_handles[mip_level].gpu;
}

D3D12_CPU_DESCRIPTOR_HANDLE texture_manager::rtv(texture_handle handle, std::uint16_t mip_level) const
{
    const texture& tex{at(handle)};

    return mip_level == 0 ? tex.output_handle.cpu : tex.mip_output_handles[mip_level - 1].cpu;
}

const texture& texture_manager::at(texture_handle handle) const
{
    return textures_.at(handle);
}
} // namespace nori::graphics::resource
