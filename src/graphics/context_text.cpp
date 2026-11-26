#include "context.hpp"
#include "text/manager.hpp"

namespace nori::graphics
{
result<text::font_handle> context::create_font(text::font_desc desc)
{
    return texts_->create_font(desc);
}

result<text::font_handle> context::create_font(nori::resource::handle source, std::uint32_t face_index)
{
    return texts_->create_font(source, face_index);
}

void context::destroy(text::font_handle handle)
{
    return texts_->destroy(handle);
}

result<text::layout_handle> context::create_text_layout(text::layout_desc desc, std::string_view utf8)
{
    return texts_->create_layout(desc, utf8);
}

result<text::layout_handle> context::create_text_layout(text::layout_desc desc, std::span<const text::item> items)
{
    return texts_->create_layout(desc, items);
}

result<void> context::update_text_layout(text::layout_handle layout, std::string_view utf8)
{
    return texts_->update(layout, utf8);
}

result<void> context::update_text_layout(text::layout_handle layout, text::layout_desc desc, std::string_view utf8)
{
    return texts_->update(layout, desc, utf8);
}

result<void> context::update_text_layout(text::layout_handle layout, std::span<const text::item> items)
{
    return texts_->update(layout, items);
}

result<void> context::update_text_layout(text::layout_handle layout, text::layout_desc desc, std::span<const text::item> items)
{
    return texts_->update(layout, desc, items);
}

text::layout_metrics context::text_layout_metrics(text::layout_handle layout) const
{
    return texts_->metrics(layout);
}

void context::destroy(text::layout_handle handle)
{
    return texts_->destroy(handle);
}
} // namespace nori::graphics
