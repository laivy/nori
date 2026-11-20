#include <gtest/gtest.h>

#include "frame_upload_test_helpers.hpp"

namespace
{
TEST_F(frame_upload_buffer_lifetime_test, shutdown_discards_unsubmitted_frame_data_and_allows_reinitialization)
{
    const auto bytes{load_test_font()};
    ASSERT_FALSE(bytes.empty());
    const auto font{text::create_font({bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(font);
    const auto layout{text::create_layout({.font = *font, .font_size = 20.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}}, "shutdown atlas").transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(layout);
    ASSERT_TRUE(begin_frame());
    constexpr auto vertices{quad(0, 4)};
    constexpr std::array<std::uint16_t, 6> index_data{0, 1, 2, 2, 1, 3};
    const auto vertex_view{create_frame_upload_buffer(buffer_usage::vertex, std::as_bytes(std::span{vertices}))};
    const auto index_view{create_frame_upload_buffer(buffer_usage::index, std::as_bytes(std::span{index_data}))};
    ASSERT_TRUE(vertex_view && index_view);
    const auto handle{begin_render_pass(pass())};
    const auto encoder{begin_encode(handle)};
    draw(*encoder, *vertex_view, *index_view, false);
    end_encode(encoder);
    ASSERT_TRUE(end_render_pass(handle));
    shutdown();
    ASSERT_TRUE(initialize());
    ASSERT_TRUE(begin_frame());
    ASSERT_TRUE(create_frame_upload_buffer(buffer_usage::vertex, std::as_bytes(std::span{vertices})));
    ASSERT_TRUE(end_frame());
}
} // namespace
