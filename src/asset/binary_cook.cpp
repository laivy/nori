#include <cstddef>
#include <utility>
#include <vector>

#include <nori/asset.hpp>

#include "file_io.hpp"

namespace nori::asset
{
result<resource::binary> cook_binary(const std::filesystem::path& source_path)
{
    return read_file(source_path).transform([](std::vector<std::byte> data)
    {
        return resource::binary{.data = std::move(data)};
    });
}
} // namespace nori::asset
