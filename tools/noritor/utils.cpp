#include "pch.hpp"
#include "utils.hpp"

namespace
{
class com_initializer
{
public:
    com_initializer()
    {
        result_ = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    }

    ~com_initializer()
    {
        if (SUCCEEDED(result_))
        {
            ::CoUninitialize();
        }
    }

    bool is_available() const
    {
        return SUCCEEDED(result_) || result_ == RPC_E_CHANGED_MODE;
    }

private:
    HRESULT result_{E_FAIL};
};
} // namespace

namespace utils
{
const char* to_string(nori::resource::type type)
{
    const auto index{static_cast<std::size_t>(type)};
    if (index >= data_type_names.size())
    {
        return "알 수 없음";
    }
    return data_type_names.at(index);
}

std::u8string to_lower(std::u8string text)
{
    std::ranges::transform(
        text,
        text.begin(),
        [](char8_t c)
        {
            if (u8'A' <= c && c <= u8'Z')
            {
                return static_cast<char8_t>(c + (u8'a' - u8'A'));
            }
            return c;
        }
    );
    return text;
}

std::string to_utf8(const std::filesystem::path& path)
{
    const std::u8string text{path.u8string()};
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}

std::optional<std::wstring> to_wide(std::string_view text)
{
    if (text.empty())
    {
        return std::wstring{};
    }

    const int size{
        ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0)
    };
    if (size == 0)
    {
        return std::nullopt;
    }

    std::wstring result(static_cast<std::size_t>(size), L'\0');
    if (::MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), size
        ) == 0)
    {
        return std::nullopt;
    }
    return result;
}

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

void draw_folder_icon(ImDrawList* draw_list, const ImVec2& min, const ImVec2& size)
{
    const ImVec2 tab_min{min + ImVec2{size.x * 0.08f, size.y * 0.16f}};
    const ImVec2 tab_max{min + ImVec2{size.x * 0.46f, size.y * 0.38f}};
    const ImVec2 body_min{min + ImVec2{size.x * 0.05f, size.y * 0.30f}};
    const ImVec2 body_max{min + ImVec2{size.x * 0.95f, size.y * 0.84f}};
    draw_list->AddRectFilled(tab_min, tab_max, IM_COL32(244, 196, 93, 255), 2.0f, ImDrawFlags_RoundCornersTop);
    draw_list->AddRectFilled(body_min, body_max, IM_COL32(224, 171, 63, 255), 2.0f);
}

void draw_file_icon(ImDrawList* draw_list, const ImVec2& min, const ImVec2& size)
{
    const ImVec2 page_min{min + ImVec2{size.x * 0.21f, size.y * 0.10f}};
    const ImVec2 page_max{min + ImVec2{size.x * 0.79f, size.y * 0.90f}};
    const ImVec2 fold{size.x * 0.15f, size.y * 0.15f};
    const ImU32 paper_color{IM_COL32(238, 240, 242, 255)};
    const ImU32 edge_color{IM_COL32(129, 136, 145, 255)};
    const ImU32 line_color{IM_COL32(154, 160, 168, 255)};
    const ImVec2 points[]{
        page_min,
        {page_max.x - fold.x, page_min.y},
        {page_max.x, page_min.y + fold.y},
        page_max,
        {page_min.x, page_max.y}
    };
    draw_list->AddConvexPolyFilled(points, std::size(points), paper_color);
    for (std::size_t index{0}; index < std::size(points); ++index)
    {
        draw_list->AddLine(points[index], points[(index + 1) % std::size(points)], edge_color, 1.0f);
    }
    draw_list->AddLine({page_max.x - fold.x, page_min.y}, {page_max.x - fold.x, page_min.y + fold.y}, edge_color, 1.0f);
    draw_list->AddLine({page_max.x - fold.x, page_min.y + fold.y}, {page_max.x, page_min.y + fold.y}, edge_color, 1.0f);

    draw_list->AddLine(
        min + ImVec2{size.x * 0.30f, size.y * 0.40f}, min + ImVec2{size.x * 0.66f, size.y * 0.40f}, line_color, 1.0f
    );
    draw_list->AddLine(
        min + ImVec2{size.x * 0.30f, size.y * 0.52f}, min + ImVec2{size.x * 0.70f, size.y * 0.52f}, line_color, 1.0f
    );
    draw_list->AddLine(
        min + ImVec2{size.x * 0.30f, size.y * 0.64f}, min + ImVec2{size.x * 0.62f, size.y * 0.64f}, line_color, 1.0f
    );
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

bool has_sub_directory(const std::filesystem::path& path)
{
    std::error_code ec;
    for (
        const auto& entry :
        std::filesystem::directory_iterator{path, std::filesystem::directory_options::skip_permission_denied, ec}
    )
    {
        if (entry.is_directory(ec))
        {
            return true;
        }
    }
    return false;
}

bool is_nori_file(const std::filesystem::path& path)
{
    return to_lower(path.extension().u8string()) ==
           to_lower(std::filesystem::path{nori::resource::extension}.u8string());
}

bool is_png_file(const std::filesystem::path& path)
{
    return to_lower(path.extension().u8string()) == std::u8string{u8".png"};
}

std::optional<nori::resource::image_asset> load_png_image(const std::filesystem::path& path)
{
    if (!is_png_file(path))
    {
        return std::nullopt;
    }

    const std::optional<std::wstring> wide_path{to_wide(to_utf8(path))};
    if (!wide_path)
    {
        return std::nullopt;
    }

    const com_initializer com;
    if (!com.is_available())
    {
        return std::nullopt;
    }

    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    if (FAILED(
            ::CoCreateInstance(
                CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.GetAddressOf())
            )
        ))
    {
        return std::nullopt;
    }

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromFilename(
            wide_path->c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf()
        )))
    {
        return std::nullopt;
    }

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, frame.GetAddressOf())))
    {
        return std::nullopt;
    }

    UINT width{};
    UINT height{};
    if (FAILED(frame->GetSize(&width, &height)) || width == 0 || height == 0)
    {
        return std::nullopt;
    }

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    if (FAILED(factory->CreateFormatConverter(converter.GetAddressOf())))
    {
        return std::nullopt;
    }
    if (FAILED(converter->Initialize(
            frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom
        )))
    {
        return std::nullopt;
    }

    constexpr UINT bytes_per_pixel{4};
    if (width > std::numeric_limits<UINT>::max() / bytes_per_pixel)
    {
        return std::nullopt;
    }

    const UINT stride{width * bytes_per_pixel};
    const std::uint64_t data_size{static_cast<std::uint64_t>(stride) * height};
    if (data_size > static_cast<std::uint64_t>(std::numeric_limits<UINT>::max()) ||
        data_size > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
    {
        return std::nullopt;
    }

    nori::resource::image_asset asset{
        .img = {.format = nori::resource::image_format::r8g8b8a8, .width = width, .height = height},
        .data = std::vector<std::byte>(static_cast<std::size_t>(data_size)),
    };
    if (FAILED(converter->CopyPixels(
            nullptr, stride, static_cast<UINT>(asset.data.size()), reinterpret_cast<BYTE*>(asset.data.data())
        )))
    {
        return std::nullopt;
    }

    return asset;
}
} // namespace utils
