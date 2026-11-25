#include <cstddef>
#include <vector>

#include <nori/asset.hpp>

#include "file_io.hpp"
#include "image_decode.hpp"

namespace nori::asset
{
result<resource::image> cook_image(const std::filesystem::path& source_path, resource::image_format format, texture_compression_quality quality)
{
    return read_file(source_path).and_then([format, quality](const std::vector<std::byte>& data)
    {
        return decode_image(data, format, quality);
    });
}
} // namespace nori::asset
