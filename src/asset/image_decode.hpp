#pragma once
#include <cstddef>
#include <span>

#include <nori/asset.hpp>
#include <nori/asset/error_code.hpp>
#include <nori/resource/image.hpp>

namespace nori::asset
{
result<resource::image> decode_image(std::span<const std::byte> data, resource::image_format format, texture_compression_quality quality);
} // namespace nori::asset
