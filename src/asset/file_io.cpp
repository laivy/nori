#include <fstream>
#include <limits>

#include "file_io.hpp"

namespace nori::asset
{
result<std::vector<std::byte>> read_file(const std::filesystem::path& path)
{
    std::ifstream input{path, std::ios::binary | std::ios::ate};
    if (!input)
    {
        return std::unexpected{failure{}};
    }

    const std::streamoff size{input.tellg()};
    if (size < 0 || size > std::numeric_limits<std::streamsize>::max())
    {
        return std::unexpected{failure{}};
    }

    std::vector<std::byte> data(static_cast<std::size_t>(size));
    input.seekg(0);
    if (!input.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size())))
    {
        return std::unexpected{failure{}};
    }
    return data;
}
} // namespace nori::asset
