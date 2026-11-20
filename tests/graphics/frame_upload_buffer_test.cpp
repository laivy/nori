#include <gtest/gtest.h>

#include "frame_upload_test_helpers.hpp"

namespace
{
TEST_F(frame_upload_buffer_test, rectangles_preserve_indexed_draw_state)
{
    ASSERT_TRUE(begin_frame());
    constexpr auto positions{quad(0, 2)};
    constexpr std::array<std::uint16_t, 6> index_data{0, 1, 2, 2, 1, 3};
    const auto vertices{create_frame_upload_buffer(buffer_usage::vertex, std::as_bytes(std::span{positions}))};
    const auto indices{create_frame_upload_buffer(buffer_usage::index, std::as_bytes(std::span{index_data}))};
    ASSERT_TRUE(vertices && indices);
    const auto handle{begin_render_pass(pass())};
    const auto encoder{begin_encode(handle)};
    draw(*encoder, *vertices, *indices, false);
    encoder->draw_rect({{1.0f, 0.0f}, {3.0f, 4.0f}, {0.0f, 0.0f, 1.0f, 1.0f}});
    encoder->draw_indexed(6);
    end_encode(encoder);
    ASSERT_TRUE(end_render_pass(handle));
    ASSERT_TRUE(end_frame());
    const auto rb{readback(target_, {0, 0, 4, 4}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(rb);
    expect_readback(*rb, 0xFF0000FFu, 0xFFFF0000u);
}

TEST_F(frame_upload_buffer_test, copied_constant_buffers_preserve_each_draw_color_and_transform)
{
    ASSERT_TRUE(begin_frame());
    constexpr auto positions{quad(0, 2)};
    constexpr std::array<std::uint16_t, 6> index_data{0, 1, 2, 2, 1, 3};
    const auto vertices{create_frame_upload_buffer(buffer_usage::vertex, std::as_bytes(std::span{positions}))};
    const auto indices{create_frame_upload_buffer(buffer_usage::index, std::as_bytes(std::span{index_data}))};
    auto constants{std::array<float, 8>{4, 4, 0, 0, 1, 0, 0, 1}};
    const auto red{create_frame_upload_buffer(buffer_usage::constant, std::as_bytes(std::span{constants}))};
    ASSERT_TRUE(vertices && indices && red);
    EXPECT_EQ(red->size, 32u);
    constants.fill(1000);
    const auto handle{begin_render_pass(pass())};
    const auto encoder{begin_encode(handle)};
    draw(*encoder, *vertices, *indices, *red);
    const auto green = []()
    {
        constexpr std::array<float, 8> values{4, 4, 2, 0, 0, 1, 0, 1};
        std::vector<std::byte> temporary(64 * 1024);
        std::memcpy(temporary.data(), values.data(), sizeof(values));
        return create_frame_upload_buffer(buffer_usage::constant, temporary);
    }();
    ASSERT_TRUE(green);
    EXPECT_EQ(green->size, 65536u);
    encoder->set_constant_buffer(0, *green);
    encoder->draw_indexed(6);
    end_encode(encoder);
    ASSERT_TRUE(end_render_pass(handle));
    ASSERT_TRUE(end_frame());
    expect_columns();
}

TEST_F(frame_upload_buffer_test, constant_buffer_views_cross_worker_recording_and_passes_after_page_growth)
{
    ASSERT_TRUE(begin_frame());
    constexpr auto mixed_usage{buffer_usage::vertex | buffer_usage::index | buffer_usage::constant};
    constexpr auto positions{quad(0, 2)};
    constexpr std::array<std::uint16_t, 6> index_data{0, 1, 2, 2, 1, 3};
    const auto vertices{create_frame_upload_buffer(mixed_usage, std::as_bytes(std::span{positions}))};
    const auto indices{create_frame_upload_buffer(buffer_usage::index | buffer_usage::constant, std::as_bytes(std::span{index_data}))};
    constexpr std::array<float, 8> red_constants{4, 4, 0, 0, 1, 0, 0, 1};
    std::vector<std::byte> source(33, std::byte{0xFF});
    std::memcpy(source.data(), red_constants.data(), sizeof(red_constants));
    const auto red{create_frame_upload_buffer(buffer_usage::constant, source)};
    ASSERT_TRUE(vertices && indices && red);
    EXPECT_EQ(red->size, 33u);
    source.assign(33, std::byte{0});
    const auto first{begin_render_pass(pass())};
    std::jthread worker{[this, first, vertices = *vertices, indices = *indices, red = *red]()
    {
        const auto encoder{begin_encode(first)};
        draw(*encoder, vertices, indices, red);
        end_encode(encoder);
    }};
    worker.join();
    ASSERT_TRUE(end_render_pass(first));
    const std::vector<std::byte> large(64 * 1024, std::byte{0xFF});
    for (int i{0}; i < 18; ++i)
    {
        ASSERT_TRUE(create_frame_upload_buffer(buffer_usage::constant, large));
    }
    constexpr std::array<std::byte, 3> odd_size{};
    ASSERT_TRUE(create_frame_upload_buffer(buffer_usage::vertex, odd_size));
    const auto green = []()
    {
        constexpr std::array<float, 8> constants{4, 4, 2, 0, 0, 1, 0, 1};
        std::vector<std::byte> temporary(257, std::byte{0xFF});
        std::memcpy(temporary.data(), constants.data(), sizeof(constants));
        return create_frame_upload_buffer(buffer_usage::constant, temporary);
    }();
    ASSERT_TRUE(green);
    EXPECT_EQ(green->size, 257u);
    const auto second{begin_render_pass(pass(begin_op::load))};
    const auto encoder{begin_encode(second)};
    draw(*encoder, *vertices, *indices, *green);
    end_encode(encoder);
    ASSERT_TRUE(end_render_pass(second));
    ASSERT_TRUE(end_frame());
    expect_columns();
}

TEST_F(frame_upload_buffer_test, frame_constant_buffers_and_regular_constant_buffer_offsets_share_the_binding_slot)
{
    constexpr std::array<float, 8> green_constants{4, 4, 2, 0, 0, 1, 0, 1};
    std::array<std::byte, 512> regular_data{};
    std::memcpy(regular_data.data() + 256, green_constants.data(), sizeof(green_constants));
    const auto regular{create_buffer({512, buffer_usage::constant, buffer_access::cpu_write}, regular_data).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(regular);
    ASSERT_TRUE(begin_frame());
    constexpr auto positions{quad(0, 2)};
    constexpr std::array<std::uint16_t, 6> index_data{0, 1, 2, 2, 1, 3};
    const auto vertices{create_frame_upload_buffer(buffer_usage::vertex, std::as_bytes(std::span{positions}))};
    const auto indices{create_frame_upload_buffer(buffer_usage::index, std::as_bytes(std::span{index_data}))};
    constexpr std::array<float, 8> red_constants{4, 4, 0, 0, 1, 0, 0, 1};
    const auto red{create_frame_upload_buffer(buffer_usage::constant, std::as_bytes(std::span{red_constants}))};
    ASSERT_TRUE(vertices && indices && red);
    const auto handle{begin_render_pass(pass())};
    const auto encoder{begin_encode(handle)};
    draw(*encoder, *vertices, *indices, *red);
    encoder->set_constant_buffer(0, *regular, 256);
    encoder->draw_indexed(6);
    end_encode(encoder);
    ASSERT_TRUE(end_render_pass(handle));
    destroy(*regular);
    ASSERT_TRUE(end_frame());
    expect_columns();
}

TEST_F(frame_upload_buffer_test, copied_vertex_and_index_data_survive_source_mutation_and_later_draw)
{
    ASSERT_TRUE(begin_frame());
    auto vertices{quad(0, 2)};
    std::array<std::uint16_t, 6> indices{0, 1, 2, 2, 1, 3};
    const auto first_vertices{create_frame_upload_buffer(buffer_usage::vertex, std::as_bytes(std::span{vertices}))};
    const auto first_indices{create_frame_upload_buffer(buffer_usage::index, std::as_bytes(std::span{indices}))};
    ASSERT_TRUE(first_vertices && first_indices);
    EXPECT_EQ(first_vertices->size, 64u);
    EXPECT_EQ(first_indices->size, 12u);
    const auto handle{begin_render_pass(pass())};
    const auto encoder{begin_encode(handle)};
    draw(*encoder, *first_vertices, *first_indices, false);
    vertices = quad(2, 4);
    std::array<std::uint32_t, 6> later_indices{3, 1, 2, 2, 1, 0};
    const auto second_vertices{create_frame_upload_buffer(buffer_usage::vertex, std::as_bytes(std::span{vertices}))};
    const auto second_indices{create_frame_upload_buffer(buffer_usage::index, std::as_bytes(std::span{later_indices}))};
    ASSERT_TRUE(second_vertices && second_indices);
    later_indices.fill(1000);
    vertices.fill(1000);
    indices.fill(1000);
    draw(*encoder, *second_vertices, *second_indices, true, index_format::uint32);
    end_encode(encoder);
    ASSERT_TRUE(end_render_pass(handle));
    ASSERT_TRUE(end_frame());
    expect_columns();
}

TEST_F(frame_upload_buffer_test, views_can_cross_worker_recording_and_passes_after_large_allocation)
{
    ASSERT_TRUE(begin_frame());
    constexpr std::array<std::uint16_t, 6> index_data{0, 1, 2, 2, 1, 3};
    const auto vertices = []()
    {
        constexpr auto positions{quad(0, 2)};
        const std::vector<float> temporary(positions.begin(), positions.end());
        return create_frame_upload_buffer(buffer_usage::vertex, std::as_bytes(std::span{temporary}));
    }();
    const auto indices{create_frame_upload_buffer(buffer_usage::index, std::as_bytes(std::span{index_data}))};
    ASSERT_TRUE(vertices && indices);
    const auto first{begin_render_pass(pass())};
    std::jthread worker{[this, first, vertices = *vertices, indices = *indices]()
    {
        const auto encoder{begin_encode(first)};
        draw(*encoder, vertices, indices, false);
        end_encode(encoder);
    }};
    worker.join();
    ASSERT_TRUE(end_render_pass(first));
    constexpr std::array<std::byte, 3> odd_size{};
    ASSERT_TRUE(create_frame_upload_buffer(buffer_usage::vertex, odd_size));
    {
        std::vector<std::byte> large(2 * 1024 * 1024, std::byte{0xFF});
        ASSERT_TRUE(create_frame_upload_buffer(buffer_usage::vertex | buffer_usage::index, large));
    }
    auto right{quad(2, 4)};
    const auto later_vertices{create_frame_upload_buffer(buffer_usage::vertex, std::as_bytes(std::span{right}))};
    const auto later_indices{create_frame_upload_buffer(buffer_usage::index, std::as_bytes(std::span{index_data}))};
    ASSERT_TRUE(later_vertices && later_indices);
    right.fill(1000);
    const auto second{begin_render_pass(pass(begin_op::load))};
    const auto encoder{begin_encode(second)};
    draw(*encoder, *later_vertices, *later_indices, true);
    end_encode(encoder);
    ASSERT_TRUE(end_render_pass(second));
    ASSERT_TRUE(end_frame());
    expect_columns();
}

TEST_F(frame_upload_buffer_test, odd_sized_constants_preserve_adjacent_vertex_and_index_data)
{
    ASSERT_TRUE(begin_frame());
    constexpr std::array<float, 8> red{4, 4, 0, 0, 1, 0, 0, 1};
    constexpr std::array<float, 8> green{4, 4, 0, 0, 0, 1, 0, 1};
    std::array<std::byte, 33> small_data{};
    std::array<std::byte, 257> larger_data{};
    std::memcpy(small_data.data(), red.data(), sizeof(red));
    std::memcpy(larger_data.data(), green.data(), sizeof(green));
    const auto first_constant_buffer{create_frame_upload_buffer(buffer_usage::constant, small_data)};
    constexpr auto positions{quad(0, 2)};
    const auto vertices{create_frame_upload_buffer(buffer_usage::vertex, std::as_bytes(std::span{positions}))};
    const auto larger{create_frame_upload_buffer(buffer_usage::constant, larger_data)};
    constexpr std::array<std::uint16_t, 6> index_data{0, 1, 2, 2, 1, 3};
    const auto indices{create_frame_upload_buffer(buffer_usage::index, std::as_bytes(std::span{index_data}))};
    ASSERT_TRUE(first_constant_buffer && vertices && larger && indices);
    EXPECT_EQ(first_constant_buffer->size, 33u);
    EXPECT_EQ(larger->size, 257u);
    EXPECT_EQ(vertices->size, sizeof(positions));
    EXPECT_EQ(indices->size, sizeof(index_data));
    const auto handle{begin_render_pass(pass())};
    const auto encoder{begin_encode(handle)};
    draw(*encoder, *vertices, *indices, *first_constant_buffer);
    encoder->set_constant_buffer(0, *larger);
    encoder->set_vertex_buffer(*vertices, 16);
    encoder->set_index_buffer(*indices, index_format::uint16);
    encoder->set_scissor_rect({0, 0, 1, 4});
    encoder->draw_indexed(6);
    end_encode(encoder);
    ASSERT_TRUE(end_render_pass(handle));
    ASSERT_TRUE(end_frame());
    const auto request{readback(target_, {0, 0, 4, 4}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(request);
    ASSERT_TRUE(submit_and_wait());
    const auto pixels{poll(*request)};
    ASSERT_EQ(pixels.status, readback_status::ready);
    ASSERT_EQ(pixels.value.data.size(), 64u);
    for (std::size_t index{0}; index < 16; ++index)
    {
        const std::uint32_t expected{index % 4 == 0 ? 0xFF00FF00u : index % 4 == 1 ? 0xFF0000FFu :
                                                                                     0u};
        std::uint32_t actual{};
        std::memcpy(&actual, pixels.value.data.data() + index * 4, 4);
        EXPECT_EQ(actual, expected) << index;
    }
    destroy(*request);
}
} // namespace
