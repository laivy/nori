#pragma once
#include <dwrite.h>

#include <optional>

#include <wrl/implements.h>

#include <nori/graphics/text.hpp>

namespace nori::graphics::text
{
class color_effect final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IUnknown>
{
public:
    color_effect(core::vec4 tint, std::optional<outline_desc> stroke);

    core::vec4 color;
    std::optional<outline_desc> outline;
};

class image_object final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDWriteInlineObject>
{
public:
    explicit image_object(inline_image desc);

    HRESULT STDMETHODCALLTYPE Draw(void* context, IDWriteTextRenderer* renderer, FLOAT x, FLOAT y, BOOL sideways, BOOL right_to_left, IUnknown* effect) override;
    HRESULT STDMETHODCALLTYPE GetMetrics(DWRITE_INLINE_OBJECT_METRICS* measured) override;
    HRESULT STDMETHODCALLTYPE GetOverhangMetrics(DWRITE_OVERHANG_METRICS* overhang) override;
    HRESULT STDMETHODCALLTYPE GetBreakConditions(DWRITE_BREAK_CONDITION* before, DWRITE_BREAK_CONDITION* after) override;
    inline_image description() const;

private:
    inline_image desc_;
};
} // namespace nori::graphics::text
