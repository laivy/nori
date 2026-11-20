#include <d2d1.h>

#include <algorithm>
#include <bit>
#include <cstdint>

#include <wrl/client.h>

#include "text/glyph_rasterizer.hpp"

namespace
{
class raster_surface
{
public:
    raster_surface() = default;
    ~raster_surface();

    raster_surface(const raster_surface&) = delete;
    raster_surface& operator=(const raster_surface&) = delete;

    bool reserve(std::uint32_t width, std::uint32_t height);
    HDC dc() const;
    const std::byte* pixels() const;
    std::uint32_t width() const;
    std::uint32_t height() const;

private:
    HDC dc_{};
    HBITMAP bitmap_{};
    HGDIOBJ previous_{};
    void* pixels_{};
    std::uint32_t width_{};
    std::uint32_t height_{};
};

raster_surface::~raster_surface()
{
    if (previous_)
    {
        ::SelectObject(dc_, previous_);
    }
    if (bitmap_)
    {
        ::DeleteObject(bitmap_);
    }
    if (dc_)
    {
        ::DeleteDC(dc_);
    }
}

bool raster_surface::reserve(std::uint32_t width, std::uint32_t height)
{
    if (width <= width_ && height <= height_)
    {
        return true;
    }
    if (!dc_)
    {
        dc_ = ::CreateCompatibleDC(nullptr);
        if (!dc_)
        {
            return false;
        }
    }
    const auto capacity_width{std::bit_ceil(std::max(width, width_))};
    const auto capacity_height{std::bit_ceil(std::max(height, height_))};
    BITMAPINFO info{};
    info.bmiHeader = {sizeof(BITMAPINFOHEADER), static_cast<LONG>(capacity_width), -static_cast<LONG>(capacity_height), 1, 32, BI_RGB};
    void* pixels{};
    const auto bitmap{::CreateDIBSection(dc_, &info, DIB_RGB_COLORS, &pixels, nullptr, 0)};
    if (!bitmap)
    {
        return false;
    }
    const auto previous{::SelectObject(dc_, bitmap)};
    if (!previous || previous == HGDI_ERROR)
    {
        ::DeleteObject(bitmap);
        return false;
    }
    if (bitmap_)
    {
        ::DeleteObject(bitmap_);
    }
    else
    {
        previous_ = previous;
    }
    bitmap_ = bitmap;
    pixels_ = pixels;
    width_ = capacity_width;
    height_ = capacity_height;
    return true;
}

HDC raster_surface::dc() const
{
    return dc_;
}

const std::byte* raster_surface::pixels() const
{
    return static_cast<const std::byte*>(pixels_);
}

std::uint32_t raster_surface::width() const
{
    return width_;
}

std::uint32_t raster_surface::height() const
{
    return height_;
}
}

namespace nori::graphics::text
{
class glyph_rasterizer::impl
{
public:
    result<glyph_masks> raster_outline(IDWriteFontFace* face, std::uint16_t index, float em_size, bool sideways, float thickness, RECT bounds);

private:
    bool prepare(std::uint32_t width, std::uint32_t height);
    bool draw(ID2D1Geometry* geometry, RECT bounds, std::vector<std::byte>& coverage);

    Microsoft::WRL::ComPtr<ID2D1Factory> factory_;
    Microsoft::WRL::ComPtr<ID2D1StrokeStyle> stroke_;
    raster_surface surface_;
    Microsoft::WRL::ComPtr<ID2D1DCRenderTarget> target_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;
};

result<glyph_masks> glyph_rasterizer::impl::raster_outline(IDWriteFontFace* face, std::uint16_t index, float em_size, bool sideways, float thickness, RECT bounds)
{
    const auto width{static_cast<std::uint32_t>(bounds.right - bounds.left)};
    const auto height{static_cast<std::uint32_t>(bounds.bottom - bounds.top)};
    if (!prepare(width, height))
    {
        return std::unexpected{error_code::resource_creation_failed};
    }
    Microsoft::WRL::ComPtr<ID2D1PathGeometry> contour;
    Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(factory_->CreatePathGeometry(&contour)) ||
        FAILED(contour->Open(&sink)) ||
        FAILED(face->GetGlyphRunOutline(em_size, &index, nullptr, nullptr, 1, sideways, FALSE, sink.Get())) ||
        FAILED(sink->Close()))
    {
        return std::unexpected{error_code::resource_creation_failed};
    }
    Microsoft::WRL::ComPtr<ID2D1PathGeometry> widened;
    sink.Reset();
    if (FAILED(factory_->CreatePathGeometry(&widened)) ||
        FAILED(widened->Open(&sink)) ||
        FAILED(contour->Widen(thickness * 2.0f, stroke_.Get(), nullptr, 0.1f, sink.Get())) ||
        FAILED(sink->Close()))
    {
        return std::unexpected{error_code::resource_creation_failed};
    }
    Microsoft::WRL::ComPtr<ID2D1PathGeometry> outer;
    sink.Reset();
    if (FAILED(factory_->CreatePathGeometry(&outer)) || FAILED(outer->Open(&sink)) || FAILED(widened->CombineWithGeometry(contour.Get(), D2D1_COMBINE_MODE_EXCLUDE, nullptr, 0.1f, sink.Get())) || FAILED(sink->Close()))
    {
        return std::unexpected{error_code::resource_creation_failed};
    }
    glyph_masks coverage{std::vector<std::byte>(static_cast<std::size_t>(width) * height), std::vector<std::byte>(static_cast<std::size_t>(width) * height)};
    if (!draw(outer.Get(), bounds, coverage.outline) || !draw(contour.Get(), bounds, coverage.fill))
    {
        return std::unexpected{error_code::resource_creation_failed};
    }
    for (std::size_t i{}; i < coverage.fill.size(); ++i)
    {
        coverage.outline[i] = std::min(coverage.outline[i], std::byte(255 - std::to_integer<int>(coverage.fill[i])));
    }
    return coverage;
}

bool glyph_rasterizer::impl::prepare(std::uint32_t width, std::uint32_t height)
{
    if (!factory_ && FAILED(::D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory_.GetAddressOf())))
    {
        return false;
    }
    if (!stroke_)
    {
        auto properties{D2D1::StrokeStyleProperties()};
        properties.lineJoin = D2D1_LINE_JOIN_ROUND;
        if (FAILED(factory_->CreateStrokeStyle(properties, nullptr, 0, &stroke_)))
        {
            return false;
        }
    }
    if (!surface_.reserve(width, height))
    {
        return false;
    }
    if (!target_)
    {
        const auto properties{D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.0f, 96.0f)};
        if (FAILED(factory_->CreateDCRenderTarget(&properties, &target_)))
        {
            return false;
        }
    }
    if (!brush_ && FAILED(target_->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &brush_)))
    {
        return false;
    }
    const RECT capacity{0, 0, static_cast<LONG>(surface_.width()), static_cast<LONG>(surface_.height())};
    return SUCCEEDED(target_->BindDC(surface_.dc(), &capacity));
}

bool glyph_rasterizer::impl::draw(ID2D1Geometry* geometry, RECT bounds, std::vector<std::byte>& coverage)
{
    target_->BeginDraw();
    target_->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
    target_->SetTransform(D2D1::Matrix3x2F::Translation(-static_cast<float>(bounds.left), -static_cast<float>(bounds.top)));
    target_->FillGeometry(geometry, brush_.Get());
    if (const HRESULT res{target_->EndDraw()}; FAILED(res))
    {
        if (res == D2DERR_RECREATE_TARGET)
        {
            brush_.Reset();
            target_.Reset();
        }
        return false;
    }
    const auto width{static_cast<std::size_t>(bounds.right - bounds.left)};
    const auto height{static_cast<std::size_t>(bounds.bottom - bounds.top)};
    for (std::size_t y{}; y < height; ++y)
    {
        for (std::size_t x{}; x < width; ++x)
        {
            coverage[y * width + x] = surface_.pixels()[(y * surface_.width() + x) * 4 + 3];
        }
    }
    return true;
}

glyph_rasterizer::glyph_rasterizer() :
    impl_{std::make_unique<impl>()}
{
}

glyph_rasterizer::~glyph_rasterizer() = default;

result<glyph_masks> glyph_rasterizer::raster_outline(IDWriteFontFace* face, std::uint16_t index, float em_size, bool sideways, float thickness, RECT bounds)
{
    return impl_->raster_outline(face, index, em_size, sideways, thickness, bounds);
}
}
