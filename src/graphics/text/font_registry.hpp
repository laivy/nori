#pragma once
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <dwrite_3.h>
#include <wrl/client.h>

#include <nori/core/slot_map.hpp>
#include <nori/graphics/error_code.hpp>
#include <nori/graphics/text.hpp>

namespace nori::graphics::text
{
struct font_selection
{
    Microsoft::WRL::ComPtr<IDWriteFontFace5> face;
    Microsoft::WRL::ComPtr<IDWriteFontCollection1> collection;
    std::wstring family;
    std::vector<DWRITE_FONT_AXIS_VALUE> axes;
};

class font_registry
{
public:
    ~font_registry();

    font_registry(const font_registry&) = delete;
    font_registry(font_registry&&) = delete;
    font_registry& operator=(const font_registry&) = delete;
    font_registry& operator=(font_registry&&) = delete;

    static std::unique_ptr<font_registry> create(IDWriteFactory5* factory);

    result<font_handle> create_font(font_desc desc);
    void destroy(font_handle handle);
    result<const font_selection*> select(font_handle handle, bool bold, bool italic);

private:
    struct font
    {
        Microsoft::WRL::ComPtr<IDWriteFontFile> file;
        std::vector<Microsoft::WRL::ComPtr<IDWriteFontFace5>> faces;
        std::array<std::optional<font_selection>, 4> selections; // 0: 일반, 1: 굵게, 2: 기울임, 3: 굵게 + 기울임
    };

    font_registry(IDWriteFactory5* factory, Microsoft::WRL::ComPtr<IDWriteInMemoryFontFileLoader> font_loader);

    result<font_selection> make_selection(Microsoft::WRL::ComPtr<IDWriteFontFace5> face);

    Microsoft::WRL::ComPtr<IDWriteFactory5> factory_;
    Microsoft::WRL::ComPtr<IDWriteInMemoryFontFileLoader> font_loader_;
    core::slot_map<font_handle, font> fonts_;
};
} // namespace nori::graphics::text
