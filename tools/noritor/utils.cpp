#include "pch.hpp"

#include "utils.hpp"

namespace
{
std::size_t get_prev_utf8_char_index(std::string_view text, std::size_t index)
{
    if (index == 0)
    {
        return index;
    }
    std::size_t result{std::min(index, text.size()) - 1};
    while (result > 0 && (static_cast<unsigned char>(text[result]) & 0xC0) == 0x80)
    {
        --result;
    }
    return result;
}

std::size_t get_next_utf8_char_index(std::string_view text, std::size_t index)
{
    if (index >= text.size())
    {
        return text.size();
    }

    const unsigned char c{static_cast<unsigned char>(text[index])};
    std::size_t length{1};
    if ((c & 0xE0) == 0xC0)
    {
        length = 2;
    }
    else if ((c & 0xF0) == 0xE0)
    {
        length = 3;
    }
    else if ((c & 0xF8) == 0xF0)
    {
        length = 4;
    }
    return std::min(index + length, text.size());
}
} // namespace

namespace nori::editor
{
namespace utils
{
const char* to_string(nori::resource::property_type type)
{
    if (type == nori::resource::property_type::binary)
    {
        return "바이너리";
    }
    const auto index{static_cast<std::size_t>(type)};
    if (index >= data_type_names.size())
    {
        return "알 수 없음";
    }
    return data_type_names.at(index);
}

std::string to_lower(std::string text)
{
    std::ranges::transform(
        text,
        text.begin(),
        [](char c)
    {
        if ('A' <= c && c <= 'Z')
        {
            return static_cast<char>(c + ('a' - 'A'));
        }
        return c;
    }
    );
    return text;
}

std::vector<std::string> split_file_name(std::string_view text, float width)
{
    constexpr std::string_view ellipsis{"..."};
    std::vector<std::string> lines;
    std::size_t line_start{0};
    while (line_start < text.size() && lines.size() < file_name_line_max)
    {
        std::size_t line_end{line_start};
        while (line_end < text.size())
        {
            const std::size_t next{get_next_utf8_char_index(text, line_end)};
            if (ImGui::CalcTextSize(text.data() + line_start, text.data() + next).x > width)
            {
                break;
            }
            line_end = next;
        }

        if (line_end == line_start)
        {
            line_end = get_next_utf8_char_index(text, line_start);
        }
        if (line_end < text.size() && lines.size() + 1 == file_name_line_max)
        {
            const float ellipsis_width{ImGui::CalcTextSize(ellipsis.data()).x};
            while (
                line_end > line_start &&
                ImGui::CalcTextSize(text.data() + line_start, text.data() + line_end).x + ellipsis_width > width
            )
            {
                line_end = get_prev_utf8_char_index(text, line_end);
            }

            std::string line;
            if (line_end == line_start)
            {
                line = ellipsis;
            }
            else
            {
                line = text.substr(line_start, line_end - line_start);
                line += ellipsis;
            }
            lines.push_back(std::move(line));
            return lines;
        }

        lines.emplace_back(text.substr(line_start, line_end - line_start));
        line_start = line_end;
    }
    return lines;
}

std::vector<std::filesystem::path> get_drives()
{
    DWORD buffer_size{::GetLogicalDriveStringsW(0, nullptr)};
    std::wstring buffer(buffer_size, L'\0');
    if (::GetLogicalDriveStringsW(buffer_size, buffer.data()) == 0)
    {
        return {};
    }

    std::vector<std::filesystem::path> drives;
    std::wstring_view remaining{buffer.data(), buffer.size()};
    while (!remaining.empty() && remaining.front() != L'\0')
    {
        const std::size_t end{remaining.find(L'\0')};
        drives.emplace_back(remaining.substr(0, end));
        remaining.remove_prefix(end + 1);
    }
    return drives;
}

bool has_sub_directory(std::filesystem::path path)
{
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator{path, std::filesystem::directory_options::skip_permission_denied, ec})
    {
        if (entry.is_directory(ec))
        {
            return true;
        }
    }
    return false;
}

bool is_nori_file(std::filesystem::path path)
{
    const nori::resource::path resource_path{path};
    return resource_path.has_file_path() && nori::resource::subpath_iterator{resource_path} == nori::resource::subpath_iterator{};
}
} // namespace utils
} // namespace nori::editor
