#include "text_test_helpers.hpp"

namespace nori::graphics::test_support
{
std::vector<std::byte> text_font_fixture(std::string_view name)
{
    return load_test_font(std::filesystem::path{__FILE__}.parent_path() / "testdata" / name);
}

std::vector<std::byte> draw_layout(nori::graphics::text::layout_handle layout, nori::core::vec4 tint, const std::function<void()>& before_replay, const std::function<void()>& after_replay, std::optional<nori::graphics::text::effect> effect, nori::core::vec2 position, const std::function<void(nori::graphics::render_pass_handle)>& record)
{
    const auto target{create_texture({texture_format::rgba8_unorm, 160, 96, 1, 1, texture_usage::render_target | texture_usage::copy_source}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    EXPECT_TRUE(target);
    if (!target)
    {
        return {};
    }
    EXPECT_TRUE(begin_frame());
    const auto pass{begin_render_pass({.render_targets = {{*target, {begin_op::clear}, {end_op::store}}}})};
    if (record)
    {
        record(pass);
    }
    else
    {
        const auto encoder{begin_encode(pass)};
        encoder->draw_text({layout, position, tint, effect});
        end_encode(encoder);
    }
    if (before_replay)
    {
        before_replay();
    }
    EXPECT_TRUE(end_render_pass(pass));
    if (after_replay)
    {
        after_replay();
    }
    EXPECT_TRUE(end_frame());
    const auto request{readback(*target, {0, 0, 160, 96, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    EXPECT_TRUE(request);
    if (!request)
    {
        destroy(*target);
        return {};
    }
    std::vector<std::byte> pixels;
    EXPECT_TRUE(submit());
    const auto deadline{std::chrono::steady_clock::now() + std::chrono::seconds{5}};
    while (std::chrono::steady_clock::now() < deadline)
    {
        auto current{poll(*request)};
        EXPECT_NE(current.status, readback_status::failed);
        if (current.status == readback_status::ready)
        {
            pixels = std::move(current.value.data);
            break;
        }
        std::this_thread::yield();
    }
    EXPECT_FALSE(pixels.empty());
    destroy(*request);
    destroy(*target);
    return pixels;
}
} // namespace nori::graphics::test_support
