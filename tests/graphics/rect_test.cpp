#include <array>

#include "graphics_test_helpers.hpp"

namespace
{
class rect_test : public initialized_graphics_test
{
};
} // namespace

namespace nori::graphics
{
TEST_F(rect_test, records_values_and_blends_rectangles_in_order_without_text_resources)
{
    const auto pixels{draw_and_read_pixels([](command_encoder& encoder)
    {
        rect_draw_desc desc{{8.0f, 8.0f}, {24.0f, 16.0f}, {1.0f, 0.0f, 0.0f, 1.0f}};
        encoder.draw_rect(desc);
        desc.color = {0.0f, 1.0f, 0.0f, 1.0f};
        encoder.draw_rect({{16.0f, 12.0f}, {32.0f, 24.0f}, {0.0f, 0.0f, 1.0f, 0.5f}});
    })};
    ASSERT_EQ(pixels.size(), 64u * 48u * 4u);
    const auto expect_pixel = [&](std::size_t x, std::size_t y, std::array<int, 4> expected)
    {
        const auto offset{(y * 64 + x) * 4};
        for (std::size_t channel{}; channel < expected.size(); ++channel)
        {
            EXPECT_NEAR(std::to_integer<int>(pixels[offset + channel]), expected[channel], 1);
        }
    };
    expect_pixel(4, 4, {0, 0, 0, 0});
    expect_pixel(12, 10, {255, 0, 0, 255});
    expect_pixel(20, 16, {127, 0, 128, 255});
    expect_pixel(40, 30, {0, 0, 128, 128});
    expect_pixel(52, 40, {0, 0, 0, 0});
}

TEST_F(rect_test, applies_recorded_viewport_and_scissor_and_ignores_zero_area)
{
    const auto pixels{draw_and_read_pixels([](command_encoder& encoder)
    {
        encoder.set_viewport({{16.0f, 8.0f}, {32.0f, 24.0f}, {0.0f, 1.0f}});
        encoder.set_scissor_rect({20, 12, 12, 12});
        encoder.draw_rect({{0.0f, 0.0f}, {32.0f, 32.0f}, {0.0f, 1.0f, 0.0f, 1.0f}});
        encoder.set_viewport({{}, {64.0f, 48.0f}, {0.0f, 1.0f}});
        encoder.set_scissor_rect({0, 0, 64, 48});
        encoder.draw_rect({{0.0f, 0.0f}, {0.0f, 48.0f}, {1.0f, 0.0f, 0.0f, 1.0f}});
        encoder.draw_rect({{0.0f, 0.0f}, {64.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}});
    })};
    ASSERT_EQ(pixels.size(), 64u * 48u * 4u);
    for (std::size_t y{}; y < 48; ++y)
    {
        for (std::size_t x{}; x < 64; ++x)
        {
            const auto offset{(y * 64 + x) * 4};
            const bool covered{x >= 20 && x < 32 && y >= 12 && y < 24};
            EXPECT_EQ(pixels[offset], std::byte{});
            EXPECT_EQ(pixels[offset + 1], covered ? std::byte{255} : std::byte{});
            EXPECT_EQ(pixels[offset + 2], std::byte{});
            EXPECT_EQ(pixels[offset + 3], covered ? std::byte{255} : std::byte{});
        }
    }
}
} // namespace nori::graphics
