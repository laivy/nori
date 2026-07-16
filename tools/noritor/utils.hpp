#pragma once

namespace utils
{
constexpr std::size_t file_name_line_max{3};
constexpr std::array data_types{
    nori::resource::type::none,
    nori::resource::type::folder,
    nori::resource::type::int32,
    nori::resource::type::int64,
    nori::resource::type::float32,
    nori::resource::type::string,
    nori::resource::type::image
};
constexpr std::array data_type_names{"없음", "폴더", "정수(32)", "정수(64)", "실수", "문자열", "이미지"};

const char* to_string(nori::resource::type type);
std::u8string to_lower(std::u8string text);
std::string to_utf8(const std::filesystem::path& path);
std::optional<std::wstring> to_wide(std::string_view text);
std::size_t get_prev_utf8_char_index(std::string_view text, std::size_t index);
std::size_t get_next_utf8_char_index(std::string_view text, std::size_t index);
std::vector<std::string> split_file_name(std::string_view text, float width);
void draw_folder_icon(ImDrawList* draw_list, const ImVec2& min, const ImVec2& size);
void draw_file_icon(ImDrawList* draw_list, const ImVec2& min, const ImVec2& size);
std::vector<std::filesystem::path> get_drives();
bool has_sub_directory(const std::filesystem::path& path);
bool is_nori_file(const std::filesystem::path& path);
bool is_png_file(const std::filesystem::path& path);
std::optional<nori::resource::image_asset> load_png_image(const std::filesystem::path& path);
} // namespace utils
