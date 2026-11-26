#include "context.hpp"
#include "descriptor_allocator.hpp"
#include "text_test_helpers.hpp"

namespace
{
class text_failure_backend_test : public text_test
{
};
} // namespace

namespace nori::graphics
{
using test_support::draw_layout;

TEST_F(text_failure_backend_test, atlas_allocation_failure_preserves_error_and_previous_layout)
{
    const auto bytes{load_test_font()};
    const auto font{text::create_font({bytes, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(font);
    const text::layout_desc desc{.font = *font, .font_size = 32.0f, .color = {1.0f, 1.0f, 1.0f, 1.0f}};
    const auto layout{text::create_layout(desc, "ABC").transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(layout);
    const auto original_metrics{text::metrics(*layout)};
    const auto expected{draw_layout(*layout)};
    auto& descriptors{*ctx()->descriptors()};
    std::vector<descriptor_handle> occupied;
    for (;;)
    {
        const auto descriptor{descriptors.allocate(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)};
        if (descriptor.cpu.ptr == 0)
        {
            break;
        }
        occupied.push_back(descriptor);
    }
    auto large_desc{desc};
    large_desc.font_size = 192.0f;
    std::string expanded;
    for (std::uint32_t code{0xAC00}; code < 0xAC00 + 300; ++code)
    {
        expanded.push_back(static_cast<char>(0xE0 | (code >> 12)));
        expanded.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
        expanded.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    }
    const auto failed{update(*layout, large_desc, expanded)};
    EXPECT_FALSE(failed);
    if (!failed)
    {
        EXPECT_EQ(failed.error(), error_code::descriptor_allocation_failed);
    }
    for (const auto& descriptor : occupied)
    {
        descriptors.free(descriptor);
    }
    EXPECT_FLOAT_EQ(text::metrics(*layout).width, original_metrics.width);
    EXPECT_FLOAT_EQ(text::metrics(*layout).height, original_metrics.height);
    EXPECT_EQ(draw_layout(*layout), expected);
    ASSERT_TRUE(text::update(*layout, "CBA"));
    text::destroy(*layout);
    text::destroy(*font);
}
} // namespace nori::graphics
