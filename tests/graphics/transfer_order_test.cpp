#include "command_encoder_test_helpers.hpp"

namespace
{
TEST_F(transfer_order_test, update_preserves_prior_copy)
{
    const auto source{create_texture({texture_format::rgba8_unorm, 1, 1, 1, 1, texture_usage::shader_resource | texture_usage::copy_source | texture_usage::copy_destination}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto destination{create_texture({texture_format::rgba8_unorm, 1, 1, 1, 1, texture_usage::shader_resource | texture_usage::copy_source | texture_usage::copy_destination}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(source && destination);
    const std::uint32_t before{0xFF112233};
    const std::uint32_t after{0xFFAABBCC};
    ASSERT_TRUE(update(*source, {0, 0, 1, 1}, {std::as_bytes(std::span{&before, 1}), 4}));
    ASSERT_TRUE(copy(*source, {0, 0, 1, 1}, *destination, {0, 0, 1, 1}));
    ASSERT_TRUE(update(*source, {0, 0, 1, 1}, {std::as_bytes(std::span{&after, 1}), 4}));
    const auto request{readback(*destination, {0, 0, 1, 1}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(request);
    ASSERT_TRUE(submit_and_wait());
    const auto status{poll(*request)};
    ASSERT_EQ(status.status, readback_status::ready);
    std::uint32_t color{};
    std::memcpy(&color, status.value.data.data(), 4);
    EXPECT_EQ(color, before);
    destroy(*request);
    destroy(*destination);
    destroy(*source);
}

TEST_F(transfer_order_test, end_frame_completes_queued_transfers)
{
    const auto source{create_texture({texture_format::rgba8_unorm, 1, 1, 1, 1, texture_usage::shader_resource | texture_usage::copy_source | texture_usage::copy_destination}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto destination{create_texture({texture_format::rgba8_unorm, 1, 1, 1, 1, texture_usage::shader_resource | texture_usage::copy_source | texture_usage::copy_destination}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(source && destination);
    const std::uint32_t pixel{0xFF123456};
    ASSERT_TRUE(update(*source, {0, 0, 1, 1}, {std::as_bytes(std::span{&pixel, 1}), 4}));
    ASSERT_TRUE(begin_frame());
    ASSERT_TRUE(copy(*source, {0, 0, 1, 1}, *destination, {0, 0, 1, 1}));
    const auto pending{readback(*destination, {0, 0, 1, 1}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(pending);
    EXPECT_EQ(poll(*pending).status, readback_status::pending);
    ASSERT_TRUE(end_frame());
    const auto status{poll_until_complete(*pending)};
    ASSERT_EQ(status.status, readback_status::ready);
    std::uint32_t color{};
    std::memcpy(&color, status.value.data.data(), 4);
    EXPECT_EQ(color, pixel);
    destroy(*pending);
    destroy(*destination);
    destroy(*source);
}

TEST_F(transfer_order_test, frame_upload_copy_update_preserves_snapshots_without_early_submission)
{
    const auto source{create_texture({texture_format::rgba8_unorm, 1, 1, 1, 1, texture_usage::shader_resource | texture_usage::copy_source | texture_usage::copy_destination}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto destination{create_texture({texture_format::rgba8_unorm, 1, 1, 1, 1, texture_usage::shader_resource | texture_usage::copy_source | texture_usage::copy_destination}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(source && destination);
    ASSERT_TRUE(begin_frame());
    std::uint32_t input{0xFF112233};
    ASSERT_TRUE(update(*source, {0, 0, 1, 1}, {std::as_bytes(std::span{&input, 1}), 4}));
    for (int index{0}; index < 65; ++index)
    {
        ASSERT_TRUE(copy(*source, {0, 0, 1, 1}, *destination, {0, 0, 1, 1}));
    }
    input = 0xFFAABBCC;
    ASSERT_TRUE(update(*source, {0, 0, 1, 1}, {std::as_bytes(std::span{&input, 1}), 4}));
    input = 0;
    const auto old_value{readback(*destination, {0, 0, 1, 1}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto new_value{readback(*source, {0, 0, 1, 1}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(old_value && new_value);
    EXPECT_EQ(poll(*old_value).status, readback_status::pending);
    EXPECT_EQ(poll(*new_value).status, readback_status::pending);
    ASSERT_TRUE(end_frame());
    const std::array handles{*old_value, *new_value};
    const std::array<std::uint32_t, 2> expected{0xFF112233, 0xFFAABBCC};
    for (std::size_t index{0}; index < handles.size(); ++index)
    {
        const auto status{poll_until_complete(handles[index])};
        ASSERT_EQ(status.status, readback_status::ready);
        std::uint32_t pixel{};
        std::memcpy(&pixel, status.value.data.data(), 4);
        EXPECT_EQ(pixel, expected[index]);
        destroy(handles[index]);
    }
    destroy(*destination);
    destroy(*source);
}

TEST_F(transfer_order_test, transfers_preserve_completed_readbacks)
{
    const auto source{create_color(4, 3)};
    const auto copied{create_color()};
    const auto destination{create_color(2)};
    ASSERT_TRUE(source && copied && destination);
    std::array<std::uint32_t, 16> pixels;
    pixels.fill(0xFF336699);
    ASSERT_TRUE(update(*source, {0, 0, 4, 4}, {std::as_bytes(std::span{pixels}), 16}));
    ASSERT_TRUE(generate_mips(*source));
    ASSERT_TRUE(copy(*source, {0, 0, 4, 4}, *copied, {0, 0, 4, 4}));
    ASSERT_TRUE(blit(*copied, {0, 0, 4, 4}, *destination, {0, 0, 2, 2}, blit_filter::linear));
    const auto first{readback(*destination, {0, 0, 2, 2}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(first);
    expect_pixels(*first, pixels.front());
    const auto second{readback(*source, {0, 0, 1, 1, 2}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(second);
    expect_pixels(*second, pixels.front());
    expect_pixels(*first, pixels.front());
    destroy(*second);
    destroy(*first);
    destroy(*destination);
    destroy(*copied);
    destroy(*source);
}

TEST_F(transfer_order_test, compressed_texture_update_and_readback)
{
    const auto texture{create_texture({texture_format::bc7_unorm_srgb, 4, 4, 1, 1, texture_usage::shader_resource | texture_usage::copy_destination}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(texture);
    const std::array<std::byte, 16> block{std::byte{0x11}, std::byte{0x22}, std::byte{0x33}, std::byte{0x44}, std::byte{0x55}, std::byte{0x66}, std::byte{0x77}, std::byte{0x88}, std::byte{0x99}, std::byte{0xAA}, std::byte{0xBB}, std::byte{0xCC}, std::byte{0xDD}, std::byte{0xEE}, std::byte{0xFF}, std::byte{0x00}};
    ASSERT_TRUE(update(*texture, {0, 0, 4, 4}, {std::as_bytes(std::span{block}), 16}));
    const auto request{readback(*texture, {0, 0, 4, 4}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(request);
    ASSERT_TRUE(submit_and_wait());
    const auto status{poll(*request)};
    ASSERT_EQ(status.status, readback_status::ready);
    EXPECT_EQ(status.value.data.size(), 16u);
    EXPECT_EQ(status.value.row_pitch, 16u);
    EXPECT_EQ(status.value.data, (std::vector<std::byte>{block.begin(), block.end()}));
    destroy(*request);
    destroy(*texture);
}

TEST_F(transfer_order_test, frame_begin_flushes_prior_uploads_and_transfers)
{
    const std::uint32_t pixel{0xFF123456};
    const auto data{std::as_bytes(std::span{&pixel, 1})};
    const auto source_buffer{create_buffer({sizeof(pixel), buffer_usage::copy_source, buffer_access::gpu_only}, data).transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto destination_buffer{create_buffer({sizeof(pixel), buffer_usage::copy_destination, buffer_access::gpu_only}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(source_buffer && destination_buffer);

    const texture_usage usage{texture_usage::shader_resource | texture_usage::render_target | texture_usage::copy_source | texture_usage::copy_destination};
    const auto source{create_texture({texture_format::rgba8_unorm, 1, 1, 1, 1, usage}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto copied{create_texture({texture_format::rgba8_unorm, 1, 1, 1, 1, usage}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto destination{create_texture({texture_format::rgba8_unorm, 1, 1, 1, 1, usage}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(source && copied && destination);
    ASSERT_TRUE(update(*source, {0, 0, 1, 1}, {data, sizeof(pixel)}));

    ASSERT_TRUE(copy(*source_buffer, 0, *destination_buffer, 0, sizeof(pixel)));
    ASSERT_TRUE(update(*source_buffer, 0, data));
    ASSERT_TRUE(update(*source, {0, 0, 1, 1}, {data, sizeof(pixel)}));
    for (std::uint32_t i{0}; i < 128; ++i)
    {
        ASSERT_TRUE(copy(*source_buffer, 0, *destination_buffer, 0, sizeof(pixel)));
        ASSERT_TRUE(copy(*source, {0, 0, 1, 1}, *copied, {0, 0, 1, 1}));
        ASSERT_TRUE(blit(*copied, {0, 0, 1, 1}, *destination, {0, 0, 1, 1}));
    }
    ASSERT_TRUE(begin_frame());
    ASSERT_TRUE(end_frame());

    const auto request{readback(*destination, {0, 0, 1, 1}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(request);
    ASSERT_TRUE(submit_and_wait());
    const auto status{poll(*request)};
    ASSERT_EQ(status.status, readback_status::ready);
    ASSERT_EQ(status.value.data.size(), sizeof(pixel));
    std::uint32_t actual{};
    std::memcpy(&actual, status.value.data.data(), sizeof(actual));
    EXPECT_EQ(actual, pixel);

    destroy(*request);
    destroy(*destination);
    destroy(*copied);
    destroy(*source);
    destroy(*destination_buffer);
    destroy(*source_buffer);
}
} // namespace
