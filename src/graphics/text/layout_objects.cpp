#include "text/layout_objects.hpp"

namespace nori::graphics::text
{
color_effect::color_effect(core::vec4 tint, std::optional<outline_desc> stroke) :
    color{tint},
    outline{stroke}
{
}

image_object::image_object(inline_image desc) :
    desc_{desc}
{
}

HRESULT STDMETHODCALLTYPE image_object::Draw(void* context, IDWriteTextRenderer* renderer, FLOAT x, FLOAT y, BOOL sideways, BOOL right_to_left, IUnknown* effect)
{
    return renderer->DrawInlineObject(context, x, y, this, sideways, right_to_left, effect);
}

HRESULT STDMETHODCALLTYPE image_object::GetMetrics(DWRITE_INLINE_OBJECT_METRICS* measured)
{
    *measured = {desc_.size.x, desc_.size.y, desc_.size.y, FALSE};
    return S_OK;
}

HRESULT STDMETHODCALLTYPE image_object::GetOverhangMetrics(DWRITE_OVERHANG_METRICS* overhang)
{
    *overhang = {};
    return S_OK;
}

HRESULT STDMETHODCALLTYPE image_object::GetBreakConditions(DWRITE_BREAK_CONDITION* before, DWRITE_BREAK_CONDITION* after)
{
    *before = DWRITE_BREAK_CONDITION_CAN_BREAK;
    *after = DWRITE_BREAK_CONDITION_CAN_BREAK;
    return S_OK;
}

inline_image image_object::description() const
{
    return desc_;
}
} // namespace nori::graphics::text
