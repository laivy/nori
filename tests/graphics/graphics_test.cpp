#include <dwrite.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <thread>
#include <vector>

#include <wrl/client.h>

#include <gtest/gtest.h>

#include <nori/graphics.hpp>
#include <nori/graphics/command_encoder.hpp>
#include <nori/graphics/render_pass.hpp>
#include <nori/resource.hpp>

#include "graphics_test_helpers.hpp"

namespace
{
std::optional<nori::graphics::texture_readback> wait_readback(nori::graphics::readback_handle handle)
{
    if (!nori::graphics::submit())
    {
        return std::nullopt;
    }
    const auto deadline{std::chrono::steady_clock::now() + std::chrono::seconds{5}};
    do
    {
        auto pixels{nori::graphics::poll(handle)};
        if (pixels.status == nori::graphics::readback_status::failed)
        {
            return std::nullopt;
        }
        if (pixels.status == nori::graphics::readback_status::ready)
        {
            return std::move(pixels.value);
        }
        std::this_thread::yield();
    } while (std::chrono::steady_clock::now() < deadline);
    return std::nullopt;
}
} // namespace

namespace nori::graphics
{
static_assert(std::same_as<decltype(create_sampler(sampler_desc{})), unique_sampler_handle>);
static_assert(std::same_as<decltype(create_shader({})), unique_shader_handle>);
static_assert(std::same_as<decltype(create_graphics_pipeline(graphics_pipeline_desc{})), unique_pipeline_handle>);
static_assert(std::same_as<decltype(create_swap_chain(swap_chain_desc{})), result<unique_swap_chain_handle>>);
static_assert(std::same_as<decltype(readback(texture_handle{}, texture_region{})), result<unique_readback_handle>>);
static_assert(std::same_as<decltype(text::create_font(text::font_desc{})), result<text::unique_font_handle>>);
static_assert(std::same_as<decltype(text::create_layout(text::layout_desc{}, std::string_view{})), result<text::unique_layout_handle>>);

TEST_F(initialized_graphics_test, abandoned_readback_does_not_prevent_other_readback)
{
    const auto texture{create_texture({texture_format::rgba8_unorm, 1, 1, 1, 1, texture_usage::copy_source | texture_usage::copy_destination})};
    ASSERT_TRUE(texture);
    const std::uint32_t pixel{0xFF123456};
    ASSERT_TRUE(update(texture->get(), {0, 0, 1, 1}, {std::as_bytes(std::span{&pixel, 1}), sizeof(pixel)}));
    auto abandoned{readback(texture->get(), {0, 0, 1, 1})};
    const auto kept{readback(texture->get(), {0, 0, 1, 1})};
    ASSERT_TRUE(abandoned && kept);
    abandoned->reset();
    const auto pixels{wait_readback(kept->get())};
    ASSERT_TRUE(pixels);
    ASSERT_GE(pixels->data.size(), sizeof(pixel));
    std::uint32_t actual{};
    std::memcpy(&actual, pixels->data.data(), sizeof(actual));
    EXPECT_EQ(actual, pixel);
}

TEST(graphics_descriptors, sampler_exhaustion)
{
    ASSERT_TRUE(initialize());
    const sampler_desc desc{sampler_filter::linear, sampler_filter::linear, sampler_filter::linear, sampler_address_mode::wrap, sampler_address_mode::wrap, sampler_address_mode::wrap, 1, 0.0f, std::numeric_limits<float>::max()};
    std::vector<unique_sampler_handle> samplers;
    for (;;)
    {
        auto sampler{create_sampler(desc)};
        if (!sampler)
        {
            break;
        }
        samplers.push_back(std::move(sampler));
    }
    ASSERT_FALSE(samplers.empty());
    samplers.clear();
    const auto fence{submit()};
    ASSERT_NE(fence, 0u);
    wait(fence);
    auto replacement{create_sampler(desc)};
    ASSERT_TRUE(replacement);
    replacement.reset();
    shutdown();
}

TEST_F(initialized_graphics_test, owning_texture_and_buffer_handles)
{
    unique_texture_handle texture;
    unique_buffer_handle buffer;
    auto created_texture{create_texture({texture_format::rgba8_unorm, 1, 1, 1, 1, texture_usage::copy_destination})};
    auto created_buffer{create_buffer({4, buffer_usage::copy_source, buffer_access::cpu_write})};
    ASSERT_TRUE(created_texture && created_buffer);
    texture = std::move(*created_texture);
    buffer = std::move(*created_buffer);
    const std::uint32_t pixel{0xFF123456};
    const auto bytes{std::as_bytes(std::span{&pixel, 1})};
    ASSERT_TRUE(update(texture.get(), {0, 0, 1, 1}, {bytes, sizeof(pixel)}));
    ASSERT_TRUE(update(buffer.get(), 0, bytes));
    const buffer_handle released{buffer.release()};
    EXPECT_FALSE(buffer);
    ASSERT_TRUE(update(released, 0, bytes));
    destroy(released);
    texture.reset();
    EXPECT_FALSE(texture);
}

TEST_F(initialized_graphics_test, explicit_submission_completes_batched_readbacks)
{
    const auto texture{create_texture({texture_format::rgba8_unorm, 1, 1, 1, 1, texture_usage::copy_source | texture_usage::copy_destination}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(texture);
    const std::uint32_t pixel{0xFF123456};
    ASSERT_TRUE(update(*texture, {0, 0, 1, 1}, {std::as_bytes(std::span{&pixel, 1}), sizeof(pixel)}));
    const auto first{readback(*texture, {0, 0, 1, 1}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto second{readback(*texture, {0, 0, 1, 1}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(first && second);
    destroy(*texture);
    EXPECT_EQ(poll(*first).status, readback_status::pending);
    EXPECT_EQ(poll(*second).status, readback_status::pending);

    const std::uint64_t fence{submit()};
    ASSERT_NE(fence, 0u);
    wait(fence);
    EXPECT_TRUE(poll(fence));
    for (const readback_handle request : {*first, *second})
    {
        const auto pixels{poll(request)};
        ASSERT_EQ(pixels.status, readback_status::ready);
        ASSERT_EQ(pixels.value.data.size(), sizeof(pixel));
        std::uint32_t actual{};
        std::memcpy(&actual, pixels.value.data.data(), sizeof(actual));
        EXPECT_EQ(actual, pixel);
        destroy(request);
    }
}

TEST_F(initialized_graphics_test, empty_submission_and_frame_queries_preserve_pending_work)
{
    const std::uint64_t first{submit()};
    const std::uint64_t second{submit()};
    ASSERT_NE(first, 0u);
    ASSERT_GT(second, first);
    wait(second);

    const auto texture{create_texture({texture_format::rgba8_unorm, 1, 1, 1, 1, texture_usage::render_target | texture_usage::copy_source}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(texture);
    ASSERT_TRUE(begin_frame());
    const auto pass{begin_render_pass({.render_targets = {{*texture, {begin_op::clear, {.color = {1, 0, 0, 1}}}, {end_op::store}}}})};
    ASSERT_TRUE(end_render_pass(pass));
    const auto request{readback(*texture, {0, 0, 1, 1}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(request);
    EXPECT_TRUE(poll(second));
    wait(second);
    EXPECT_EQ(poll(*request).status, readback_status::pending);
    ASSERT_TRUE(end_frame());

    const std::uint64_t completed_frame{submit()};
    ASSERT_NE(completed_frame, 0u);
    wait(completed_frame);
    const auto pixels{poll(*request)};
    ASSERT_EQ(pixels.status, readback_status::ready);
    std::uint32_t actual{};
    ASSERT_EQ(pixels.value.data.size(), sizeof(actual));
    std::memcpy(&actual, pixels.value.data.data(), sizeof(actual));
    EXPECT_EQ(actual, 0xFF0000FFu);
    destroy(*request);
    destroy(*texture);
}

TEST_F(initialized_graphics_test, texture_transfer)
{
    std::array<std::uint32_t, 16> pixels;
    pixels.fill(0xFF336699u);

    const texture_usage usage{texture_usage::shader_resource | texture_usage::render_target | texture_usage::copy_source | texture_usage::copy_destination};
    const auto source{create_texture({texture_format::rgba8_unorm, 4, 4, 3, 1, usage}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto copied{create_texture({texture_format::rgba8_unorm, 4, 4, 1, 1, usage}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto destination{create_texture({texture_format::rgba8_unorm, 2, 2, 1, 1, usage}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(source && copied && destination);
    ASSERT_TRUE(update(*source, {0, 0, 4, 4}, {std::as_bytes(std::span{pixels}), 16}));
    EXPECT_TRUE(generate_mips(*source));
    EXPECT_TRUE(copy(*source, {0, 0, 4, 4}, *copied, {0, 0, 4, 4}));
    EXPECT_TRUE(blit(*copied, {0, 0, 4, 4}, *destination, {0, 0, 2, 2}));

    const auto request{readback(*destination, {0, 0, 2, 2}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(request);
    ASSERT_TRUE(submit());
    std::optional<texture_readback> status;
    const auto deadline{std::chrono::steady_clock::now() + std::chrono::seconds{5}};
    do
    {
        auto current{poll(*request)};
        ASSERT_NE(current.status, readback_status::failed);
        if (current.status == readback_status::ready)
        {
            status = std::move(current.value);
        }
        if (!status.has_value())
        {
            std::this_thread::yield();
        }
    } while (!status.has_value() && std::chrono::steady_clock::now() < deadline);
    ASSERT_TRUE(status.has_value());
    ASSERT_TRUE(status);
    ASSERT_EQ(status->data.size(), 16);
    std::uint32_t first_pixel{};
    std::memcpy(&first_pixel, status->data.data(), sizeof(first_pixel));
    EXPECT_EQ(first_pixel, pixels.front());

    destroy(*request);
    destroy(*destination);
    destroy(*copied);
    destroy(*source);
}

TEST_F(initialized_graphics_test, text_layout_lifecycle)
{
    const std::vector<std::byte> bytes{load_test_font()};
    ASSERT_FALSE(bytes.empty());
    const auto font{text::create_font({bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(font);
    const auto layout{text::create_layout(
        {.font = *font,
            .font_size = 20.0f,
            .color = {1.0f, 1.0f, 1.0f, 1.0f},
            .max_width = 120.0f,
            .wrapping_mode = text::wrapping::wrap},
        "한글 ABC\n둘째 줄"
    )
            .transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(layout);

    const auto measured{text::metrics(*layout)};
    EXPECT_GT(measured.width, 0.0f);
    EXPECT_GT(measured.height, 20.0f);
    EXPECT_GE(measured.line_count, 2u);
    EXPECT_GT(measured.first_baseline, 0.0f);

    EXPECT_TRUE(text::update(*layout, "12345"));
    const auto updated{text::metrics(*layout)};
    EXPECT_EQ(updated.line_count, 1u);

    EXPECT_TRUE(text::update(*layout, ""));
    const auto empty_metrics{text::metrics(*layout)};
    EXPECT_FLOAT_EQ(empty_metrics.width, 0.0f);

    text::destroy(*layout);
    text::destroy(*font);
}

TEST_F(initialized_graphics_test, text_tabs_preserve_default_spacing)
{
    const auto bytes{load_test_font()};
    ASSERT_FALSE(bytes.empty());
    const auto font{text::create_font({bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(font);

    Microsoft::WRL::ComPtr<IDWriteFactory> factory;
    ASSERT_TRUE(SUCCEEDED(::DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory), &factory)));
    Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
    ASSERT_TRUE(SUCCEEDED(factory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 20.0f, L"", &format)));
    const float tab_stop{format->GetIncrementalTabStop()};
    const text::layout_desc desc{.font = *font, .font_size = 20.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}};
    const auto letter{text::create_layout(desc, "A").transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto tabbed{text::create_layout(desc, "\tA").transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(letter && tabbed);
    const float letter_width{text::metrics(*letter).width};
    EXPECT_FLOAT_EQ(text::metrics(*tabbed).width, tab_stop + letter_width);

    ASSERT_TRUE(text::update(*tabbed, "A\tA"));
    EXPECT_FLOAT_EQ(text::metrics(*tabbed).width, tab_stop + letter_width);
    ASSERT_TRUE(text::update(*tabbed, "\t\tA"));
    EXPECT_FLOAT_EQ(text::metrics(*tabbed).width, 2.0f * tab_stop + letter_width);

    text::destroy(*tabbed);
    text::destroy(*letter);
    text::destroy(*font);
}

TEST_F(initialized_graphics_test, unsupported_characters)
{
    const std::vector<std::byte> bytes{load_test_font()};
    ASSERT_FALSE(bytes.empty());
    auto font{text::create_font({bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(font);

    const std::string unsupported{"A\xF0\x9F\x99\x82"
                                  "B"};
    const std::string replacement{"A\xF4\x8F\xBF\xBF"
                                  "B"};
    auto replaced_layout{text::create_layout({.font = *font, .font_size = 20.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}, .wrapping_mode = text::wrapping::wrap}, unsupported).transform([](auto&& owner)
    {
        return owner.release();
    })};
    auto replacement_layout{text::create_layout({.font = *font, .font_size = 20.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}, .wrapping_mode = text::wrapping::wrap}, replacement).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(replaced_layout && replacement_layout);

    const auto replaced_metrics{text::metrics(*replaced_layout)};
    const auto replacement_metrics{text::metrics(*replacement_layout)};
    EXPECT_FLOAT_EQ(replaced_metrics.width, replacement_metrics.width);
    EXPECT_FLOAT_EQ(replaced_metrics.height, replacement_metrics.height);
    EXPECT_EQ(replaced_metrics.line_count, replacement_metrics.line_count);

    text::destroy(*replacement_layout);
    text::destroy(*replaced_layout);
    text::destroy(*font);
}

TEST_F(initialized_graphics_test, font_slot_growth)
{
    const std::vector<std::byte> bytes{load_test_font()};
    ASSERT_FALSE(bytes.empty());
    auto first_font{text::create_font({bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(first_font);
    auto layout{text::create_layout({.font = *first_font, .font_size = 20.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}, .wrapping_mode = text::wrapping::wrap}, "First font").transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(layout);

    std::vector<text::font_handle> additional_fonts;
    additional_fonts.reserve(64);
    for (std::size_t index{0}; index < additional_fonts.capacity(); ++index)
    {
        auto font{text::create_font({bytes, 0}).transform([](auto&& owner)
        {
            return owner.release();
        })};
        ASSERT_TRUE(font);
        additional_fonts.push_back(*font);
    }

    EXPECT_TRUE(text::update(*layout, "First font after slot growth"));
    const auto measured{text::metrics(*layout)};
    EXPECT_GT(measured.width, 0.0f);

    text::destroy(*layout);
    for (const text::font_handle font : additional_fonts)
    {
        text::destroy(font);
    }
    text::destroy(*first_font);
}

TEST_F(initialized_graphics_test, offscreen_text)
{
    const std::vector<std::byte> bytes{load_test_font()};
    ASSERT_FALSE(bytes.empty());
    auto font{text::create_font({bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(font);
    auto layout{text::create_layout(
        {.font = *font, .font_size = 28.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}, .wrapping_mode = text::wrapping::wrap},
        "가A"
    )
            .transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(layout);

    const texture_usage usage{
        texture_usage::render_target |
        texture_usage::copy_source
    };
    auto color{create_texture({texture_format::rgba8_unorm, 96, 64, 1, 1, usage}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(color);

    ASSERT_TRUE(begin_frame());
    const auto target_pass{begin_render_pass(
        {.render_targets = {{*color, {begin_op::clear}, {end_op::store}}}}
    )};
    ASSERT_TRUE(target_pass);
    auto commands{begin_encode(target_pass)};
    commands->set_scissor_rect({0, 0, 48, 64});
    commands->draw_text({.layout = *layout, .position = {8.0f, 8.0f}, .color = {1.0f, 0.0f, 0.0f, 1.0f}});
    end_encode(commands);
    ASSERT_TRUE(end_render_pass(target_pass));
    ASSERT_TRUE(end_frame());

    auto request{readback(*color, {0, 0, 96, 64, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(request);
    ASSERT_TRUE(submit());
    std::optional<texture_readback> status;
    const auto deadline{std::chrono::steady_clock::now() + std::chrono::seconds{5}};
    do
    {
        auto current{poll(*request)};
        ASSERT_NE(current.status, readback_status::failed);
        if (current.status == readback_status::ready)
        {
            status = std::move(current.value);
        }
        if (!status.has_value())
        {
            std::this_thread::yield();
        }
    } while (!status.has_value() && std::chrono::steady_clock::now() < deadline);
    ASSERT_TRUE(status.has_value());
    ASSERT_TRUE(status);

    for (std::size_t row{0}; row < 64; ++row)
    {
        for (std::size_t column{48}; column < 96; ++column)
        {
            std::uint32_t pixel{};
            std::memcpy(&pixel, status->data.data() + (row * 96 + column) * 4, 4);
            EXPECT_EQ(pixel, 0u);
        }
    }

    bool found_covered_pixel{};
    std::uint8_t maximum_red{};
    std::uint8_t maximum_alpha{};
    for (std::size_t index{0}; index + 3 < status->data.size(); index += 4)
    {
        const auto red{std::to_integer<std::uint8_t>(status->data[index])};
        const auto green{std::to_integer<std::uint8_t>(status->data[index + 1])};
        const auto blue{std::to_integer<std::uint8_t>(status->data[index + 2])};
        const auto alpha{std::to_integer<std::uint8_t>(status->data[index + 3])};
        maximum_red = std::max(maximum_red, red);
        maximum_alpha = std::max(maximum_alpha, alpha);
        if (red > 0 && alpha > 0 && green == 0 && blue == 0)
        {
            found_covered_pixel = true;
            break;
        }
    }
    EXPECT_TRUE(found_covered_pixel) << "maximum red=" << static_cast<int>(maximum_red)
                                     << ", maximum alpha=" << static_cast<int>(maximum_alpha);

    destroy(*request);
    destroy(*color);
    text::destroy(*layout);
    text::destroy(*font);
}

TEST_F(initialized_graphics_test, text_right_to_left_glyphs_match_visual_order)
{
    std::array<wchar_t, MAX_PATH> windows_directory;
    const auto length{::GetWindowsDirectoryW(windows_directory.data(), static_cast<UINT>(windows_directory.size()))};
    ASSERT_GT(length, 0u);
    ASSERT_LT(length, windows_directory.size());
    const auto bytes{load_test_font(std::filesystem::path{windows_directory.data()} / "Fonts" / "segoeui.ttf")};
    ASSERT_FALSE(bytes.empty());
    const auto font{text::create_font({bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(font);
    const text::layout_desc desc{.font = *font, .font_size = 32.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}};
    const auto word{text::create_layout(desc, "אב").transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto alef{text::create_layout(desc, "א").transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto bet{text::create_layout(desc, "ב").transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(word && alef && bet);
    const auto actual{create_texture({texture_format::rgba8_unorm, 96, 64, 1, 1, texture_usage::render_target | texture_usage::copy_source}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto expected{create_texture({texture_format::rgba8_unorm, 96, 64, 1, 1, texture_usage::render_target | texture_usage::copy_source}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(actual && expected);

    ASSERT_TRUE(begin_frame());
    const auto actual_pass{begin_render_pass({.render_targets = {{*actual, {begin_op::clear}, {end_op::store}}}})};
    const auto actual_commands{begin_encode(actual_pass)};
    actual_commands->draw_text({*word, {8.0f, 8.0f}, {1.0f, 1.0f, 1.0f, 1.0f}});
    end_encode(actual_commands);
    ASSERT_TRUE(end_render_pass(actual_pass));

    const auto expected_pass{begin_render_pass({.render_targets = {{*expected, {begin_op::clear}, {end_op::store}}}})};
    const auto expected_commands{begin_encode(expected_pass)};
    expected_commands->draw_text({*bet, {8.0f, 8.0f}, {1.0f, 1.0f, 1.0f, 1.0f}});
    expected_commands->draw_text({*alef, {8.0f + std::round(text::metrics(*bet).width), 8.0f}, {1.0f, 1.0f, 1.0f, 1.0f}});
    end_encode(expected_commands);
    ASSERT_TRUE(end_render_pass(expected_pass));
    ASSERT_TRUE(end_frame());

    const auto actual_readback{readback(*actual, {0, 0, 96, 64}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto expected_readback{readback(*expected, {0, 0, 96, 64}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(actual_readback && expected_readback);
    const auto actual_pixels{wait_readback(*actual_readback)};
    const auto expected_pixels{wait_readback(*expected_readback)};
    ASSERT_TRUE(actual_pixels && expected_pixels);
    EXPECT_EQ(actual_pixels->data, expected_pixels->data);
    std::size_t first_ink_x{96};
    std::size_t last_ink_x{};
    for (std::size_t pixel{0}; pixel < actual_pixels->data.size(); pixel += 4)
    {
        if (actual_pixels->data[pixel + 3] != std::byte{})
        {
            const std::size_t x{pixel / 4 % 96};
            first_ink_x = std::min(first_ink_x, x);
            last_ink_x = std::max(last_ink_x, x);
        }
    }
    EXPECT_LT(first_ink_x, 96u);

    Microsoft::WRL::ComPtr<IDWriteFactory> factory;
    ASSERT_TRUE(SUCCEEDED(::DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory), &factory)));
    Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
    ASSERT_TRUE(SUCCEEDED(factory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 32.0f, L"", &format)));
    Microsoft::WRL::ComPtr<IDWriteTextLayout> native_layout;
    ASSERT_TRUE(SUCCEEDED(factory->CreateTextLayout(L"אב", 2, format.Get(), 96.0f, 64.0f, &native_layout)));
    DWRITE_OVERHANG_METRICS overhang{};
    ASSERT_TRUE(SUCCEEDED(native_layout->GetOverhangMetrics(&overhang)));
    constexpr float rasterization_tolerance{2.0f};
    EXPECT_NEAR(static_cast<float>(first_ink_x), 8.0f - overhang.left, rasterization_tolerance);
    EXPECT_NEAR(static_cast<float>(last_ink_x + 1), 104.0f + overhang.right, rasterization_tolerance);

    destroy(*expected_readback);
    destroy(*actual_readback);
    destroy(*expected);
    destroy(*actual);
    text::destroy(*bet);
    text::destroy(*alef);
    text::destroy(*word);
    text::destroy(*font);
}

TEST_F(initialized_graphics_test, text_layout_contents_are_resolved_at_pass_end)
{
    const auto bytes{load_test_font()};
    ASSERT_FALSE(bytes.empty());
    const auto font{text::create_font({bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(font);
    const auto layout{text::create_layout({.font = *font, .font_size = 28.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}, .wrapping_mode = text::wrapping::wrap}, "").transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(layout);
    const auto filled{create_texture({texture_format::rgba8_unorm, 64, 64, 1, 1, texture_usage::render_target | texture_usage::copy_source}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto empty{create_texture({texture_format::rgba8_unorm, 64, 64, 1, 1, texture_usage::render_target | texture_usage::copy_source}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(filled && empty);

    ASSERT_TRUE(begin_frame());
    const auto first_pass{begin_render_pass({.render_targets = {{*filled, {begin_op::clear}, {end_op::store}}}})};
    const auto first_commands{begin_encode(first_pass)};
    first_commands->draw_text({*layout, {8.0f, 8.0f}, {1.0f, 0.0f, 0.0f, 1.0f}});
    end_encode(first_commands);
    ASSERT_TRUE(text::update(*layout, "A"));
    ASSERT_TRUE(end_render_pass(first_pass));

    const auto second_pass{begin_render_pass({.render_targets = {{*empty, {begin_op::clear}, {end_op::store}}}})};
    const auto second_commands{begin_encode(second_pass)};
    second_commands->draw_text({*layout, {8.0f, 8.0f}, {1.0f, 0.0f, 0.0f, 1.0f}});
    end_encode(second_commands);
    ASSERT_TRUE(text::update(*layout, ""));
    ASSERT_TRUE(end_render_pass(second_pass));
    text::destroy(*layout);
    text::destroy(*font);

    const auto filled_readback{readback(*filled, {0, 0, 64, 64}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto empty_readback{readback(*empty, {0, 0, 64, 64}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(filled_readback && empty_readback);
    ASSERT_TRUE(end_frame());
    const auto filled_pixels{wait_readback(*filled_readback)};
    const auto empty_pixels{wait_readback(*empty_readback)};
    ASSERT_TRUE(filled_pixels && empty_pixels);
    bool has_red_glyph{};
    for (std::size_t i{0}; i < filled_pixels->data.size(); i += 4)
    {
        has_red_glyph |= filled_pixels->data[i] != std::byte{};
        EXPECT_EQ(filled_pixels->data[i + 1], std::byte{});
        EXPECT_EQ(filled_pixels->data[i + 2], std::byte{});
    }
    EXPECT_TRUE(has_red_glyph);
    for (const auto pixel_byte : empty_pixels->data)
    {
        EXPECT_EQ(pixel_byte, std::byte{});
    }
    destroy(*filled_readback);
    destroy(*empty_readback);
    destroy(*filled);
    destroy(*empty);
}

TEST_F(initialized_graphics_test, clear_color_belongs_to_each_pass)
{
    const auto color{create_texture({texture_format::rgba8_unorm, 2, 2, 1, 1, texture_usage::render_target | texture_usage::copy_source}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(color);
    const std::array clear_colors{core::vec4{1, 0, 0, 1}, core::vec4{0, 1, 0, 1}};
    for (std::size_t index{0}; index < clear_colors.size(); ++index)
    {
        ASSERT_TRUE(begin_frame());
        render_pass_desc desc{{{*color, {begin_op::clear, {.color = clear_colors[index]}}, {end_op::store}}}, std::nullopt};
        const auto target_pass{begin_render_pass(desc)};
        ASSERT_TRUE(target_pass);
        desc.render_targets.clear();
        ASSERT_TRUE(end_render_pass(target_pass));
        ASSERT_TRUE(end_frame());
        const auto request{readback(*color, {0, 0, 2, 2, 0}).transform([](auto&& owner)
        {
            return owner.release();
        })};
        ASSERT_TRUE(request);
        const auto pixels{wait_readback(*request)};
        ASSERT_TRUE(pixels);
        ASSERT_EQ(pixels->data.size(), 16u);
        for (std::size_t offset{0}; offset < pixels->data.size(); offset += 4)
        {
            EXPECT_EQ(std::to_integer<unsigned>(pixels->data[offset]), index == 0 ? 255u : 0u);
            EXPECT_EQ(std::to_integer<unsigned>(pixels->data[offset + 1]), index == 1 ? 255u : 0u);
            EXPECT_EQ(std::to_integer<unsigned>(pixels->data[offset + 3]), 255u);
        }
        destroy(*request);
    }
    destroy(*color);
}

TEST_F(initialized_graphics_test, destroy_preserves_pending_blit_resources)
{
    const auto source{create_texture({texture_format::rgba8_unorm, 1, 1, 1, 1, texture_usage::shader_resource | texture_usage::copy_destination}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto destination{create_texture({texture_format::rgba8_unorm, 1, 1, 1, 1, texture_usage::render_target | texture_usage::copy_source}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(source && destination);
    const std::uint32_t pixel{0xFF123456};
    ASSERT_TRUE(update(*source, {0, 0, 1, 1, 0}, {std::as_bytes(std::span{&pixel, 1}), 4}));
    ASSERT_TRUE(blit(*source, {0, 0, 1, 1, 0}, *destination, {0, 0, 1, 1, 0}));
    destroy(*source);
    const auto request{readback(*destination, {0, 0, 1, 1, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(request);
    destroy(*destination);
    const auto pixels{wait_readback(*request)};
    ASSERT_TRUE(pixels);
    ASSERT_EQ(pixels->data.size(), 4u);
    std::uint32_t actual{};
    std::memcpy(&actual, pixels->data.data(), 4);
    EXPECT_EQ(actual, pixel);
    destroy(*request);
}

TEST_F(initialized_graphics_test, multiple_attachments)
{
    const texture_usage usage{texture_usage::render_target};
    auto first{create_texture({texture_format::rgba8_unorm, 16, 16, 1, 1, usage}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    auto second{create_texture({texture_format::rgba8_unorm, 16, 16, 1, 1, usage}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(first && second);

    ASSERT_TRUE(begin_frame());
    const auto target_pass{begin_render_pass(
        {.render_targets = {{*first, {begin_op::clear}, {end_op::store}}, {*second, {begin_op::clear}, {end_op::store}}}}
    )};
    ASSERT_TRUE(target_pass);
    ASSERT_TRUE(end_render_pass(target_pass));
    ASSERT_TRUE(end_frame());

    destroy(*second);
    destroy(*first);
}

TEST_F(initialized_graphics_test, color_attachments_keep_their_own_operations)
{
    const texture_usage usage{texture_usage::render_target | texture_usage::copy_source};
    const auto first{create_texture({texture_format::rgba8_unorm, 2, 2, 1, 1, usage}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto second{create_texture({texture_format::rgba8_unorm, 2, 2, 1, 1, usage}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(first && second);

    ASSERT_TRUE(begin_frame());
    const auto target_pass{begin_render_pass({.render_targets = {
                                                  {*second, {begin_op::clear, {.color = {0, 1, 0, 1}}}, {end_op::store}},
                                                  {*first, {begin_op::clear, {.color = {1, 0, 0, 1}}}, {end_op::store}},
                                              }})};
    ASSERT_TRUE(end_render_pass(target_pass));
    ASSERT_TRUE(end_frame());

    const std::array targets{*first, *second};
    const std::array<std::uint32_t, 2> expected{0xFF0000FF, 0xFF00FF00};
    for (std::size_t index{0}; index < targets.size(); ++index)
    {
        const auto request{readback(targets[index], {0, 0, 2, 2}).transform([](auto&& owner)
        {
            return owner.release();
        })};
        ASSERT_TRUE(request);
        const auto pixels{wait_readback(*request)};
        ASSERT_TRUE(pixels);
        for (std::size_t offset{0}; offset < pixels->data.size(); offset += 4)
        {
            std::uint32_t pixel{};
            std::memcpy(&pixel, pixels->data.data() + offset, sizeof(pixel));
            EXPECT_EQ(pixel, expected[index]);
        }
        destroy(*request);
    }
    destroy(*second);
    destroy(*first);
}

TEST_F(initialized_graphics_test, failed_swap_chain_creation_allows_retry)
{
    const std::unique_ptr<std::remove_pointer_t<HWND>, decltype(&::DestroyWindow)> window{
        ::CreateWindowExW(0, L"STATIC", L"Nori creation retry test", WS_POPUP, 0, 0, 64, 32, nullptr, nullptr, ::GetModuleHandleW(nullptr), nullptr),
        &::DestroyWindow
    };
    ASSERT_TRUE(window);
    const auto synchronization_texture{create_texture({texture_format::rgba8_unorm, 1, 1, 1, 1, texture_usage::copy_source}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(synchronization_texture);
    const auto previous_work{readback(*synchronization_texture, {0, 0, 1, 1}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(previous_work);
    ASSERT_TRUE(wait_readback(*previous_work));
    ASSERT_EQ(poll(*previous_work).status, readback_status::ready);
    destroy(*previous_work);
    std::vector<texture_handle> colors;
    for (;;)
    {
        const auto color{create_texture({texture_format::rgba8_unorm, 1, 1, 1, 1, texture_usage::render_target | texture_usage::copy_source}).transform([](auto&& owner)
        {
            return owner.release();
        })};
        if (!color)
        {
            ASSERT_EQ(color.error(), error_code::descriptor_allocation_failed);
            break;
        }
        colors.push_back(*color);
    }
    ASSERT_GT(colors.size(), 2u);
    destroy(colors.back());
    colors.pop_back();
    ASSERT_TRUE(begin_frame());
    const auto color_pass{begin_render_pass({.render_targets = {{colors.front(), {begin_op::clear, {.color = {1, 0, 0, 1}}}, {end_op::store}}}})};
    ASSERT_TRUE(end_render_pass(color_pass));
    const auto released_slot{readback(colors.front(), {0, 0, 1, 1}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(released_slot);
    ASSERT_TRUE(end_frame());
    ASSERT_TRUE(wait_readback(*released_slot));
    ASSERT_EQ(poll(*released_slot).status, readback_status::ready);
    destroy(*released_slot);

    for (int attempt{0}; attempt < 3; ++attempt)
    {
        const auto failed{create_swap_chain({window.get(), false}).transform([](auto&& owner)
        {
            return owner.release();
        })};
        ASSERT_FALSE(failed);
        EXPECT_EQ(failed.error(), error_code::descriptor_allocation_failed);
    }
    destroy(colors.back());
    colors.pop_back();
    const auto released_second_slot{readback(colors.front(), {0, 0, 1, 1}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(released_second_slot);
    ASSERT_TRUE(wait_readback(*released_second_slot));
    ASSERT_EQ(poll(*released_second_slot).status, readback_status::ready);
    destroy(*released_second_slot);

    const auto chain{create_swap_chain({window.get(), false}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(chain);
    ASSERT_TRUE(begin_frame());
    const auto screen{back_buffer(*chain)};
    const auto screen_pass{begin_render_pass({.render_targets = {{screen, {begin_op::clear, {.color = {1, 0, 0, 1}}}, {end_op::store}}}})};
    ASSERT_TRUE(end_render_pass(screen_pass));
    const auto request{readback(screen, {0, 0, 1, 1}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(request);
    ASSERT_TRUE(end_frame());
    ASSERT_TRUE(present(*chain));
    const auto pixels{wait_readback(*request)};
    ASSERT_TRUE(pixels);
    ASSERT_EQ(pixels->data.size(), 4u);
    std::uint32_t pixel{};
    std::memcpy(&pixel, pixels->data.data(), sizeof(pixel));
    EXPECT_EQ(pixel, 0xFF0000FFu);
    destroy(*request);
    destroy(*chain);
    for (const auto color : colors)
    {
        destroy(color);
    }
    const auto cleanup_work{readback(*synchronization_texture, {0, 0, 1, 1}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(cleanup_work);
    ASSERT_TRUE(wait_readback(*cleanup_work));
    ASSERT_EQ(poll(*cleanup_work).status, readback_status::ready);
    destroy(*cleanup_work);
    destroy(*synchronization_texture);
}

TEST_F(initialized_graphics_test, back_buffer_can_be_rendered_and_read_as_a_texture)
{
    const std::unique_ptr<std::remove_pointer_t<HWND>, decltype(&::DestroyWindow)> window{
        ::CreateWindowExW(0, L"STATIC", L"Nori back buffer test", WS_POPUP, 0, 0, 64, 32, nullptr, nullptr, ::GetModuleHandleW(nullptr), nullptr),
        &::DestroyWindow
    };
    ASSERT_TRUE(window);
    const auto chain{create_swap_chain({window.get(), false}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(chain);
    ASSERT_TRUE(begin_frame());
    const texture_handle screen{back_buffer(*chain)};
    const auto screen_pass{begin_render_pass({.render_targets = {{screen, {begin_op::clear, {.color = {1, 0, 0, 1}}}, {end_op::store}}}})};
    ASSERT_TRUE(end_render_pass(screen_pass));
    const auto request{readback(screen, {0, 0, 64, 32}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(request);
    ASSERT_TRUE(end_frame());
    ASSERT_TRUE(present(*chain));
    const auto pixels{wait_readback(*request)};
    ASSERT_TRUE(pixels);
    ASSERT_EQ(pixels->data.size(), 64u * 32u * 4u);
    for (std::size_t offset{0}; offset < pixels->data.size(); offset += 4)
    {
        std::uint32_t pixel{};
        std::memcpy(&pixel, pixels->data.data() + offset, sizeof(pixel));
        EXPECT_EQ(pixel, 0xFF0000FFu);
    }
    destroy(*request);
    destroy(*chain);
}

TEST_F(initialized_graphics_test, back_buffer_copy_and_readback_survive_resize)
{
    const std::unique_ptr<std::remove_pointer_t<HWND>, decltype(&::DestroyWindow)> window{
        ::CreateWindowExW(0, L"STATIC", L"Nori back buffer resize test", WS_POPUP, 0, 0, 64, 32, nullptr, nullptr, ::GetModuleHandleW(nullptr), nullptr),
        &::DestroyWindow
    };
    ASSERT_TRUE(window);
    const auto chain{create_swap_chain({window.get(), false}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto source{create_texture({texture_format::rgba8_unorm, 64, 32, 1, 1, texture_usage::render_target | texture_usage::copy_source}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto snapshot{create_texture({texture_format::rgba8_unorm, 64, 32, 1, 1, texture_usage::copy_destination | texture_usage::copy_source}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(chain && source && snapshot);
    ASSERT_TRUE(begin_frame());
    const auto source_pass{begin_render_pass({.render_targets = {{*source, {begin_op::clear, {.color = {0, 1, 0, 1}}}, {end_op::store}}}})};
    ASSERT_TRUE(end_render_pass(source_pass));
    const auto screen{back_buffer(*chain)};
    ASSERT_TRUE(copy(*source, {0, 0, 64, 32}, screen, {0, 0, 64, 32}));
    ASSERT_TRUE(copy(screen, {0, 0, 64, 32}, *snapshot, {0, 0, 64, 32}));
    const auto screen_readback{readback(screen, {0, 0, 64, 32}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto snapshot_readback{readback(*snapshot, {0, 0, 64, 32}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(screen_readback && snapshot_readback);
    ASSERT_TRUE(end_frame());
    ASSERT_TRUE(present(*chain));
    ASSERT_TRUE(resize(*chain, 128, 96));
    ASSERT_TRUE(begin_frame());
    const auto resized_screen{back_buffer(*chain)};
    const auto resized_pass{begin_render_pass({.render_targets = {{resized_screen, {begin_op::clear, {.color = {0, 0, 1, 1}}}, {end_op::store}}}})};
    ASSERT_TRUE(end_render_pass(resized_pass));
    const auto resized_readback{readback(resized_screen, {0, 0, 128, 96}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(resized_readback);
    ASSERT_TRUE(end_frame());
    ASSERT_TRUE(present(*chain));
    destroy(*chain);

    const std::array readbacks{*screen_readback, *snapshot_readback, *resized_readback};
    const std::array<std::uint32_t, 3> expected{0xFF00FF00, 0xFF00FF00, 0xFFFF0000};
    for (std::size_t index{0}; index < readbacks.size(); ++index)
    {
        const auto pixels{wait_readback(readbacks[index])};
        ASSERT_TRUE(pixels);
        EXPECT_EQ(pixels->width, index == 2 ? 128u : 64u);
        EXPECT_EQ(pixels->height, index == 2 ? 96u : 32u);
        ASSERT_EQ(pixels->data.size(), index == 2 ? 128u * 96u * 4u : 64u * 32u * 4u);
        for (std::size_t offset{0}; offset < pixels->data.size(); offset += 4)
        {
            std::uint32_t pixel{};
            std::memcpy(&pixel, pixels->data.data() + offset, sizeof(pixel));
            EXPECT_EQ(pixel, expected[index]);
        }
        destroy(readbacks[index]);
    }
    destroy(*snapshot);
    destroy(*source);
}

TEST_F(initialized_graphics_test, swap_chain_attachments_can_be_selected_independently)
{
    const std::unique_ptr<std::remove_pointer_t<HWND>, decltype(&::DestroyWindow)> window{
        ::CreateWindowExW(0, L"STATIC", L"Nori attachment test", WS_POPUP, 0, 0, 64, 32, nullptr, nullptr, ::GetModuleHandleW(nullptr), nullptr),
        &::DestroyWindow
    };
    ASSERT_TRUE(window);
    const auto chain{create_swap_chain({window.get(), false}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(chain);
    const auto color{create_texture({texture_format::rgba16_float, 64, 32, 1, 1, texture_usage::render_target | texture_usage::copy_source}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto depth{create_texture({texture_format::d32_float, 64, 32, 1, 1, texture_usage::depth_stencil | texture_usage::shader_resource}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto depth_snapshot{create_texture({texture_format::r32_float, 64, 32, 1, 1, texture_usage::render_target | texture_usage::copy_source}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(color && depth && depth_snapshot);
    const auto font{text::create_font({load_test_font(), 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(font);
    const auto layout{text::create_layout({.font = *font, .font_size = 16.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}}, "A").transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(layout);

    ASSERT_TRUE(begin_frame());
    const auto screen_pass{begin_render_pass({.render_targets = {{back_buffer(*chain), {begin_op::clear}, {end_op::store}}}, .depth_stencil = {{*depth, {begin_op::clear, {.depth = 0.25f}}, {begin_op::clear}, {end_op::store}, {end_op::store}}}})};
    const auto screen_commands{begin_encode(screen_pass)};
    screen_commands->draw_text({*layout, {0, 0}, {1, 1, 1, 1}});
    end_encode(screen_commands);
    ASSERT_TRUE(end_render_pass(screen_pass));
    ASSERT_TRUE(blit(*depth, {0, 0, 64, 32}, *depth_snapshot, {0, 0, 64, 32}));
    const auto depth_readback{readback(*depth_snapshot, {0, 0, 64, 32}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(depth_readback);

    const auto texture_pass{begin_render_pass({.render_targets = {{*color, {begin_op::clear, {.color = {0, 0, 1, 1}}}, {end_op::store}}}, .depth_stencil = {{*depth, {begin_op::clear, {.depth = 1.0f}}, {begin_op::clear}, {end_op::store}, {end_op::store}}}})};
    const auto texture_commands{begin_encode(texture_pass)};
    texture_commands->draw_text({*layout, {0, 0}, {1, 1, 1, 1}});
    end_encode(texture_commands);
    ASSERT_TRUE(end_render_pass(texture_pass));
    const auto color_readback{readback(*color, {63, 31, 1, 1}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(color_readback);
    ASSERT_TRUE(end_frame());
    ASSERT_TRUE(present(*chain));

    const auto depth_pixels{wait_readback(*depth_readback)};
    ASSERT_TRUE(depth_pixels);
    ASSERT_EQ(depth_pixels->data.size(), 64u * 32u * sizeof(float));
    for (std::size_t offset{0}; offset < depth_pixels->data.size(); offset += sizeof(float))
    {
        float value{};
        std::memcpy(&value, depth_pixels->data.data() + offset, sizeof(value));
        EXPECT_FLOAT_EQ(value, 0.25f);
    }
    const auto color_pixels{wait_readback(*color_readback)};
    ASSERT_TRUE(color_pixels);
    std::array<std::uint16_t, 4> pixel{};
    ASSERT_EQ(color_pixels->data.size(), sizeof(pixel));
    std::memcpy(pixel.data(), color_pixels->data.data(), sizeof(pixel));
    const std::array<std::uint16_t, 4> expected{0, 0, 0x3C00, 0x3C00};
    EXPECT_EQ(pixel, expected);

    ASSERT_TRUE(begin_frame());
    const auto depthless_pass{begin_render_pass({.render_targets = {{back_buffer(*chain), {begin_op::clear}, {end_op::store}}}})};
    const auto depthless_commands{begin_encode(depthless_pass)};
    depthless_commands->draw_text({*layout, {0, 0}, {1, 1, 1, 1}});
    end_encode(depthless_commands);
    ASSERT_TRUE(end_render_pass(depthless_pass));
    ASSERT_TRUE(end_frame());
    ASSERT_TRUE(present(*chain));
    ASSERT_TRUE(submit_and_wait());

    destroy(*color_readback);
    destroy(*depth_readback);
    text::destroy(*layout);
    text::destroy(*font);
    destroy(*depth_snapshot);
    destroy(*depth);
    destroy(*color);
    destroy(*chain);
}

TEST_F(initialized_graphics_test, depth_only_pass_clears_texture)
{
    const auto depth{create_texture({texture_format::d32_float, 4, 2, 1, 1, texture_usage::depth_stencil | texture_usage::shader_resource}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto snapshot{create_texture({texture_format::r32_float, 4, 2, 1, 1, texture_usage::render_target | texture_usage::copy_source}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(depth && snapshot);
    ASSERT_TRUE(begin_frame());
    const auto target_pass{begin_render_pass({.depth_stencil = {{*depth, {begin_op::clear, {.depth = 0.5f}}, {begin_op::clear}, {end_op::store}, {end_op::store}}}})};
    ASSERT_TRUE(end_render_pass(target_pass));
    ASSERT_TRUE(end_frame());
    ASSERT_TRUE(blit(*depth, {0, 0, 4, 2}, *snapshot, {0, 0, 4, 2}));
    const auto request{readback(*snapshot, {0, 0, 4, 2}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(request);
    const auto pixels{wait_readback(*request)};
    ASSERT_TRUE(pixels);
    ASSERT_EQ(pixels->data.size(), 8u * sizeof(float));
    for (std::size_t offset{0}; offset < pixels->data.size(); offset += sizeof(float))
    {
        float value{};
        std::memcpy(&value, pixels->data.data() + offset, sizeof(value));
        EXPECT_FLOAT_EQ(value, 0.5f);
    }
    destroy(*request);
    destroy(*snapshot);
    destroy(*depth);
}

TEST_F(initialized_graphics_test, swap_chain_resize)
{
    const std::unique_ptr<std::remove_pointer_t<HWND>, decltype(&::DestroyWindow)> window{
        ::CreateWindowExW(0, L"STATIC", L"Nori resize test", WS_OVERLAPPEDWINDOW, 0, 0, 256, 256, nullptr, nullptr, ::GetModuleHandleW(nullptr), nullptr),
        &::DestroyWindow
    };
    ASSERT_TRUE(window);
    const auto chain{create_swap_chain({window.get(), false}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(chain);
    const auto font{text::create_font({load_test_font(), 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(font);
    const auto layout{text::create_layout({.font = *font, .font_size = 16.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}}, "Resize").transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(layout);
    const std::array<std::array<std::uint32_t, 2>, 3> sizes{{{64, 32}, {64, 32}, {128, 96}}};
    for (const auto& size : sizes)
    {
        ASSERT_TRUE(resize(*chain, size[0], size[1]));
        ASSERT_TRUE(begin_frame());
        const auto target_pass{begin_render_pass({
            .render_targets = {{back_buffer(*chain), {begin_op::clear}, {end_op::store}}},
        })};
        ASSERT_TRUE(target_pass);
        const auto commands{begin_encode(target_pass)};
        commands->draw_text({.layout = *layout, .color = {1.0f, 1.0f, 1.0f, 1.0f}});
        end_encode(commands);
        ASSERT_TRUE(end_render_pass(target_pass));
        ASSERT_TRUE(end_frame());
        ASSERT_TRUE(present(*chain));
    }
    ASSERT_TRUE(submit_and_wait());
    text::destroy(*layout);
    text::destroy(*font);
    destroy(*chain);
}

TEST_F(initialized_graphics_test, zero_size_resize_uses_window_client_size)
{
    const std::unique_ptr<std::remove_pointer_t<HWND>, decltype(&::DestroyWindow)> window{
        ::CreateWindowExW(0, L"STATIC", L"Nori window size test", WS_POPUP, 0, 0, 64, 32, nullptr, nullptr, ::GetModuleHandleW(nullptr), nullptr),
        &::DestroyWindow
    };
    ASSERT_TRUE(window);
    const auto chain{create_swap_chain({window.get(), false}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(chain);
    ASSERT_TRUE(resize(*chain, 128, 96));
    ASSERT_TRUE(resize(*chain, 0, 0));
    ASSERT_TRUE(begin_frame());
    const auto screen{back_buffer(*chain)};
    const auto screen_pass{begin_render_pass({.render_targets = {{screen, {begin_op::clear, {.color = {0, 0, 1, 1}}}, {end_op::store}}}})};
    ASSERT_TRUE(end_render_pass(screen_pass));
    const auto request{readback(screen, {0, 0, 64, 32}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(request);
    ASSERT_TRUE(end_frame());
    ASSERT_TRUE(present(*chain));
    const auto pixels{wait_readback(*request)};
    ASSERT_TRUE(pixels);
    ASSERT_EQ(pixels->data.size(), 64u * 32u * 4u);
    for (std::size_t offset{0}; offset < pixels->data.size(); offset += 4)
    {
        std::uint32_t pixel{};
        std::memcpy(&pixel, pixels->data.data() + offset, sizeof(pixel));
        EXPECT_EQ(pixel, 0xFFFF0000u);
    }
    destroy(*request);
    destroy(*chain);
}

TEST_F(initialized_graphics_resource_test, texture_source_creates_independent_textures)
{
    const std::array<std::uint32_t, 4> pixels{0xFF112233u, 0xFF445566u, 0xFF778899u, 0xFFAABBCCu};
    const nori::resource::handle root{nori::resource::create_root()};
    const nori::resource::handle source{nori::resource::create(root, "texture", nori::resource::empty)};
    ASSERT_TRUE(root && source);
    nori::resource::set(
        source,
        nori::resource::image{
            .width = 2,
            .height = 2,
            .mip_levels = 1,
            .data = std::vector<std::byte>(std::as_bytes(std::span{pixels}).begin(), std::as_bytes(std::span{pixels}).end()),
        }
    );

    auto described{create_texture({texture_format::rgba8_unorm, 2, 2, 1, 1, texture_usage::shader_resource}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    auto first{create_texture(source).transform([](auto&& owner)
    {
        return owner.release();
    })};
    auto cached{create_texture(source).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(described && first && cached);
    EXPECT_NE(*described, *first);
    EXPECT_NE(*first, *cached);

    destroy(*first);
    ASSERT_TRUE(update(*cached, {0, 0, 2, 2}, {std::as_bytes(std::span{pixels}), 2 * sizeof(std::uint32_t)}));
    destroy(*cached);
    const std::array<std::uint32_t, 1> replacement_pixels{0xFF00FF00u};
    nori::resource::set(
        source,
        nori::resource::image{
            .width = 1,
            .height = 1,
            .mip_levels = 1,
            .data = std::vector<std::byte>(std::as_bytes(std::span{replacement_pixels}).begin(), std::as_bytes(std::span{replacement_pixels}).end()),
        }
    );

    const auto replacement{create_texture(source).transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto replacement_cached{create_texture(source).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(replacement && replacement_cached);
    EXPECT_NE(*replacement, *replacement_cached);

    destroy(*replacement);
    destroy(*replacement_cached);
    destroy(*described);
    nori::resource::remove(root);
}

TEST_F(initialized_graphics_resource_test, removed_texture_source)
{
    const nori::resource::handle root{nori::resource::create_root()};
    const nori::resource::handle source{nori::resource::create(root, "texture", nori::resource::empty)};
    ASSERT_TRUE(root && source);
    nori::resource::set(
        source,
        nori::resource::image{
            .width = 1,
            .height = 1,
            .mip_levels = 1,
            .data = std::vector<std::byte>(4),
        }
    );

    const auto texture{create_texture(source).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(texture);
    nori::resource::remove(root);

    destroy(*texture);
}

TEST_F(initialized_graphics_resource_test, bc_texture_mips)
{
    const nori::resource::handle root{nori::resource::create_root()};
    const nori::resource::handle source{nori::resource::create(root, "texture", nori::resource::empty)};
    ASSERT_TRUE(root && source);
    nori::resource::set(
        source,
        nori::resource::image{
            .format = nori::resource::image_format::bc7_unorm_srgb,
            .width = 8,
            .height = 8,
            .mip_levels = 4,
            .data = std::vector<std::byte>(112, std::byte{0x33})
        }
    );

    const auto texture{create_texture(source).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(texture);

    destroy(*texture);
    nori::resource::remove(root);
}

TEST_F(initialized_graphics_resource_test, font_cache)
{
    const std::vector<std::byte> bytes{load_test_font()};
    ASSERT_FALSE(bytes.empty());
    const nori::resource::handle root{nori::resource::create_root()};
    const nori::resource::handle source{nori::resource::create(root, "font", nori::resource::empty)};
    ASSERT_TRUE(root && source);
    nori::resource::set(source, nori::resource::binary{bytes});

    auto first{text::create_font(source).transform([](auto&& owner)
    {
        return owner.release();
    })};
    auto cached{text::create_font(source).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(first && cached);
    ASSERT_NE(*first, *cached);
    text::destroy(*first);

    auto layout{text::create_layout({.font = *cached, .font_size = 16.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}, .wrapping_mode = text::wrapping::wrap}, "리소스").transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(layout);
    nori::resource::remove(root);

    text::destroy(*layout);
    text::destroy(*cached);
}
} // namespace nori::graphics
