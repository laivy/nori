#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace nori::resource
{
enum class image_format : std::uint8_t
{
    unknown,
    r8g8b8a8
};

struct image
{
    constexpr bool operator==(const image&) const = default;

    image_format format;
    std::uint32_t width;
    std::uint32_t height;
};

struct image_asset
{
    constexpr bool operator==(const image_asset&) const = default;

    image img;
    std::vector<std::byte> data;
};
} // namespace nori::resource
