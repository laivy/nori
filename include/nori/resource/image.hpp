#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace nori::resource
{
enum class image_format : std::uint8_t
{
    rgba8_unorm,
    bc5_unorm,
    bc7_unorm,
    bc7_unorm_srgb
};

struct image
{
    constexpr bool operator==(const image&) const = default;

    image_format format;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t mip_levels;
    std::vector<std::byte> data;
};

struct image_view
{
    constexpr bool operator==(const image_view& rhs) const noexcept
    {
        return format == rhs.format &&
            width == rhs.width &&
            height == rhs.height &&
            mip_levels == rhs.mip_levels &&
            std::ranges::equal(data, rhs.data);
    }

    image_format format;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t mip_levels;
    std::span<const std::byte> data;
};
} // namespace nori::resource
