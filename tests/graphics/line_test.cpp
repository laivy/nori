#include <array>

#include "graphics_test_helpers.hpp"
#include "test_shader/quad_ps.hpp"
#include "test_shader/quad_vs.hpp"

namespace
{
class line_test : public initialized_graphics_test
{
protected:
    void expect_pixel(const std::vector<std::byte>& pixels, std::size_t x, std::size_t y, std::array<int, 4> expected) const
    {
        const auto offset{(y * 64 + x) * 4};
        for (std::size_t channel{}; channel < expected.size(); ++channel)
        {
            EXPECT_NEAR(std::to_integer<int>(pixels[offset + channel]), expected[channel], 1) << x << ", " << y << ", " << channel;
        }
    }
};
} // namespace

namespace nori::graphics
{
TEST_F(line_test, records_colored_segments_with_flat_ends_and_blends_in_order)
{
    const auto pixels{draw_and_read_pixels([](command_encoder& encoder)
    {
        line_draw_desc desc{{8.0f, 16.0f}, {40.0f, 16.0f}, 8.0f, {1.0f, 0.0f, 0.0f, 1.0f}};
        encoder.draw_line(desc);
        desc.color = {0.0f, 1.0f, 0.0f, 1.0f};
        encoder.draw_line({{24.0f, 8.0f}, {24.0f, 36.0f}, 8.0f, {0.0f, 0.0f, 1.0f, 0.5f}});
        encoder.draw_rect({{48.0f, 8.0f}, {8.0f, 8.0f}, {0.0f, 1.0f, 0.0f, 1.0f}});
    })};
    ASSERT_EQ(pixels.size(), 64u * 48u * 4u);
    expect_pixel(pixels, 12, 16, {255, 0, 0, 255});
    expect_pixel(pixels, 24, 16, {127, 0, 128, 255});
    expect_pixel(pixels, 24, 30, {0, 0, 128, 128});
    expect_pixel(pixels, 6, 16, {0, 0, 0, 0});
    expect_pixel(pixels, 41, 16, {0, 0, 0, 0});
    expect_pixel(pixels, 24, 6, {0, 0, 0, 0});
    expect_pixel(pixels, 24, 37, {0, 0, 0, 0});
    expect_pixel(pixels, 50, 10, {0, 255, 0, 255});
}

TEST_F(line_test, antialiases_fractional_sides_flat_caps_and_diagonals_without_msaa)
{
    const auto pixels{draw_and_read_pixels([](command_encoder& encoder)
    {
        encoder.draw_line({{8.25f, 8.25f}, {40.25f, 8.25f}, 2.0f, {1.0f, 1.0f, 1.0f, 1.0f}});
        encoder.draw_line({{8.0f, 20.0f}, {28.0f, 40.0f}, 2.0f, {1.0f, 1.0f, 1.0f, 1.0f}});
        encoder.draw_line({{40.0f, 32.5f}, {56.0f, 32.5f}, 0.25f, {1.0f, 1.0f, 1.0f, 1.0f}});
    })};
    ASSERT_EQ(pixels.size(), 64u * 48u * 4u);
    expect_pixel(pixels, 16, 7, {191, 191, 191, 191});
    expect_pixel(pixels, 16, 9, {64, 64, 64, 64});
    expect_pixel(pixels, 8, 8, {191, 191, 191, 191});
    expect_pixel(pixels, 40, 8, {64, 64, 64, 64});
    expect_pixel(pixels, 16, 6, {0, 0, 0, 0});
    expect_pixel(pixels, 7, 8, {0, 0, 0, 0});
    expect_pixel(pixels, 18, 30, {255, 255, 255, 255});
    const auto diagonal_alpha{std::to_integer<int>(pixels[(32 * 64 + 18) * 4 + 3])};
    EXPECT_GT(diagonal_alpha, 0);
    EXPECT_LT(diagonal_alpha, 255);
    expect_pixel(pixels, 18, 34, {0, 0, 0, 0});
    expect_pixel(pixels, 48, 32, {64, 64, 64, 64});
}

TEST_F(line_test, applies_recorded_viewport_and_scissor_and_ignores_empty_segments)
{
    const auto pixels{draw_and_read_pixels([](command_encoder& encoder)
    {
        encoder.set_viewport({{16.0f, 8.0f}, {32.0f, 24.0f}, {0.0f, 1.0f}});
        encoder.set_scissor_rect({22, 12, 12, 12});
        encoder.draw_line({{8.0f, 16.0f}, {48.0f, 16.0f}, 8.0f, {0.0f, 1.0f, 0.0f, 1.0f}});
        encoder.set_viewport({{}, {64.0f, 48.0f}, {0.0f, 1.0f}});
        encoder.set_scissor_rect({0, 0, 64, 48});
        encoder.draw_line({{0.0f, 24.0f}, {64.0f, 24.0f}, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f}});
        encoder.draw_line({{32.0f, 24.0f}, {32.0f, 24.0f}, 40.0f, {1.0f, 0.0f, 0.0f, 1.0f}});
    })};
    ASSERT_EQ(pixels.size(), 64u * 48u * 4u);
    for (std::size_t y{}; y < 48; ++y)
    {
        for (std::size_t x{}; x < 64; ++x)
        {
            const bool is_covered{x >= 22 && x < 34 && y >= 14 && y < 18};
            expect_pixel(pixels, x, y, is_covered ? std::array{0, 255, 0, 255} : std::array{0, 0, 0, 0});
        }
    }
}

TEST_F(line_test, preserves_diagonal_coverage_when_reversing_endpoints_under_nonuniform_viewport)
{
    const auto record = [](command_encoder& encoder, bool reverse)
    {
        encoder.set_viewport({{4.0f, 4.0f}, {48.0f, 24.0f}, {0.0f, 1.0f}});
        const core::vec2 start{-8.0f, 8.0f};
        const core::vec2 end{56.0f, 40.0f};
        encoder.draw_line({reverse ? end : start, reverse ? start : end, 6.0f, {0.5f, 1.0f, 0.0f, 0.5f}});
    };
    const auto forward{draw_and_read_pixels([&](command_encoder& encoder)
    {
        record(encoder, false);
    })};
    const auto reverse{draw_and_read_pixels([&](command_encoder& encoder)
    {
        record(encoder, true);
    })};
    ASSERT_EQ(forward.size(), 64u * 48u * 4u);
    ASSERT_EQ(reverse.size(), forward.size());
    for (std::size_t i{}; i < forward.size(); ++i)
    {
        EXPECT_NEAR(std::to_integer<int>(forward[i]), std::to_integer<int>(reverse[i]), 1);
    }
    expect_pixel(forward, 22, 16, {64, 128, 0, 128});
    expect_pixel(forward, 22, 22, {0, 0, 0, 0});
    const auto alpha{std::to_integer<int>(forward[(17 * 64 + 22) * 4 + 3])};
    EXPECT_GT(alpha, 0);
    EXPECT_LT(alpha, 128);
}

TEST_F(line_test, preserves_user_pipeline_vertex_index_constants_texture_and_sampler_bindings)
{
    const auto vs{create_shader(std::as_bytes(std::span{quad_vs})).release()};
    const auto ps{create_shader(std::as_bytes(std::span{quad_ps})).release()};
    ASSERT_TRUE(vs && ps);
    const std::array bindings{
        shader_binding{shader_binding_type::constant_buffer, 0, shader_stage::vertex | shader_stage::pixel},
        shader_binding{shader_binding_type::texture_srv, 0, shader_stage::pixel},
        shader_binding{shader_binding_type::sampler, 0, shader_stage::pixel},
    };
    const std::array elements{
        input_element{"POSITION", 0, input_element_format::float2, 0},
        input_element{"TEXCOORD", 0, input_element_format::float2, 8},
    };
    const std::array formats{texture_format::rgba8_unorm};
    const std::array blends{render_target_blend_desc{false, blend_factor::one, blend_factor::zero, blend_op::add, blend_factor::one, blend_factor::zero, blend_op::add, color_write_mask::all}};
    const auto pipeline{create_graphics_pipeline({.vertex_shader = vs, .pixel_shader = ps, .bindings = bindings, .render_target_formats = formats, .layout = {16, elements}, .topology = primitive_topology::triangle_list, .render_target_blends = blends, .sample_count = 1}).release()};
    ASSERT_TRUE(pipeline);
    const auto texture{create_texture({texture_format::r8_unorm, 1, 1, 1, 1, texture_usage::shader_resource | texture_usage::copy_destination}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(texture);
    const std::array white{std::byte{255}};
    ASSERT_TRUE(update(*texture, {0, 0, 1, 1}, {white, 1}));
    const auto sampler{create_sampler({sampler_filter::point, sampler_filter::point, sampler_filter::point, sampler_address_mode::clamp, sampler_address_mode::clamp, sampler_address_mode::clamp, 1, 0, 0}).release()};
    ASSERT_TRUE(sampler);
    const auto pixels{draw_and_read_pixels([&](command_encoder& encoder)
    {
        const std::array<float, 16> vertices{8, 8, 0, 0, 24, 8, 1, 0, 8, 24, 0, 1, 24, 24, 1, 1};
        const std::array<std::uint16_t, 6> indices{0, 1, 2, 2, 1, 3};
        const auto vertex_buffer{create_frame_upload_buffer(buffer_usage::vertex, std::as_bytes(std::span{vertices}))};
        const auto index_buffer{create_frame_upload_buffer(buffer_usage::index, std::as_bytes(std::span{indices}))};
        ASSERT_TRUE(vertex_buffer && index_buffer);
        const std::array<float, 8> constants{64, 48, 0, 0, 0, 1, 0, 1};
        encoder.set_pipeline(pipeline);
        encoder.set_vertex_buffer(*vertex_buffer, 16);
        encoder.set_index_buffer(*index_buffer, index_format::uint16);
        encoder.set_constant_buffer(0, std::as_bytes(std::span{constants}));
        encoder.set_texture(1, *texture);
        encoder.set_sampler(2, sampler);
        encoder.draw_indexed(6);
        encoder.draw_line({{0.0f, 16.0f}, {40.0f, 16.0f}, 4.0f, {1.0f, 0.0f, 0.0f, 1.0f}});
        encoder.draw_indexed(6);
    })};
    ASSERT_EQ(pixels.size(), 64u * 48u * 4u);
    expect_pixel(pixels, 16, 16, {0, 255, 0, 255});
    expect_pixel(pixels, 32, 16, {255, 0, 0, 255});
    expect_pixel(pixels, 16, 10, {0, 255, 0, 255});
    destroy(sampler);
    destroy(*texture);
    destroy(pipeline);
    destroy(ps);
    destroy(vs);
}

TEST_F(line_test, clips_long_finite_segments_to_the_target)
{
    const auto pixels{draw_and_read_pixels([](command_encoder& encoder)
    {
        encoder.draw_line({{-1.0e20f, 16.0f}, {1.0e20f, 16.0f}, 4.0f, {1.0f, 0.0f, 0.0f, 1.0f}});
        encoder.draw_line({{48.0f, -1.0e20f}, {48.0f, 1.0e20f}, 4.0f, {0.0f, 1.0f, 0.0f, 1.0f}});
        const auto largest{std::numeric_limits<float>::max()};
        encoder.draw_line({{-largest, -largest}, {largest, largest}, 2.0f, {0.0f, 0.0f, 1.0f, 1.0f}});
    })};
    ASSERT_EQ(pixels.size(), 64u * 48u * 4u);
    expect_pixel(pixels, 24, 16, {255, 0, 0, 255});
    expect_pixel(pixels, 48, 32, {0, 255, 0, 255});
    expect_pixel(pixels, 32, 32, {0, 0, 255, 255});
    expect_pixel(pixels, 16, 32, {0, 0, 0, 0});
}
} // namespace nori::graphics
