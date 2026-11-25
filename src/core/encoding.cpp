#include <limits>

#include <nori/core/encoding.hpp>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace nori::core
{
std::filesystem::path utf8_to_path(std::string_view text)
{
    return std::filesystem::path{std::u8string_view{reinterpret_cast<const char8_t*>(text.data()), text.size()}};
}

std::string path_to_utf8(const std::filesystem::path& path)
{
    const std::u8string u8path{path.generic_u8string()};
    return std::string{reinterpret_cast<const char*>(u8path.data()), u8path.size()};
}

#ifdef _WIN32
std::wstring utf8_to_utf16(std::string_view text)
{
    if (text.empty())
    {
        return {};
    }
    if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        return {};
    }

    const int required_size{::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0)};
    if (required_size == 0)
    {
        return {};
    }

    std::wstring result(static_cast<std::size_t>(required_size), L'\0');
    if (::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), required_size) != required_size)
    {
        return {};
    }
    return result;
}

std::string utf16_to_utf8(std::wstring_view text)
{
    if (text.empty())
    {
        return {};
    }
    if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        return {};
    }

    const int required_size{::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr)};
    if (required_size == 0)
    {
        return {};
    }

    std::string result(static_cast<std::size_t>(required_size), '\0');
    if (!::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), required_size, nullptr, nullptr))
    {
        return {};
    }
    return result;
}
#endif
} // namespace nori::core
