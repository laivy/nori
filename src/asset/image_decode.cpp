#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <vector>

#include <nori/core/vector.hpp>

#include "image_decode.hpp"

#ifdef _WIN32
#include <DirectXTex.h>
#include <objbase.h>
#endif

namespace
{
constexpr std::uint32_t maximum_dimension{16384};

#ifdef _WIN32
class com_apartment final
{
public:
    com_apartment() :
        result_{CoInitializeEx(nullptr, COINIT_MULTITHREADED)}
    {
    }

    ~com_apartment()
    {
        if (result_ == S_OK || result_ == S_FALSE)
        {
            CoUninitialize();
        }
    }

    [[nodiscard]] bool valid() const noexcept
    {
        return SUCCEEDED(result_) || result_ == RPC_E_CHANGED_MODE;
    }

private:
    HRESULT result_;
};

bool is_dds(const std::span<const std::byte> data)
{
    constexpr std::array signature{
        std::byte{'D'}, std::byte{'D'}, std::byte{'S'}, std::byte{' '}
    };
    return data.size() >= signature.size() &&
        std::equal(signature.begin(), signature.end(), data.begin());
}

std::string hresult_diagnostics(const char* operation, const HRESULT result)
{
    return std::string{operation} + " failed with HRESULT " + std::to_string(result);
}

nori::asset::result<DirectX::ScratchImage> load_image(const std::span<const std::byte> data)
{
    if (data.empty())
    {
        return std::unexpected{nori::asset::failure{
            .code = nori::asset::error_code::invalid_format,
            .diagnostics = "image is empty"
        }};
    }

    DirectX::ScratchImage image;
    DirectX::TexMetadata metadata{};
    HRESULT result{E_FAIL};
    if (is_dds(data))
    {
        result = DirectX::LoadFromDDSMemory(
            data.data(), data.size(), DirectX::DDS_FLAGS_NONE, &metadata, image
        );
    }
    else
    {
        result = DirectX::LoadFromWICMemory(
            data.data(), data.size(), DirectX::WIC_FLAGS_IGNORE_SRGB, &metadata, image
        );
        if (FAILED(result))
        {
            image.Release();
            result = DirectX::LoadFromTGAMemory(
                data.data(), data.size(), DirectX::TGA_FLAGS_NONE, &metadata, image
            );
        }
    }
    if (FAILED(result))
    {
        return std::unexpected{nori::asset::failure{
            .code = nori::asset::error_code::unsupported_format,
            .diagnostics = hresult_diagnostics("DirectXTex image decode", result)
        }};
    }
    if (metadata.dimension != DirectX::TEX_DIMENSION_TEXTURE2D || metadata.arraySize != 1 || metadata.depth != 1 || metadata.IsCubemap() || metadata.width == 0 || metadata.height == 0 || metadata.width > maximum_dimension || metadata.height > maximum_dimension)
    {
        return std::unexpected{nori::asset::failure{
            .code = nori::asset::error_code::unsupported_format,
            .diagnostics = "only single 2D images are supported"
        }};
    }
    if (DirectX::IsTypeless(metadata.format) || DirectX::IsPlanar(metadata.format) || DirectX::BitsPerColor(metadata.format) > 8)
    {
        return std::unexpected{nori::asset::failure{
            .code = nori::asset::error_code::unsupported_format,
            .diagnostics = "HDR, planar, and typeless images are not supported"
        }};
    }
    return image;
}

nori::asset::result<DirectX::ScratchImage> convert_to_rgba8(const DirectX::Image& source)
{
    DirectX::ScratchImage result;
    HRESULT hr{};
    if (source.format == DXGI_FORMAT_R8G8B8A8_UNORM)
    {
        hr = result.InitializeFromImage(source);
    }
    else if (DirectX::IsCompressed(source.format))
    {
        hr = DirectX::Decompress(source, DXGI_FORMAT_R8G8B8A8_UNORM, result);
    }
    else
    {
        hr = DirectX::Convert(
            source,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            DirectX::TEX_FILTER_DEFAULT,
            DirectX::TEX_THRESHOLD_DEFAULT,
            result
        );
    }
    if (FAILED(hr))
    {
        return std::unexpected{nori::asset::failure{
            .code = nori::asset::error_code::invalid_format,
            .diagnostics = hresult_diagnostics("DirectXTex RGBA conversion", hr)
        }};
    }
    return result;
}

std::size_t full_mip_count(std::size_t width, std::size_t height)
{
    std::size_t result{1};
    while (width > 1 || height > 1)
    {
        width = std::max<std::size_t>(1, width / 2);
        height = std::max<std::size_t>(1, height / 2);
        ++result;
    }
    return result;
}

void write_normal_mip(const DirectX::Image& source, DirectX::Image& destination)
{
    for (std::size_t y{0}; y < destination.height; ++y)
    {
        auto output{destination.pixels + y * destination.rowPitch};
        for (std::size_t x{0}; x < destination.width; ++x)
        {
            nori::core::vec3 normal{};
            float alpha{};
            std::size_t sample_count{};
            for (std::size_t offset_y{0}; offset_y < 2; ++offset_y)
            {
                const std::size_t source_y{std::min(source.height - 1, y * 2 + offset_y)};
                auto input{source.pixels + source_y * source.rowPitch};
                for (std::size_t offset_x{0}; offset_x < 2; ++offset_x)
                {
                    const std::size_t source_x{std::min(source.width - 1, x * 2 + offset_x)};
                    auto pixel{input + source_x * 4};
                    normal += nori::core::vec3{
                        static_cast<float>(pixel[0]) / 127.5f - 1.0f,
                        static_cast<float>(pixel[1]) / 127.5f - 1.0f,
                        static_cast<float>(pixel[2]) / 127.5f - 1.0f,
                    };
                    alpha += pixel[3];
                    ++sample_count;
                }
            }
            normal = normal.normalized();
            auto encode = [](const float value)
            {
                return static_cast<std::uint8_t>(std::clamp(value * 127.5f + 127.5f, 0.0f, 255.0f));
            };
            output[x * 4 + 0] = encode(normal.x);
            output[x * 4 + 1] = encode(normal.y);
            output[x * 4 + 2] = encode(normal.z);
            output[x * 4 + 3] = static_cast<std::uint8_t>(alpha / sample_count);
        }
    }
}

nori::asset::result<DirectX::ScratchImage> generate_mips(const DirectX::Image& source, const nori::resource::image_format format)
{
    DirectX::ScratchImage result;
    if (source.width == 1 && source.height == 1)
    {
        const HRESULT initialized{result.InitializeFromImage(source)};
        if (FAILED(initialized))
        {
            return std::unexpected{nori::asset::failure{
                .code = nori::asset::error_code::invalid_format,
                .diagnostics = hresult_diagnostics("DirectXTex mip initialization", initialized)
            }};
        }
        return result;
    }
    if (format == nori::resource::image_format::bc5_unorm)
    {
        const std::size_t mip_count{full_mip_count(source.width, source.height)};
        const HRESULT initialized{result.Initialize2D(
            DXGI_FORMAT_R8G8B8A8_UNORM, source.width, source.height, 1, mip_count
        )};
        if (FAILED(initialized))
        {
            return std::unexpected{nori::asset::failure{
                .code = nori::asset::error_code::invalid_format,
                .diagnostics = hresult_diagnostics("DirectXTex mip initialization", initialized)
            }};
        }
        auto base{const_cast<DirectX::Image*>(result.GetImage(0, 0, 0))};
        for (std::size_t y{0}; y < source.height; ++y)
        {
            std::memcpy(
                base->pixels + y * base->rowPitch,
                source.pixels + y * source.rowPitch,
                source.width * 4
            );
        }
        for (std::size_t mip{1}; mip < mip_count; ++mip)
        {
            auto destination{const_cast<DirectX::Image*>(result.GetImage(mip, 0, 0))};
            write_normal_mip(*result.GetImage(mip - 1, 0, 0), *destination);
        }
        return result;
    }

    DirectX::TEX_FILTER_FLAGS flags{DirectX::TEX_FILTER_DEFAULT};
    if (format == nori::resource::image_format::bc7_unorm_srgb)
    {
        flags = static_cast<DirectX::TEX_FILTER_FLAGS>(
            flags | DirectX::TEX_FILTER_SRGB_IN | DirectX::TEX_FILTER_SRGB_OUT
        );
    }
    const HRESULT hr{DirectX::GenerateMipMaps(source, flags, 0, result)};
    if (FAILED(hr))
    {
        return std::unexpected{nori::asset::failure{
            .code = nori::asset::error_code::invalid_format,
            .diagnostics = hresult_diagnostics("DirectXTex mip generation", hr)
        }};
    }
    return result;
}

DXGI_FORMAT to_dxgi_format(const nori::resource::image_format format)
{
    switch (format)
    {
    case nori::resource::image_format::bc5_unorm:
    {
        return DXGI_FORMAT_BC5_UNORM;
    }
    case nori::resource::image_format::bc7_unorm:
    {
        return DXGI_FORMAT_BC7_UNORM;
    }
    case nori::resource::image_format::bc7_unorm_srgb:
    {
        return DXGI_FORMAT_BC7_UNORM_SRGB;
    }
    default:
        return DXGI_FORMAT_UNKNOWN;
    }
}

nori::asset::result<DirectX::ScratchImage> compress_image(const DirectX::ScratchImage& source, const nori::resource::image_format format, const nori::asset::texture_compression_quality quality)
{
    DirectX::ScratchImage result;
    DirectX::TEX_COMPRESS_FLAGS flags{DirectX::TEX_COMPRESS_PARALLEL};
    if ((format == nori::resource::image_format::bc7_unorm ||
            format == nori::resource::image_format::bc7_unorm_srgb) &&
        quality == nori::asset::texture_compression_quality::quick)
    {
        flags = static_cast<DirectX::TEX_COMPRESS_FLAGS>(
            flags | DirectX::TEX_COMPRESS_BC7_QUICK
        );
    }
    const HRESULT hr{DirectX::Compress(
        source.GetImages(),
        source.GetImageCount(),
        source.GetMetadata(),
        to_dxgi_format(format),
        flags,
        DirectX::TEX_THRESHOLD_DEFAULT,
        result
    )};
    if (FAILED(hr))
    {
        return std::unexpected{nori::asset::failure{
            .code = nori::asset::error_code::invalid_format,
            .diagnostics = hresult_diagnostics("DirectXTex BC compression", hr)
        }};
    }
    return result;
}

nori::resource::image copy_image(const DirectX::ScratchImage& source, const nori::resource::image_format format)
{
    const DirectX::TexMetadata metadata{source.GetMetadata()};
    nori::resource::image result{
        .format = format,
        .width = static_cast<std::uint32_t>(metadata.width),
        .height = static_cast<std::uint32_t>(metadata.height),
        .mip_levels = static_cast<std::uint32_t>(metadata.mipLevels)
    };
    for (std::size_t mip{0}; mip < metadata.mipLevels; ++mip)
    {
        const DirectX::Image* image{source.GetImage(mip, 0, 0)};
        const std::size_t row_count{DirectX::IsCompressed(image->format) ? std::max<std::size_t>(1, (image->height + 3) / 4) : image->height};
        const std::size_t row_size{DirectX::IsCompressed(image->format) ? std::max<std::size_t>(1, (image->width + 3) / 4) * 16 : image->width * 4};
        const std::size_t offset{result.data.size()};
        result.data.resize(offset + row_count * row_size);
        for (std::size_t row{0}; row < row_count; ++row)
        {
            std::memcpy(
                result.data.data() + offset + row * row_size,
                image->pixels + row * image->rowPitch,
                row_size
            );
        }
    }
    return result;
}
#endif
} // namespace

namespace nori::asset
{
result<resource::image> decode_image(const std::span<const std::byte> data, const resource::image_format format, const texture_compression_quality quality)
{
#ifdef _WIN32
    if (format != resource::image_format::rgba8_unorm &&
        format != resource::image_format::bc5_unorm &&
        format != resource::image_format::bc7_unorm &&
        format != resource::image_format::bc7_unorm_srgb)
    {
        return std::unexpected{failure{.code = error_code::unsupported_format}};
    }
    thread_local const com_apartment apartment;
    if (!apartment.valid())
    {
        return std::unexpected{failure{
            .code = error_code::invalid_format,
            .diagnostics = "COM initialization failed"
        }};
    }
    auto decoded{load_image(data)};
    if (!decoded)
    {
        return std::unexpected{std::move(decoded.error())};
    }
    const DirectX::Image* top_level{decoded->GetImage(0, 0, 0)};
    if (top_level == nullptr)
    {
        return std::unexpected{failure{.code = error_code::invalid_format}};
    }
    auto rgba{convert_to_rgba8(*top_level)};
    if (!rgba)
    {
        return std::unexpected{std::move(rgba.error())};
    }
    if (format == resource::image_format::rgba8_unorm)
    {
        return copy_image(*rgba, format);
    }
    auto mips{generate_mips(*rgba->GetImage(0, 0, 0), format)};
    if (!mips)
    {
        return std::unexpected{std::move(mips.error())};
    }
    auto compressed{compress_image(*mips, format, quality)};
    if (!compressed)
    {
        return std::unexpected{std::move(compressed.error())};
    }
    return copy_image(*compressed, format);
#else
    static_cast<void>(data);
    static_cast<void>(format);
    static_cast<void>(quality);
    return std::unexpected{failure{.code = error_code::unsupported_platform}};
#endif
}
} // namespace nori::asset
