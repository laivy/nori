#pragma once
#include <filesystem>
#include <string>
#include <string_view>

namespace nori::core
{
std::filesystem::path utf8_to_path(std::string_view text);
std::string path_to_utf8(const std::filesystem::path& path);

#ifdef _WIN32
std::wstring utf8_to_utf16(std::string_view text);
std::string utf16_to_utf8(std::wstring_view text);
#endif
} // namespace nori::core
