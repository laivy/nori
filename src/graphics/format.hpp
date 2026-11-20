#pragma once
#include <dxgiformat.h>

#include <nori/graphics/pipeline.hpp>
#include <nori/graphics/texture.hpp>

namespace nori::graphics
{
DXGI_FORMAT to_dxgi_format(texture_format format);
DXGI_FORMAT to_resource_dxgi_format(texture_format format);
DXGI_FORMAT to_srv_dxgi_format(texture_format format);
bool is_depth_format(texture_format format);
std::uint32_t bytes_per_pixel(texture_format format);

DXGI_FORMAT to_dxgi_format(input_element_format format);
} // namespace nori::graphics
