#include <array>

#include "test_shader/text_masks_ps.hpp"
#include "test_shader/text_translate_vs.hpp"
#include "text_test_helpers.hpp"

namespace nori::graphics
{
using test_support::draw_layout;
using test_support::text_font_fixture;

TEST_F(text_test, outline_uses_round_corners_and_preserves_fractional_thickness)
{
    const auto bytes{text_font_fixture("Text-Regular.ttf")};
    const auto font{text::create_font({bytes, 0})};
    ASSERT_TRUE(font);
    text::layout_desc desc{.font = font->get(), .font_size = 40.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}, .outline = text::outline_desc{2.0f, {1.0f, 0.0f, 0.0f, 1.0f}}};
    const auto layout{text::create_layout(desc, "A")};
    ASSERT_TRUE(layout);
    const auto pixels{draw_layout(layout->get())};
    ASSERT_EQ(pixels.size(), 160u * 96u * 4u);
    const auto top{static_cast<std::size_t>(std::lround(8.0f + text::metrics(layout->get()).first_baseline - 28.0f))};
    EXPECT_EQ(pixels[((top - 3) * 160 + 7) * 4 + 3], std::byte{});
    EXPECT_EQ(pixels[(top * 160 + 8) * 4 + 3], std::byte{255});
    const auto corner_alpha{std::to_integer<int>(pixels[((top - 2) * 160 + 8) * 4 + 3])};
    EXPECT_GT(corner_alpha, 20);
    EXPECT_LT(corner_alpha, 180);
    EXPECT_EQ(pixels[(top * 160 + 10) * 4 + 1], std::byte{255});
    const auto pixel_shader{create_shader(std::as_bytes(std::span{text_masks_ps}))};
    std::vector<std::byte> previous;
    std::uint64_t previous_outline{};
    for (const float thickness : {0.25f, 0.75f, 1.5f, 2.5f})
    {
        desc.outline->thickness = thickness;
        ASSERT_TRUE(text::update(layout->get(), desc, "A"));
        const auto masks{draw_layout(layout->get(), {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}, text::effect{{}, pixel_shader.get(), {}})};
        ASSERT_EQ(masks.size(), pixels.size());
        std::uint64_t outline_sum{};
        for (std::size_t i{}; i < masks.size(); i += 4)
        {
            outline_sum += std::to_integer<unsigned>(masks[i + 1]);
            if (!previous.empty())
            {
                EXPECT_NEAR(std::to_integer<int>(masks[i]), std::to_integer<int>(previous[i]), 1);
                EXPECT_GE(std::to_integer<int>(masks[i + 1]) + 1, std::to_integer<int>(previous[i + 1]));
            }
        }
        EXPECT_GT(outline_sum, previous_outline);
        previous_outline = outline_sum;
        previous = masks;
    }
}

TEST_F(text_test, outlined_korean_curves_keep_holes_and_complementary_masks)
{
    const auto bytes{text_font_fixture("NanumGothic-Regular.ttf")};
    const auto font{text::create_font({bytes, 0})};
    ASSERT_TRUE(font);
    const auto pixel_shader{create_shader(std::as_bytes(std::span{text_masks_ps}))};
    for (const float font_size : {18.0f, 32.0f})
    {
        const auto layout{text::create_layout({.font = font->get(), .font_size = font_size, .color = {1.0f, 1.0f, 1.0f, 1.0f}, .outline = text::outline_desc{0.75f, {1.0f, 0.0f, 0.0f, 1.0f}}}, "용")};
        ASSERT_TRUE(layout);
        const auto masks{draw_layout(layout->get(), {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}, text::effect{{}, pixel_shader.get(), {}})};
        ASSERT_EQ(masks.size(), 160u * 96u * 4u);
        const auto fill_at = [&](std::size_t x, std::size_t y)
        {
            return std::to_integer<int>(masks[(y * 160 + x) * 4]);
        };
        std::size_t clear_hole_samples{};
        std::size_t inner_outline_samples{};
        for (std::size_t y{1}; y < 95; ++y)
        {
            for (std::size_t x{1}; x < 159; ++x)
            {
                const auto fill{fill_at(x, y)};
                const auto outline{std::to_integer<int>(masks[(y * 160 + x) * 4 + 1])};
                EXPECT_LE(fill + outline, 256);
                if (fill != 0)
                {
                    continue;
                }
                bool left{}, right{}, above{}, below{};
                for (std::size_t i{}; i < x; ++i)
                {
                    left |= fill_at(i, y) > 200;
                }
                for (std::size_t i{x + 1}; i < 160; ++i)
                {
                    right |= fill_at(i, y) > 200;
                }
                for (std::size_t i{}; i < y; ++i)
                {
                    above |= fill_at(x, i) > 200;
                }
                for (std::size_t i{y + 1}; i < 96; ++i)
                {
                    below |= fill_at(x, i) > 200;
                }
                if (left && right && above && below)
                {
                    clear_hole_samples += outline == 0;
                    inner_outline_samples += outline > 32;
                }
            }
        }
        EXPECT_GT(clear_hole_samples, 0u);
        EXPECT_GT(inner_outline_samples, 0u);
    }
}

TEST_F(text_test, outline_expands_ink_without_changing_layout_metrics)
{
    const auto bytes{text_font_fixture("Text-Regular.ttf")};
    const auto font{text::create_font({bytes, 0})};
    ASSERT_TRUE(font);
    text::layout_desc desc{.font = font->get(), .font_size = 32.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}};
    const auto plain{text::create_layout(desc, "O")};
    ASSERT_TRUE(plain);
    desc.outline = text::outline_desc{2.5f, {1.0f, 0.0f, 0.0f, 1.0f}};
    const auto outlined{text::create_layout(desc, "O")};
    ASSERT_TRUE(outlined);
    EXPECT_FLOAT_EQ(text::metrics(plain->get()).width, text::metrics(outlined->get()).width);
    EXPECT_FLOAT_EQ(text::metrics(plain->get()).height, text::metrics(outlined->get()).height);
    const auto original{draw_layout(plain->get())};
    const auto pixels{draw_layout(outlined->get())};
    ASSERT_EQ(original.size(), pixels.size());
    std::size_t red_pixels{};
    std::size_t expanded_pixels{};
    for (std::size_t i{}; i < pixels.size(); i += 4)
    {
        red_pixels += std::to_integer<int>(pixels[i]) > std::to_integer<int>(pixels[i + 1]) + 32;
        expanded_pixels += original[i + 3] == std::byte{} && pixels[i + 3] > std::byte{32};
    }
    EXPECT_GT(red_pixels, 20u);
    EXPECT_GT(expanded_pixels, 20u);
}

TEST_F(text_test, outline_spans_inherit_override_and_disable_without_changing_shaping)
{
    const auto bytes{text_font_fixture("Text-Regular.ttf")};
    const auto font{text::create_font({bytes, 0})};
    ASSERT_TRUE(font);
    const text::outline_desc red{1.5f, {1.0f, 0.0f, 0.0f, 1.0f}};
    const text::outline_desc blue{3.0f, {0.0f, 0.0f, 1.0f, 1.0f}};
    text::layout_desc desc{.font = font->get(), .font_size = 32.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}, .outline = red};
    const std::array<text::item, 3> inherited{
        text::span{"A", {}},
        text::span{"B", {.outline = blue}},
        text::span{"O", {.outline = text::outline_desc{}}},
    };
    const std::array<text::item, 3> explicit_styles{
        text::span{"A", {.outline = red}},
        text::span{"B", {.outline = blue}},
        text::span{"O", {}},
    };
    const auto layout{text::create_layout(desc, inherited)};
    ASSERT_TRUE(layout);
    const auto pixels{draw_layout(layout->get())};
    desc.outline.reset();
    const auto expected{text::create_layout(desc, explicit_styles)};
    ASSERT_TRUE(expected);
    EXPECT_EQ(pixels, draw_layout(expected->get()));
    const auto plain{text::create_layout(desc, "ABO")};
    ASSERT_TRUE(plain);
    EXPECT_FLOAT_EQ(text::metrics(layout->get()).width, text::metrics(plain->get()).width);
    const std::array<text::item, 1> disabled{text::span{"ABO", {.outline = text::outline_desc{}}}};
    desc.outline = blue;
    ASSERT_TRUE(text::update(layout->get(), desc, disabled));
    EXPECT_EQ(draw_layout(layout->get()), draw_layout(plain->get()));
}

TEST_F(text_test, outline_effects_preserve_both_masks_and_apply_draw_tint)
{
    const auto bytes{text_font_fixture("Text-Regular.ttf")};
    const auto font{text::create_font({bytes, 0})};
    ASSERT_TRUE(font);
    const auto layout{text::create_layout({.font = font->get(), .font_size = 32.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}, .outline = text::outline_desc{2.5f, {1.0f, 0.0f, 0.0f, 1.0f}}}, "O")};
    ASSERT_TRUE(layout);
    const auto vertex{create_shader(std::as_bytes(std::span{text_identity_vs}))};
    const auto pixel{create_shader(std::as_bytes(std::span{text_identity_ps}))};
    const auto original{draw_layout(layout->get())};
    EXPECT_EQ(original, draw_layout(layout->get(), {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}, text::effect{vertex.get(), pixel.get(), {}}));
    const auto tinted{draw_layout(layout->get(), {0.5f, 1.0f, 1.0f, 0.5f})};
    ASSERT_EQ(tinted.size(), original.size());
    std::size_t ring_samples{};
    for (std::size_t i{}; i < original.size(); i += 4)
    {
        if (original[i] == std::byte{255} && original[i + 1] == std::byte{})
        {
            EXPECT_NEAR(std::to_integer<int>(tinted[i]), 64, 1);
            EXPECT_EQ(tinted[i + 1], std::byte{});
            EXPECT_NEAR(std::to_integer<int>(tinted[i + 3]), 128, 1);
            ++ring_samples;
        }
    }
    EXPECT_GT(ring_samples, 10u);
}

TEST_F(text_test, outline_groups_keep_overlapping_fill_in_front_and_obey_scissor)
{
    const auto bytes{text_font_fixture("Text-Regular.ttf")};
    const auto font{text::create_font({bytes, 0})};
    ASSERT_TRUE(font);
    text::layout_desc desc{.font = font->get(), .font_size = 36.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}};
    desc.outline = text::outline_desc{7.5f, {}};
    const auto plain{text::create_layout(desc, "OO")};
    ASSERT_TRUE(plain);
    desc.outline = text::outline_desc{7.5f, {1.0f, 0.0f, 0.0f, 1.0f}};
    const auto layout{text::create_layout(desc, "OO")};
    ASSERT_TRUE(layout);
    const auto fill{draw_layout(plain->get())};
    const auto pixels{draw_layout(layout->get())};
    ASSERT_EQ(fill.size(), pixels.size());
    for (std::size_t i{}; i < fill.size(); i += 4)
    {
        if (fill[i + 3] == std::byte{255})
        {
            EXPECT_EQ(pixels[i + 1], std::byte{255});
            EXPECT_EQ(pixels[i + 2], std::byte{255});
        }
    }
    const auto clipped{draw_layout(layout->get(), {}, {}, {}, {}, {}, [&](render_pass_handle pass)
    {
        const auto encoder{begin_encode(pass)};
        encoder->set_scissor_rect({20, 10, 20, 40});
        encoder->draw_text({layout->get(), {8.0f, 8.0f}, {1.0f, 1.0f, 1.0f, 1.0f}});
        end_encode(encoder);
    })};
    for (std::size_t y{}; y < 96; ++y)
    {
        for (std::size_t x{}; x < 160; ++x)
        {
            if (x < 20 || x >= 40 || y < 10 || y >= 50)
            {
                EXPECT_EQ(clipped[(y * 160 + x) * 4 + 3], std::byte{});
            }
        }
    }
}

TEST_F(text_test, small_outlined_text_keeps_both_masks_aligned_under_vertex_effects)
{
    const auto bytes{text_font_fixture("Text-Regular.ttf")};
    const auto font{text::create_font({bytes, 0})};
    ASSERT_TRUE(font);
    const auto vertex{create_shader(std::as_bytes(std::span{text_translate_vs}))};
    const core::vec4 translation{12.0f, 4.0f, 0.0f, 0.0f};
    for (const float size : {9.0f, 12.0f, 18.0f})
    {
        const auto layout{text::create_layout({.font = font->get(), .font_size = size, .color = {1.0f, 1.0f, 1.0f, 1.0f}, .outline = text::outline_desc{0.75f, {1.0f, 0.0f, 0.0f, 1.0f}}}, "Oo")};
        ASSERT_TRUE(layout);
        const auto expected{draw_layout(layout->get(), {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}, {}, {20.0f, 12.0f})};
        const auto moved{draw_layout(layout->get(), {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}, text::effect{vertex.get(), {}, std::as_bytes(std::span{&translation, 1})})};
        EXPECT_EQ(expected, moved);
        EXPECT_TRUE(std::ranges::any_of(expected, [](std::byte value)
        {
            return value != std::byte{};
        }));
    }
}

TEST_F(text_test, outline_and_fill_share_an_opaque_antialiased_boundary)
{
    const auto bytes{text_font_fixture("Text-Regular.ttf")};
    const auto font{text::create_font({bytes, 0})};
    ASSERT_TRUE(font);
    const auto pixel{create_shader(std::as_bytes(std::span{text_masks_ps}))};
    const auto layout{text::create_layout({.font = font->get(), .font_size = 32.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}, .outline = text::outline_desc{2.5f, {1.0f, 0.0f, 0.0f, 1.0f}}}, "Oo")};
    ASSERT_TRUE(layout);
    const auto masks{draw_layout(layout->get(), {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}, text::effect{{}, pixel.get(), {}})};
    const auto pixels{draw_layout(layout->get())};
    ASSERT_EQ(masks.size(), pixels.size());
    std::size_t shared_samples{};
    for (std::size_t i{}; i < pixels.size(); i += 4)
    {
        const auto fill{std::to_integer<int>(masks[i])};
        const auto outer{std::to_integer<int>(masks[i + 1])};
        if (fill > 32 && outer > 32 && fill + outer >= 253)
        {
            EXPECT_GE(std::to_integer<int>(pixels[i + 3]), 250);
            ++shared_samples;
        }
    }
    EXPECT_GT(shared_samples, 5u);
}

TEST_F(text_test, parallel_encoders_own_each_draws_constants)
{
    const auto bytes{text_font_fixture("Text-Regular.ttf")};
    const auto font{text::create_font({bytes, 0})};
    ASSERT_TRUE(font);
    const auto layout{text::create_layout({.font = font->get(), .font_size = 32.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}}, "A")};
    ASSERT_TRUE(layout);
    const auto shader{create_shader(std::as_bytes(std::span{text_effect_ps})).release()};
    const auto pixels{draw_layout(layout->get(), {}, {}, [&]()
    {
        destroy(shader);
    }, {}, {}, [&](render_pass_handle pass)
    {
        std::array<std::jthread, 2> workers;
        for (std::size_t i{}; i < workers.size(); ++i)
        {
            workers[i] = std::jthread{[&, i]()
            {
                const auto encoder{begin_encode(pass)};
                for (std::size_t draw{}; draw < 32; ++draw)
                {
                    std::vector<float> parameters{i == 0 ? 1.0f : 0.0f, 0.0f, i == 0 ? 0.0f : 1.0f, 1.0f};
                    encoder->draw_text({layout->get(), {8.0f + static_cast<float>(i) * 50.0f, 8.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, text::effect{{}, shader, std::as_bytes(std::span{parameters})}});
                    std::ranges::fill(parameters, 0.0f);
                }
                end_encode(encoder);
            }};
        }
    })};
    ASSERT_EQ(pixels.size(), 160u * 96u * 4u);
    const auto left{(20 * 160 + 15) * 4};
    const auto right{(20 * 160 + 65) * 4};
    EXPECT_EQ(pixels[left], std::byte{255});
    EXPECT_EQ(pixels[left + 1], std::byte{});
    EXPECT_EQ(pixels[left + 2], std::byte{});
    EXPECT_EQ(pixels[right], std::byte{});
    EXPECT_EQ(pixels[right + 1], std::byte{});
    EXPECT_EQ(pixels[right + 2], std::byte{255});
}

TEST_F(text_test, effects_use_layout_at_replay_and_allow_shader_release_afterwards)
{
    const auto bytes{text_font_fixture("Text-Regular.ttf")};
    const auto font{text::create_font({bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(font);
    const auto layout{text::create_layout({.font = *font, .font_size = 32.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}}, "A").transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto reference{text::create_layout({.font = *font, .font_size = 32.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}}, "B").transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(layout && reference);
    const auto expected{draw_layout(*reference)};
    const auto shader{create_shader(std::as_bytes(std::span{text_identity_ps})).release()};
    const auto actual{draw_layout(*layout, {1.0f, 1.0f, 1.0f, 1.0f}, [&]()
    {
        EXPECT_TRUE(text::update(*layout, "B"));
    }, [&]()
    {
        destroy(shader);
    }, text::effect{{}, shader, {}})};
    EXPECT_EQ(actual, expected);
    const auto replacement{create_shader(std::as_bytes(std::span{text_effect_ps})).release()};
    const core::vec4 parameters{1.0f, 0.0f, 0.0f, 1.0f};
    const auto replaced{draw_layout(*layout, {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}, text::effect{{}, replacement, std::as_bytes(std::span{&parameters, 1})})};
    ASSERT_EQ(replaced.size(), expected.size());
    for (std::size_t i{}; i < expected.size(); i += 4)
    {
        EXPECT_EQ(replaced[i], expected[i]);
        EXPECT_EQ(replaced[i + 1], std::byte{});
        EXPECT_EQ(replaced[i + 2], std::byte{});
        EXPECT_EQ(replaced[i + 3], expected[i + 3]);
    }
    destroy(replacement);
    text::destroy(*reference);
    text::destroy(*layout);
    text::destroy(*font);
}

TEST_F(text_test, effects_preserve_image_decoration_and_overlapping_batch_order)
{
    const auto bytes{text_font_fixture("Text-Overlap.ttf")};
    const auto font{text::create_font({bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto image{create_texture({texture_format::rgba8_unorm, 1, 1, 1, 1, texture_usage::shader_resource | texture_usage::copy_destination}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(font && image);
    const std::array<std::uint32_t, 1> source{0xFF0000FFu};
    ASSERT_TRUE(update(*image, {0, 0, 1, 1, 0}, {std::as_bytes(std::span{source}), 4}));
    const std::array<text::item, 3> items{
        text::span{"A", {.underline = true, .strikethrough = true, .color = core::vec4{0.0f, 0.0f, 1.0f, 1.0f}}},
        text::inline_image{*image, {16.0f, 40.0f}},
        text::span{"B", {.color = core::vec4{0.0f, 0.0f, 1.0f, 1.0f}}}
    };
    const auto layout{text::create_layout({.font = *font, .font_size = 40.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}}, items).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(layout);
    const auto baseline{draw_layout(*layout)};
    const auto shader{create_shader(std::as_bytes(std::span{text_effect_ps})).release()};
    const core::vec4 parameters{0.0f, 1.0f, 0.0f, 1.0f};
    const auto pixels{draw_layout(*layout, {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}, text::effect{{}, shader, std::as_bytes(std::span{&parameters, 1})})};
    ASSERT_EQ(pixels.size(), baseline.size());
    for (const std::size_t x : {34u, 42u})
    {
        const auto offset{(32 * 160 + x) * 4};
        EXPECT_EQ(pixels[offset], std::byte{255});
        EXPECT_EQ(pixels[offset + 1], std::byte{});
    }
    EXPECT_EQ(pixels[(32 * 160 + 20) * 4 + 1], std::byte{255});
    EXPECT_EQ(pixels[(32 * 160 + 50) * 4 + 1], std::byte{255});
    std::size_t decoration_pixels{};
    for (std::size_t i{}; i < pixels.size(); i += 4)
    {
        if (pixels[i + 2] != std::byte{})
        {
            ++decoration_pixels;
            EXPECT_EQ(pixels[i], baseline[i]);
            EXPECT_EQ(pixels[i + 1], baseline[i + 1]);
            EXPECT_EQ(pixels[i + 2], baseline[i + 2]);
            EXPECT_EQ(pixels[i + 3], baseline[i + 3]);
        }
    }
    EXPECT_GT(decoration_pixels, 20u);
    const auto outlined{text::create_layout({.font = *font, .font_size = 40.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}, .outline = text::outline_desc{2.5f, {0.0f, 1.0f, 0.0f, 1.0f}}}, items)};
    ASSERT_TRUE(outlined);
    const auto outlined_pixels{draw_layout(outlined->get())};
    for (const std::size_t x : {34u, 42u})
    {
        const auto offset{(32 * 160 + x) * 4};
        EXPECT_EQ(outlined_pixels[offset], std::byte{255});
        EXPECT_EQ(outlined_pixels[offset + 1], std::byte{});
        EXPECT_EQ(outlined_pixels[offset + 2], std::byte{});
    }
    destroy(shader);
    text::destroy(*layout);
    destroy(*image);
    text::destroy(*font);
}

TEST_F(text_test, effect_stages_independently_reuse_defaults_without_constants)
{
    const auto bytes{text_font_fixture("Text-Regular.ttf")};
    const auto font{text::create_font({bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(font);
    const auto layout{text::create_layout({.font = *font, .font_size = 40.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}}, "AB").transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(layout);
    const auto expected{draw_layout(*layout)};
    const auto vertex{create_shader(std::as_bytes(std::span{text_identity_vs})).release()};
    const auto pixel{create_shader(std::as_bytes(std::span{text_identity_ps})).release()};
    for (const auto effect : {text::effect{vertex, {}, {}}, text::effect{{}, pixel, {}}, text::effect{vertex, pixel, {}}})
    {
        EXPECT_EQ(draw_layout(*layout, {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}, effect), expected);
    }
    destroy(pixel);
    destroy(vertex);
    const auto empty{text::create_layout({.font = *font, .font_size = 40.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}}, "").transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(empty);
    const auto empty_pixels{draw_layout(*empty)};
    EXPECT_TRUE(std::ranges::all_of(empty_pixels, [](std::byte value)
    {
        return value == std::byte{};
    }));
    text::destroy(*empty);
    text::destroy(*layout);
    text::destroy(*font);
}

TEST_F(text_test, effect_color_preserves_tint_alpha_and_recorded_constants)
{
    const auto bytes{text_font_fixture("Text-Regular.ttf")};
    const auto font{text::create_font({bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(font);
    const std::array<text::item, 1> items{text::span{"A", {.color = core::vec4{0.0f, 1.0f, 0.0f, 0.5f}}}};
    const auto layout{text::create_layout({.font = *font, .font_size = 40.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}}, items).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(layout);
    const auto shader{create_shader(std::as_bytes(std::span{text_effect_ps})).release()};
    core::vec4 parameters{1.0f, 0.0f, 0.0f, 1.0f};
    const auto pixels{draw_layout(*layout, {0.5f, 0.25f, 1.0f, 0.5f}, [&parameters]()
    {
        parameters = {0.0f, 1.0f, 0.0f, 0.0f};
    }, {}, text::effect{{}, shader, std::as_bytes(std::span{&parameters, 1})})};
    ASSERT_FALSE(pixels.empty());
    int red_max{};
    int alpha_max{};
    for (std::size_t i{0}; i < pixels.size(); i += 4)
    {
        red_max = std::max(red_max, std::to_integer<int>(pixels[i]));
        alpha_max = std::max(alpha_max, std::to_integer<int>(pixels[i + 3]));
        EXPECT_EQ(pixels[i + 1], std::byte{});
        EXPECT_EQ(pixels[i + 2], std::byte{});
    }
    EXPECT_NEAR(red_max, 32, 1);
    EXPECT_NEAR(alpha_max, 64, 1);
    destroy(shader);
    text::destroy(*layout);
    text::destroy(*font);
}

TEST_F(text_test, rich_text_matches_plain_text)
{
    const auto bytes{load_test_font()};
    ASSERT_FALSE(bytes.empty());
    const auto font{text::create_font({bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(font);
    const text::layout_desc desc{.font = *font, .font_size = 28.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}};
    const auto plain{text::create_layout(desc, "한글 ABC").transform([](auto&& owner)
    {
        return owner.release();
    })};
    const std::array<text::item, 2> items{text::span{"한글 ", {}}, text::span{"ABC", {}}};
    const auto rich{text::create_layout(desc, items).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(plain && rich);
    EXPECT_FLOAT_EQ(text::metrics(*plain).width, text::metrics(*rich).width);
    EXPECT_FLOAT_EQ(text::metrics(*plain).height, text::metrics(*rich).height);
    EXPECT_EQ(text::metrics(*plain).line_count, text::metrics(*rich).line_count);
    EXPECT_EQ(draw_layout(*plain), draw_layout(*rich));
    text::destroy(*rich);
    text::destroy(*plain);
    text::destroy(*font);
}

TEST_F(text_test, spans_inherit_or_override_layout_color_and_multiply_draw_tint)
{
    const auto bytes{load_test_font()};
    const auto font{text::create_font({bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(font);
    const std::array<text::item, 2> items{
        text::span{"A", {}},
        text::span{"B", {.color = core::vec4{0.0f, 1.0f, 0.0f, 1.0f}}}
    };
    const auto layout{text::create_layout({.font = *font, .font_size = 32.0f, .color = {1.0f, 0.0f, 0.0f, 1.0f}}, items).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(layout);
    const auto pixels{draw_layout(*layout, {0.5f, 0.25f, 1.0f, 0.5f})};
    ASSERT_FALSE(pixels.empty());
    std::uint8_t red_max{};
    std::uint8_t green_max{};
    for (std::size_t i{0}; i < pixels.size(); i += 4)
    {
        const auto red{std::to_integer<std::uint8_t>(pixels[i])};
        const auto green{std::to_integer<std::uint8_t>(pixels[i + 1])};
        EXPECT_TRUE(red == 0 || green == 0);
        EXPECT_EQ(pixels[i + 2], std::byte{});
        red_max = std::max(red_max, red);
        green_max = std::max(green_max, green);
    }
    EXPECT_NEAR(red_max, 64, 2);
    EXPECT_NEAR(green_max, 32, 2);
    text::destroy(*layout);
    text::destroy(*font);
}

TEST_F(text_test, spans_select_fonts_and_sizes)
{
    const auto bytes{load_test_font()};
    const auto first{text::create_font({bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto second{text::create_font({bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(first && second);
    const auto small_layout{text::create_layout({.font = *first, .font_size = 16.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}}, "A").transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto large_layout{text::create_layout({.font = *second, .font_size = 32.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}}, "B").transform([](auto&& owner)
    {
        return owner.release();
    })};
    const std::array<text::item, 2> items{
        text::span{"A", {}},
        text::span{"B", {.font = *second, .font_size = 32.0f}}
    };
    const auto mixed{text::create_layout({.font = *first, .font_size = 16.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}}, items).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(small_layout && large_layout && mixed);
    EXPECT_NEAR(text::metrics(*mixed).width, text::metrics(*small_layout).width + text::metrics(*large_layout).width, 0.01f);
    EXPECT_GE(text::metrics(*mixed).height, text::metrics(*large_layout).height);
    EXPECT_FALSE(draw_layout(*mixed).empty());
    text::destroy(*mixed);
    text::destroy(*large_layout);
    text::destroy(*small_layout);
    text::destroy(*second);
    text::destroy(*first);
}

TEST_F(text_test, decorations_preserve_span_and_draw_alpha)
{
    const auto bytes{load_test_font()};
    const auto font{text::create_font({bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(font);
    const std::array<text::item, 1> items{text::span{" A", {.underline = true, .strikethrough = true, .color = core::vec4{0.0f, 1.0f, 0.0f, 0.5f}}}};
    const auto layout{text::create_layout({.font = *font, .font_size = 40.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}}, items).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(layout);
    const auto plain{text::create_layout({.font = *font, .font_size = 40.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}}, " A").transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(plain);
    const auto plain_pixels{draw_layout(*plain)};
    const auto pixels{draw_layout(*layout, {0.5f, 0.25f, 1.0f, 0.5f})};
    ASSERT_EQ(pixels.size(), 160u * 96u * 4u);
    ASSERT_EQ(plain_pixels.size(), pixels.size());
    std::size_t decoration_pixels{};
    for (std::size_t i{}; i < pixels.size(); i += 4)
    {
        EXPECT_EQ(pixels[i], std::byte{});
        EXPECT_EQ(pixels[i + 2], std::byte{});
        if (pixels[i + 3] != std::byte{} && plain_pixels[i + 3] == std::byte{})
        {
            ++decoration_pixels;
            EXPECT_NEAR(std::to_integer<int>(pixels[i + 1]), 16, 1);
            EXPECT_NEAR(std::to_integer<int>(pixels[i + 3]), 64, 1);
        }
    }
    EXPECT_GE(decoration_pixels, 20u);
    text::destroy(*plain);
    text::destroy(*layout);
    text::destroy(*font);
}

TEST_F(text_test, decorations_add_colored_lines)
{
    const auto bytes{load_test_font()};
    const auto font{text::create_font({bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(font);
    const std::array<text::item, 1> items{text::span{" A", {.underline = true, .strikethrough = true, .color = core::vec4{0.0f, 1.0f, 0.0f, 1.0f}}}};
    const auto layout{text::create_layout({.font = *font, .font_size = 32.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}}, items).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(layout);
    const auto pixels{draw_layout(*layout)};
    ASSERT_FALSE(pixels.empty());
    const auto plain{text::create_layout({.font = *font, .font_size = 32.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}}, " A").transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(plain);
    const auto plain_pixels{draw_layout(*plain)};
    ASSERT_EQ(pixels.size(), plain_pixels.size());
    std::size_t added_pixels{};
    for (std::size_t i{0}; i < pixels.size(); i += 4)
    {
        EXPECT_EQ(pixels[i], std::byte{});
        EXPECT_EQ(pixels[i + 2], std::byte{});
        added_pixels += pixels[i + 1] != std::byte{} && plain_pixels[i + 3] == std::byte{};
    }
    EXPECT_GE(added_pixels, 20u);
    text::destroy(*plain);
    text::destroy(*layout);
    text::destroy(*font);
}

TEST_F(text_test, inline_images_apply_tint_and_wrap_as_indivisible_items)
{
    const auto bytes{load_test_font()};
    const auto font{text::create_font({bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(font);
    const auto image{create_texture({texture_format::rgba8_unorm, 1, 1, 1, 1, texture_usage::shader_resource | texture_usage::copy_destination}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(image);
    const std::array<std::uint32_t, 1> source{0x804080FFu};
    ASSERT_TRUE(update(*image, {0, 0, 1, 1, 0}, {std::as_bytes(std::span{source}), 4}));
    const std::array<text::item, 3> items{text::span{"", {.color = core::vec4{0.0f, 1.0f, 0.0f, 1.0f}}}, text::inline_image{*image, {16.0f, 20.0f}}, text::inline_image{*image, {16.0f, 20.0f}}};
    const text::layout_desc desc{.font = *font, .font_size = 16.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}, .max_width = 17.0f, .wrapping_mode = text::wrapping::wrap};
    const auto layout{text::create_layout(desc, items).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(layout);
    EXPECT_EQ(text::metrics(*layout).line_count, 2u);
    EXPECT_FLOAT_EQ(text::metrics(*layout).width, 16.0f);
    EXPECT_GE(text::metrics(*layout).first_baseline, 20.0f);
    const auto pixels{draw_layout(*layout, {0.5f, 0.25f, 1.0f, 0.5f})};
    ASSERT_FALSE(pixels.empty());
    const auto offset{(static_cast<std::size_t>(8.0f + text::metrics(*layout).first_baseline - 10.0f) * 160 + 12) * 4};
    EXPECT_NEAR(std::to_integer<int>(pixels[offset]), 32, 1);
    EXPECT_NEAR(std::to_integer<int>(pixels[offset + 1]), 8, 1);
    EXPECT_NEAR(std::to_integer<int>(pixels[offset + 2]), 16, 1);
    EXPECT_NEAR(std::to_integer<int>(pixels[offset + 3]), 64, 1);
    auto no_wrap{desc};
    no_wrap.wrapping_mode = text::wrapping::none;
    ASSERT_TRUE(update(*layout, no_wrap, items));
    EXPECT_EQ(text::metrics(*layout).line_count, 1u);
    EXPECT_FLOAT_EQ(text::metrics(*layout).width, 32.0f);
    text::destroy(*layout);
    destroy(*image);
    text::destroy(*font);
}

TEST_F(text_test, collection_selects_exact_styles_from_the_same_file)
{
    const auto collection_bytes{text_font_fixture("Text-Styles.ttc")};
    const auto bold_bytes{text_font_fixture("Text-Bold.ttf")};
    const auto family{text::create_font({collection_bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto bold{text::create_font({bold_bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(family && bold);
    const auto requested{text::create_layout({.font = *family, .font_size = 40.0f, .bold = true, .color = {1.0f, 1.0f, 1.0f, 1.0f}}, "AB").transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto reference{text::create_layout({.font = *bold, .font_size = 40.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}}, "AB").transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(requested && reference);
    EXPECT_FLOAT_EQ(text::metrics(*requested).width, text::metrics(*reference).width);
    EXPECT_EQ(draw_layout(*requested), draw_layout(*reference));
    text::destroy(*requested);
    text::destroy(*reference);
    text::destroy(*bold);
    text::destroy(*family);
}

TEST_F(text_test, variable_font_axes_match_actual_static_styles)
{
    const auto bytes{text_font_fixture("Text-Variable.ttf")};
    const auto font{text::create_font({bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(font);

    struct style_case
    {
        bool bold;
        bool italic;
        std::string_view reference;
    };

    const std::array cases{
        style_case{false, false, "Text-Regular.ttf"},
        style_case{true, false, "Text-Bold.ttf"},
        style_case{false, true, "Text-Italic.ttf"},
        style_case{true, true, "Text-BoldItalic.ttf"},
    };
    for (const auto& entry : cases)
    {
        SCOPED_TRACE(entry.reference);
        const auto reference_bytes{text_font_fixture(entry.reference)};
        const auto reference_font{text::create_font({reference_bytes, 0}).transform([](auto&& owner)
        {
            return owner.release();
        })};
        ASSERT_TRUE(reference_font);
        const auto actual{text::create_layout({.font = *font, .font_size = 40.0f, .bold = entry.bold, .italic = entry.italic, .color = {1.0f, 1.0f, 1.0f, 1.0f}}, "AB").transform([](auto&& owner)
        {
            return owner.release();
        })};
        const auto reference{text::create_layout({.font = *reference_font, .font_size = 40.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}}, "AB").transform([](auto&& owner)
        {
            return owner.release();
        })};
        ASSERT_TRUE(actual && reference);
        EXPECT_NEAR(text::metrics(*actual).width, text::metrics(*reference).width, 0.001f);
        EXPECT_EQ(draw_layout(*actual), draw_layout(*reference));
        text::destroy(*reference);
        text::destroy(*actual);
        text::destroy(*reference_font);
    }
    text::destroy(*font);
}

TEST_F(text_test, mixed_collection_matches_effective_variable_axes)
{
    const auto bytes{text_font_fixture("Text-Mixed.ttc")};
    const auto reference_bytes{text_font_fixture("Text-BoldItalic.ttf")};
    const auto font{text::create_font({bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto reference_font{text::create_font({reference_bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(font && reference_font);
    const auto actual{text::create_layout({.font = *font, .font_size = 40.0f, .italic = true, .color = {1.0f, 1.0f, 1.0f, 1.0f}}, "AB").transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto reference{text::create_layout({.font = *reference_font, .font_size = 40.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}}, "AB").transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(actual && reference);
    EXPECT_FLOAT_EQ(text::metrics(*actual).width, text::metrics(*reference).width);
    EXPECT_EQ(draw_layout(*actual), draw_layout(*reference));
    text::destroy(*reference);
    text::destroy(*actual);
    text::destroy(*reference_font);
    text::destroy(*font);
}

TEST_F(text_test, unsupported_style_combinations_keep_the_reference_face)
{
    struct fallback_case
    {
        std::string_view filename;
        std::uint32_t face_index;
        bool bold;
        bool italic;
    };

    const std::array cases{
        fallback_case{"Text-Regular.ttf", 0, true, true},
        fallback_case{"Text-Partial.ttc", 0, false, true},
        fallback_case{"Text-Partial.ttc", 0, true, true},
        fallback_case{"Text-Styles.ttc", 1, false, false},
        fallback_case{"Text-Limited.ttf", 0, true, false},
        fallback_case{"Text-Slant.ttf", 0, false, true},
    };
    const auto external_bold_bytes{text_font_fixture("Text-BoldItalic.ttf")};
    const auto external_font{text::create_font({external_bold_bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(external_font);
    for (const auto& entry : cases)
    {
        SCOPED_TRACE(entry.filename);
        const auto bytes{text_font_fixture(entry.filename)};
        const auto font{text::create_font({bytes, entry.face_index}).transform([](auto&& owner)
        {
            return owner.release();
        })};
        ASSERT_TRUE(font);
        const auto reference{text::create_layout({.font = *font, .font_size = 40.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}}, "AB").transform([](auto&& owner)
        {
            return owner.release();
        })};
        const auto requested{text::create_layout({.font = *font, .font_size = 40.0f, .bold = entry.bold, .italic = entry.italic, .color = {1.0f, 1.0f, 1.0f, 1.0f}}, "AB").transform([](auto&& owner)
        {
            return owner.release();
        })};
        ASSERT_TRUE(reference && requested);
        EXPECT_FLOAT_EQ(text::metrics(*reference).width, text::metrics(*requested).width);
        EXPECT_EQ(draw_layout(*reference), draw_layout(*requested));
        text::destroy(*requested);
        text::destroy(*reference);
        text::destroy(*font);
    }
    text::destroy(*external_font);
}

TEST_F(text_test, cff_collection_uses_real_bold_face)
{
    const auto family_bytes{text_font_fixture("Text-Styles.otc")};
    const auto reference_bytes{text_font_fixture("Text-Bold.otf")};
    const auto family{text::create_font({family_bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto bold{text::create_font({reference_bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(family && bold);
    const auto requested{text::create_layout({.font = *family, .font_size = 40.0f, .bold = true, .color = {1.0f, 1.0f, 1.0f, 1.0f}}, "AB").transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto reference{text::create_layout({.font = *bold, .font_size = 40.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}}, "AB").transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(requested && reference);
    EXPECT_FLOAT_EQ(text::metrics(*requested).width, text::metrics(*reference).width);
    EXPECT_EQ(draw_layout(*requested), draw_layout(*reference));
    text::destroy(*reference);
    text::destroy(*requested);
    text::destroy(*bold);
    text::destroy(*family);
}

TEST_F(text_test, failed_update_keeps_metrics_and_pixels)
{
    const auto bytes{text_font_fixture("Text-Regular.ttf")};
    const auto font{text::create_font({bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(font);
    const text::layout_desc desc{.font = *font, .font_size = 32.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}};
    const auto layout{text::create_layout(desc, "AB").transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(layout);
    const auto original_metrics{text::metrics(*layout)};
    const auto original_pixels{draw_layout(*layout)};
    const std::string invalid_utf8{"\xFF"};
    const auto malformed{text::update(*layout, invalid_utf8)};
    ASSERT_FALSE(malformed);
    EXPECT_EQ(malformed.error(), error_code::invalid_argument);
    EXPECT_FLOAT_EQ(text::metrics(*layout).width, original_metrics.width);
    EXPECT_FLOAT_EQ(text::metrics(*layout).height, original_metrics.height);
    EXPECT_EQ(draw_layout(*layout), original_pixels);
    ASSERT_TRUE(update(*layout, std::span<const text::item>{}));
    EXPECT_FLOAT_EQ(text::metrics(*layout).width, 0.0f);
    const auto empty_pixels{draw_layout(*layout)};
    EXPECT_TRUE(std::ranges::all_of(empty_pixels, [](std::byte pixel)
    {
        return pixel == std::byte{};
    }));
    text::destroy(*layout);
    text::destroy(*font);
}

TEST_F(text_test, missing_glyphs_keep_the_explicit_font)
{
    const auto bytes{text_font_fixture("Text-Regular.ttf")};
    const auto registered_bytes{load_test_font()};
    const auto font{text::create_font({bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto registered{text::create_font({registered_bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(font && registered);
    const text::layout_desc desc{.font = *font, .font_size = 32.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}};
    const auto missing{text::create_layout(desc, "한").transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto unknown{text::create_layout(desc, "\xF4\x8F\xBF\xBF").transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto replacement{text::create_layout(desc, "\xEF\xBF\xBD").transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(missing && unknown && replacement);
    EXPECT_EQ(draw_layout(*missing), draw_layout(*unknown));
    EXPECT_NE(draw_layout(*missing), draw_layout(*replacement));
    text::destroy(*replacement);
    text::destroy(*unknown);
    text::destroy(*missing);
    text::destroy(*registered);
    text::destroy(*font);
}

TEST_F(text_test, rich_update_is_resolved_at_pass_end_and_released_after_recording)
{
    const auto bytes{text_font_fixture("Text-Styles.ttc")};
    const auto font{text::create_font({bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(font);
    const text::layout_desc desc{.font = *font, .font_size = 32.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}};
    const auto layout{text::create_layout(desc, "A").transform([](auto&& owner)
    {
        return owner.release();
    })};
    const std::array<text::item, 2> items{
        text::span{"B", {.bold = true, .color = core::vec4{1.0f, 0.0f, 0.0f, 1.0f}}},
        text::span{"C", {.font_size = 40.0f, .underline = true}}
    };
    const auto reference{text::create_layout(desc, items).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(layout && reference);
    const auto expected{draw_layout(*reference)};
    const auto actual{draw_layout(*layout, {1.0f, 1.0f, 1.0f, 1.0f}, [&]()
    {
        EXPECT_TRUE(update(*layout, items));
    }, [&]()
    {
        text::destroy(*layout);
        text::destroy(*reference);
        text::destroy(*font);
    })};
    EXPECT_EQ(actual, expected);
}

TEST_F(text_test, atlas_churn_preserves_live_layouts)
{
    const auto bytes{load_test_font()};
    const auto font{text::create_font({bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(font);
    const auto stable{text::create_layout({.font = *font, .font_size = 32.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}}, "살아 있는 ABC").transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto temporary{text::create_layout({.font = *font, .font_size = 192.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}}, "").transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(stable && temporary);
    const auto expected{draw_layout(*stable)};
    for (std::uint32_t round{0}; round < 2; ++round)
    {
        SCOPED_TRACE(round);
        std::string text;
        for (std::uint32_t index{0}; index < 180; ++index)
        {
            const std::uint32_t code{0xAC00 + round * 180 + index};
            text.push_back(static_cast<char>(0xE0 | (code >> 12)));
            text.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            text.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
        ASSERT_TRUE(text::update(*temporary, text));
        EXPECT_FALSE(draw_layout(*temporary).empty());
        ASSERT_TRUE(text::update(*temporary, ""));
        EXPECT_EQ(draw_layout(*stable), expected);
    }
    text::destroy(*temporary);
    text::destroy(*stable);
    text::destroy(*font);
}

TEST_F(text_test, overlap_preserves_directwrite_callback_order)
{
    const auto bytes{text_font_fixture("Text-Overlap.ttf")};
    const auto font{text::create_font({bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto image{create_texture({texture_format::rgba8_unorm, 1, 1, 1, 1, texture_usage::shader_resource | texture_usage::copy_destination}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(font && image);
    const std::array<std::uint32_t, 1> source{0xFF0000FFu};
    ASSERT_TRUE(update(*image, {0, 0, 1, 1, 0}, {std::as_bytes(std::span{source}), 4}));
    const std::array<text::item, 3> items{
        text::span{"A", {}},
        text::inline_image{*image, {16.0f, 40.0f}},
        text::span{"B", {.color = core::vec4{0.0f, 0.0f, 1.0f, 1.0f}}}
    };
    const auto layout{text::create_layout({.font = *font, .font_size = 40.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}}, items).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(layout);
    const auto pixels{draw_layout(*layout)};
    ASSERT_EQ(pixels.size(), 160u * 96u * 4u);
    const auto pixel = [&pixels](std::size_t x)
    {
        std::array<std::byte, 4> color;
        std::memcpy(color.data(), pixels.data() + (32 * 160 + x) * 4, 4);
        return color;
    };
    EXPECT_EQ(pixel(20), (std::array{std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}}));
    EXPECT_EQ(pixel(34), (std::array{std::byte{255}, std::byte{0}, std::byte{0}, std::byte{255}}));
    EXPECT_EQ(pixel(42), (std::array{std::byte{255}, std::byte{0}, std::byte{0}, std::byte{255}}));
    EXPECT_EQ(pixel(50), (std::array{std::byte{0}, std::byte{0}, std::byte{255}, std::byte{255}}));
    text::destroy(*layout);
    destroy(*image);
    text::destroy(*font);
}

TEST_F(text_test, span_boundaries_preserve_combining_and_right_to_left_shaping)
{
    std::array<wchar_t, MAX_PATH> directory;
    ASSERT_GT(::GetWindowsDirectoryW(directory.data(), static_cast<UINT>(directory.size())), 0u);
    const auto bytes{load_test_font(std::filesystem::path{directory.data()} / "Fonts" / "segoeui.ttf")};
    ASSERT_FALSE(bytes.empty());
    const auto font{text::create_font({bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(font);
    const auto vertex{create_shader(std::as_bytes(std::span{text_identity_vs})).release()};
    const auto pixel{create_shader(std::as_bytes(std::span{text_effect_ps})).release()};
    const core::vec4 parameters{1.0f, 0.0f, 0.0f, 1.0f};
    const text::effect effect{vertex, pixel, std::as_bytes(std::span{&parameters, 1})};
    const std::array<text::item, 5> items{text::span{"of", {}}, text::span{"fi", {}}, text::span{"ce e", {}}, text::span{"\xCC\x81", {}}, text::span{" אב", {}}};
    for (const auto alignment : {text::alignment::left, text::alignment::center, text::alignment::right})
    {
        const text::layout_desc desc{.font = *font, .font_size = 24.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}, .max_width = 100.0f, .alignment_mode = alignment, .wrapping_mode = text::wrapping::wrap};
        const auto plain{text::create_layout(desc, "office e\xCC\x81 אב").transform([](auto&& owner)
        {
            return owner.release();
        })};
        const auto rich{text::create_layout(desc, items).transform([](auto&& owner)
        {
            return owner.release();
        })};
        ASSERT_TRUE(plain && rich);
        EXPECT_FLOAT_EQ(text::metrics(*plain).width, text::metrics(*rich).width);
        EXPECT_EQ(text::metrics(*plain).line_count, text::metrics(*rich).line_count);
        EXPECT_EQ(draw_layout(*plain), draw_layout(*rich));
        const auto plain_effect{draw_layout(*plain, {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}, effect)};
        EXPECT_EQ(plain_effect, draw_layout(*rich, {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}, effect));
        EXPECT_NE(plain_effect, draw_layout(*plain));
        text::destroy(*rich);
        text::destroy(*plain);
    }
    destroy(pixel);
    destroy(vertex);
    text::destroy(*font);
}

TEST_F(text_test, text_rectangles_and_lines_accept_formats_samples_and_multiple_attachments)
{
    const auto bytes{text_font_fixture("Text-Variable.ttf")};
    const auto font{text::create_font({bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(font);
    const std::array<text::item, 2> items{text::span{"A", {.bold = true, .underline = true}}, text::span{"B", {.italic = true}}};
    const auto layout{text::create_layout({.font = *font, .font_size = 24.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}}, items).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(layout);
    const auto vertex{create_shader(std::as_bytes(std::span{text_identity_vs})).release()};
    const auto pixel{create_shader(std::as_bytes(std::span{text_identity_ps})).release()};
    for (const auto format : {texture_format::rgba8_unorm, texture_format::rgba16_float})
    {
        for (const std::uint32_t samples : {1u, 4u})
        {
            const auto first{create_texture({format, 64, 48, 1, samples, texture_usage::render_target}).transform([](auto&& owner)
            {
                return owner.release();
            })};
            const auto second{create_texture({format, 64, 48, 1, samples, texture_usage::render_target}).transform([](auto&& owner)
            {
                return owner.release();
            })};
            const auto depth{create_texture({texture_format::d32_float, 64, 48, 1, samples, texture_usage::depth_stencil}).transform([](auto&& owner)
            {
                return owner.release();
            })};
            ASSERT_TRUE(first && second && depth);
            ASSERT_TRUE(begin_frame());
            const auto pass{begin_render_pass({.render_targets = {{*first, {begin_op::clear}, {end_op::store}}, {*second, {begin_op::clear}, {end_op::store}}}, .depth_stencil = {{*depth, {begin_op::clear, {.depth = 1.0f}}, {begin_op::clear}, {end_op::store}, {end_op::store}}}})};
            const auto encoder{begin_encode(pass)};
            encoder->draw_rect({{0.0f, 0.0f}, {64.0f, 48.0f}, {0.25f, 0.5f, 0.75f, 1.0f}});
            encoder->draw_line({{4.0f, 4.0f}, {60.0f, 40.0f}, 2.0f, {1.0f, 0.0f, 0.0f, 0.5f}});
            encoder->draw_text({*layout, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}});
            encoder->draw_text({*layout, {24.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, text::effect{vertex, pixel, {}}});
            encoder->draw_rect({{48.5f, 24.5f}, {8.0f, 8.0f}, {1.0f, 0.0f, 0.0f, 0.5f}});
            encoder->draw_line({{60.0f, 4.0f}, {4.0f, 40.0f}, 0.5f, {0.0f, 1.0f, 0.0f, 1.0f}});
            end_encode(encoder);
            ASSERT_TRUE(end_render_pass(pass));
            ASSERT_TRUE(end_frame());
            destroy(*depth);
            destroy(*second);
            destroy(*first);
        }
    }
    destroy(pixel);
    destroy(vertex);
    text::destroy(*layout);
    text::destroy(*font);
}
} // namespace nori::graphics
