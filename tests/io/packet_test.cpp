#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <nori/io.hpp>

#include "io_test_helpers.hpp"

TEST(packet_test, packet_round_trip)
{
    nori::io::opacket output;
    output.write(std::uint32_t{0x01020304u});
    output.write(3.5f);
    ASSERT_TRUE(output.write(std::string_view{"nori"}));
    output.write(true);
    output.write(test_packet_type::value);
    output.write(7.25);

    const std::array expected_prefix{
        std::byte{0x01}, std::byte{0x02}, std::byte{0x03}, std::byte{0x04}, std::byte{0x40}, std::byte{0x60}, std::byte{0x00}, std::byte{0x00}
    };
    const auto bytes{output.bytes()};
    ASSERT_GE(bytes.size(), expected_prefix.size());
    EXPECT_TRUE(std::ranges::equal(bytes.first(expected_prefix.size()), expected_prefix));

    std::vector<std::byte> body;
    body.append_range(bytes);
    nori::io::ipacket input{std::move(body)};

    const auto int_value{input.read<std::uint32_t>()};
    const auto float_value{input.read<float>()};
    const auto string_value{input.read<std::string>()};
    const auto bool_value{input.read<bool>()};
    const auto enum_value{input.read<test_packet_type>()};
    const auto double_value{input.read<double>()};

    ASSERT_TRUE(int_value);
    ASSERT_TRUE(float_value);
    ASSERT_TRUE(string_value);
    ASSERT_TRUE(bool_value);
    ASSERT_TRUE(enum_value);
    ASSERT_TRUE(double_value);
    EXPECT_EQ(*int_value, 0x01020304u);
    EXPECT_FLOAT_EQ(*float_value, 3.5f);
    EXPECT_EQ(*string_value, "nori");
    EXPECT_TRUE(*bool_value);
    EXPECT_EQ(*enum_value, test_packet_type::value);
    EXPECT_DOUBLE_EQ(*double_value, 7.25);
    EXPECT_EQ(input.remaining(), 0);
}

TEST(packet_test, character_types_round_trip)
{
    enum class character_type : char16_t
    {
        value = u'\u1234'
    };

    const auto round_trip = []<typename T>(T value)
    {
        nori::io::opacket output;
        output.write(value);
        const auto bytes{output.bytes()};
        EXPECT_EQ(bytes.size(), sizeof(T));

        nori::io::ipacket input{std::vector<std::byte>{bytes.begin(), bytes.end()}};
        const auto decoded{input.read<T>()};
        ASSERT_TRUE(decoded);
        EXPECT_EQ(*decoded, value);
        EXPECT_EQ(input.remaining(), 0);
    };

    round_trip('A');
    round_trip(L'\u1234');
    round_trip(u8'A');
    round_trip(u'\u1234');
    round_trip(U'\U00012345');
    round_trip(character_type::value);
}

TEST(packet_test, false_is_a_valid_value)
{
    nori::io::opacket output;
    output.write(false);
    nori::io::ipacket input{std::vector<std::byte>{std::from_range, output.bytes()}};

    const auto decoded{input.read<bool>()};
    ASSERT_TRUE(decoded);
    EXPECT_FALSE(*decoded);
    EXPECT_EQ(input.remaining(), 0);
}

TEST(packet_test, missing_fields_return_nullopt)
{
    nori::io::ipacket input{std::vector<std::byte>{}};

    EXPECT_EQ(input.read<std::uint32_t>(), std::nullopt);
    EXPECT_EQ(input.read<test_packet_type>(), std::nullopt);
    EXPECT_EQ(input.read<std::string>(), std::nullopt);
    EXPECT_EQ(input.read_bytes(), std::nullopt);
    EXPECT_EQ(input.remaining(), 0);
}
