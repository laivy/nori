#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

#include <nori/core/endian.hpp>
#include <nori/resource/image.hpp>
#include <nori/resource/value.hpp>

namespace nori::resource::codec
{
struct file_header
{
    std::uint32_t magic;
    std::uint32_t version;
    std::uint32_t property_count;
    std::uint32_t name_count;
    std::uint64_t property_table_offset;
    std::uint64_t name_table_offset;
    std::uint64_t name_table_size;
    std::uint64_t data_pool_offset;
    std::uint64_t data_pool_size;
};

struct image_header
{
    image_format format;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t mip_levels;
};

static_assert(std::numeric_limits<float>::is_iec559);

constexpr std::uint32_t file_magic{0x69726F6E}; // "nori"
constexpr std::uint32_t file_version{1};
constexpr std::uint64_t file_header_size{56};
constexpr std::size_t image_header_size{sizeof(std::uint8_t) + 3 * sizeof(std::uint32_t)};

template <std::unsigned_integral T>
constexpr void encode_scalar(std::span<std::byte>& bytes, T value)
{
    const T encoded{core::to_endian(value, std::endian::little)};
    std::ranges::copy(std::bit_cast<std::array<std::byte, sizeof(T)>>(encoded), bytes.begin());
    bytes = bytes.subspan(sizeof(T));
}

template <typename Buffer, std::unsigned_integral T>
requires std::constructible_from<std::span<std::byte>, Buffer&>
constexpr void encode_scalar(Buffer& buffer, T value)
{
    std::span<std::byte> bytes{buffer};
    encode_scalar(bytes, value);
}

template <std::unsigned_integral T>
constexpr T decode_scalar(std::span<const std::byte>& bytes)
{
    std::array<std::byte, sizeof(T)> buffer{};
    std::ranges::copy(bytes.first(sizeof(T)), buffer.begin());
    bytes = bytes.subspan(sizeof(T));
    return core::to_endian(std::bit_cast<T>(buffer), std::endian::little);
}

constexpr std::uint32_t canonical_float_bits(std::uint32_t bits)
{
    if ((bits & 0x7FFFFFFFu) == 0)
    {
        return 0;
    }
    if ((bits & 0x7F800000u) == 0x7F800000u && (bits & 0x007FFFFFu) != 0)
    {
        return 0x7FC00000u;
    }
    return bits;
}

constexpr bool is_data_pool_type(property_type tag)
{
    return tag == property_type::string || tag == property_type::image || tag == property_type::binary;
}

constexpr bool is_valid_image(image_format format, std::uint32_t width, std::uint32_t height, std::uint32_t mip_levels, std::uint64_t data_size)
{
    if (format > image_format::bc7_unorm_srgb || width == 0 || height == 0 || mip_levels == 0 || mip_levels > static_cast<std::uint32_t>(std::bit_width(std::max(width, height))))
    {
        return false;
    }

    std::uint64_t expected{0};
    for (std::uint32_t level{0}; level < mip_levels; ++level)
    {
        const std::uint64_t columns{format == image_format::rgba8_unorm ? width : (width - 1) / 4 + 1};
        const std::uint64_t rows{format == image_format::rgba8_unorm ? height : (height - 1) / 4 + 1};
        const std::uint64_t unit_size{format == image_format::rgba8_unorm ? 4u : 16u};
        if (columns * rows > (std::numeric_limits<std::uint64_t>::max() - expected) / unit_size)
        {
            return false;
        }

        expected += columns * rows * unit_size;
        width = std::max(1u, width / 2);
        height = std::max(1u, height / 2);
    }
    return expected == data_size && expected <= std::numeric_limits<std::uint64_t>::max() - image_header_size;
}
} // namespace nori::resource::codec
