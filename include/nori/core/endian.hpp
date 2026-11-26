#pragma once
#include <bit>
#include <cstdint>
#include <type_traits>

namespace nori::core
{
template <typename T>
requires(
    (std::is_integral_v<T> && !std::is_same_v<std::remove_cv_t<T>, bool>) ||
    (std::is_floating_point_v<T> && (sizeof(T) == 4 || sizeof(T) == 8))
)
[[nodiscard]] constexpr T to_endian(T value, std::endian endian) noexcept
{
    static_assert(std::endian::native == std::endian::little || std::endian::native == std::endian::big);
    if (sizeof(T) == 1 || endian == std::endian::native)
    {
        return value;
    }

    if constexpr (std::is_integral_v<T>)
    {
        return std::byteswap(value);
    }
    else
    {
        using uint_t = std::conditional_t<
            sizeof(T) == sizeof(std::uint32_t),
            std::uint32_t,
            std::uint64_t
        >;
        return std::bit_cast<T>(std::byteswap(std::bit_cast<uint_t>(value)));
    }
}
} // namespace nori::core
