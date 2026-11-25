#pragma once
#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <nori/resource/value.hpp>

namespace nori::editor
{
namespace utils
{
constexpr std::size_t file_name_line_max{3};
constexpr std::array data_types{
    nori::resource::property_type::empty,
    nori::resource::property_type::boolean,
    nori::resource::property_type::int32,
    nori::resource::property_type::int64,
    nori::resource::property_type::float32,
    nori::resource::property_type::string,
    nori::resource::property_type::image
};
constexpr std::array data_type_names{"폴더", "불리언", "정수(32)", "정수(64)", "실수", "문자열", "이미지"};

const char* to_string(nori::resource::property_type type);
std::string to_lower(std::string text);
std::vector<std::string> split_file_name(std::string_view text, float width);
std::vector<std::filesystem::path> get_drives();
bool has_sub_directory(std::filesystem::path path);
bool is_nori_file(std::filesystem::path path);
} // namespace utils
} // namespace nori::editor
