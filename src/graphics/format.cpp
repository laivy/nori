#include <utility>

#include "format.hpp"

namespace nori::graphics
{
DXGI_FORMAT to_dxgi_format(texture_format format)
{
    switch (format)
    {
    case texture_format::r8_unorm:
    {
        return DXGI_FORMAT_R8_UNORM;
    }
    case texture_format::rgba8_unorm:
    {
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    }
    case texture_format::rgba16_float:
    {
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
    }
    case texture_format::r32_float:
    {
        return DXGI_FORMAT_R32_FLOAT;
    }
    case texture_format::d32_float:
    {
        return DXGI_FORMAT_D32_FLOAT;
    }
    case texture_format::bc5_unorm:
    {
        return DXGI_FORMAT_BC5_UNORM;
    }
    case texture_format::bc7_unorm:
    {
        return DXGI_FORMAT_BC7_UNORM;
    }
    case texture_format::bc7_unorm_srgb:
    {
        return DXGI_FORMAT_BC7_UNORM_SRGB;
    }
    default:
        std::unreachable();
    }
}

DXGI_FORMAT to_resource_dxgi_format(texture_format format)
{
    return is_depth_format(format) ? DXGI_FORMAT_R32_TYPELESS : to_dxgi_format(format);
}

DXGI_FORMAT to_srv_dxgi_format(texture_format format)
{
    return is_depth_format(format) ? DXGI_FORMAT_R32_FLOAT : to_dxgi_format(format);
}

bool is_depth_format(texture_format format)
{
    return format == texture_format::d32_float;
}

std::uint32_t bytes_per_pixel(texture_format format)
{
    switch (format)
    {
    case texture_format::r8_unorm:
    {
        return 1;
    }
    case texture_format::rgba8_unorm:
    case texture_format::r32_float:
    {
        return 4;
    }
    case texture_format::rgba16_float:
    {
        return 8;
    }
    case texture_format::bc5_unorm:
    case texture_format::bc7_unorm:
    case texture_format::bc7_unorm_srgb:
    {
        return 16;
    }
    default:
        std::unreachable();
    }
}

DXGI_FORMAT to_dxgi_format(input_element_format format)
{
    switch (format)
    {
    case input_element_format::float1:
    {
        return DXGI_FORMAT_R32_FLOAT;
    }
    case input_element_format::float2:
    {
        return DXGI_FORMAT_R32G32_FLOAT;
    }
    case input_element_format::float3:
    {
        return DXGI_FORMAT_R32G32B32_FLOAT;
    }
    case input_element_format::float4:
    {
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
    }
    case input_element_format::uint1:
    {
        return DXGI_FORMAT_R32_UINT;
    }
    case input_element_format::uint2:
    {
        return DXGI_FORMAT_R32G32_UINT;
    }
    case input_element_format::uint3:
    {
        return DXGI_FORMAT_R32G32B32_UINT;
    }
    case input_element_format::uint4:
    {
        return DXGI_FORMAT_R32G32B32A32_UINT;
    }
    case input_element_format::int1:
    {
        return DXGI_FORMAT_R32_SINT;
    }
    case input_element_format::int2:
    {
        return DXGI_FORMAT_R32G32_SINT;
    }
    case input_element_format::int3:
    {
        return DXGI_FORMAT_R32G32B32_SINT;
    }
    case input_element_format::int4:
    {
        return DXGI_FORMAT_R32G32B32A32_SINT;
    }
    default:
        std::unreachable();
    }
}
} // namespace nori::graphics
