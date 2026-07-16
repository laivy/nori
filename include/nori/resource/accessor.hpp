#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <nori/resource/handle.hpp>
#include <nori/resource/error_code.hpp>
#include "image.hpp"
#include "type.hpp"

namespace nori::resource
{
handle create(std::string_view name);
void remove(handle target);

result<void> set_parent(handle child, handle parent);
result<void> set_name(handle target, std::string_view name);
void set_value(handle target, std::int32_t value);
void set_value(handle target, std::int64_t value);
void set_value(handle target, float value);
void set_value(handle target, const char* value);
void set_value(handle target, const std::string& value);
void set_value(handle target, const image_asset& value);

handle get(std::string_view path);
handle get_parent(handle target);
std::vector<handle> get_children(handle target);

std::string get_name(handle target);
type get_type(handle target);
std::optional<type> get_type(std::string_view path);
std::optional<type> get_type(handle parent, std::string_view path);
std::int32_t get_int32(handle target);
std::optional<std::int32_t> get_int32(std::string_view path);
std::optional<std::int32_t> get_int32(handle parent, std::string_view path);
std::int64_t get_int64(handle target);
std::optional<std::int64_t> get_int64(std::string_view path);
std::optional<std::int64_t> get_int64(handle parent, std::string_view path);
float get_float(handle target);
std::optional<float> get_float(std::string_view path);
std::optional<float> get_float(handle parent, std::string_view path);
std::string get_string(handle target);
std::optional<std::string> get_string(std::string_view path);
std::optional<std::string> get_string(handle parent, std::string_view path);
image get_image(handle target);
std::optional<image> get_image(std::string_view path);
std::optional<image> get_image(handle parent, std::string_view path);
const image_asset& get_image_asset(handle target);
const image_asset* get_image_asset(std::string_view path);
const image_asset* get_image_asset(handle parent, std::string_view path);

result<void> serialize(const std::filesystem::path& path, handle target);
} // namespace nori::resource
