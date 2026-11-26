#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

#include <gtest/gtest.h>

#include <nori/core/endian.hpp>

namespace
{
TEST(endian_test, scalar_conversion)
{
    constexpr std::uint32_t value{0x01020304u};
    const std::uint32_t little{nori::core::to_endian(value, std::endian::little)};
    const std::uint32_t big{nori::core::to_endian(value, std::endian::big)};
    const std::array expected_little{
        std::byte{4}, std::byte{3}, std::byte{2}, std::byte{1}
    };
    const std::array expected_big{
        std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}
    };
    const auto little_bytes{std::bit_cast<std::array<std::byte, sizeof(little)>>(little)};
    const auto big_bytes{std::bit_cast<std::array<std::byte, sizeof(big)>>(big)};

    EXPECT_EQ(little_bytes, expected_little);
    EXPECT_EQ(big_bytes, expected_big);
    EXPECT_EQ(nori::core::to_endian(little, std::endian::little), value);
    EXPECT_EQ(nori::core::to_endian(big, std::endian::big), value);

    constexpr std::uint8_t u8_value{0xAB};
    EXPECT_EQ(nori::core::to_endian(u8_value, std::endian::big), u8_value);
    EXPECT_EQ(nori::core::to_endian(u8_value, std::endian::little), u8_value);

    constexpr float float_value{3.5f};
    const float encoded{nori::core::to_endian(float_value, std::endian::big)};
    EXPECT_EQ(nori::core::to_endian(encoded, std::endian::big), float_value);
}
} // namespace
