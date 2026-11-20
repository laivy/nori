#pragma once
#include <cstddef>
#include <filesystem>
#include <vector>

#include <nori/asset/error_code.hpp>

namespace nori::asset
{
result<std::vector<std::byte>> read_file(const std::filesystem::path& path);
} // namespace nori::asset
