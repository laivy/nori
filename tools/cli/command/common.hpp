#pragma once
#include <cstddef>
#include <initializer_list>
#include <string_view>

#include <nlohmann/json.hpp>

#include <nori/resource.hpp>

#include "../protocol.hpp"

namespace nori::cli::command
{
[[nodiscard]] nlohmann::ordered_json to_json(resource::handle handle);

[[nodiscard]] error_code to_error_code(resource::error_code code) noexcept;

[[nodiscard]] bool has_valid_param_keys(const nlohmann::json& params, std::initializer_list<std::string_view> required, std::initializer_list<std::string_view> optional = {});
[[nodiscard]] result<std::size_t> parse_position(const nlohmann::json& value);
[[nodiscard]] result<void> prepare_edit(resource::handle target);

[[nodiscard]] result<resource::handle> create(const resource::path& target);
[[nodiscard]] result<resource::handle> create(resource::handle parent, const resource::path& target);
[[nodiscard]] bool is_root_path(const resource::path& target);

[[nodiscard]] result<void> save(resource::handle target, std::filesystem::path file_path);
} // namespace nori::cli::command
