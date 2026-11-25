#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

#include <nori/asset.hpp>
#include <nori/resource.hpp>

#include "../protocol.hpp"

namespace nori::cli::command
{
enum class import_type
{
    binary,
    image,
    model,
    shader
};

struct diff_args
{
    resource::path before;
    resource::path after;
};

struct exists_args
{
    resource::path path;
};

struct get_args
{
    resource::path path;
};

struct tree_args
{
    resource::path path;
    std::optional<std::size_t> depth;
};

struct import_args
{
    import_type type;
    std::filesystem::path source;
    resource::path destination;
    std::optional<resource::image_format> format;
    asset::texture_compression_quality quality;
    std::vector<asset::animation_source> animations;
};

struct set_args
{
    using value_type = std::variant<resource::empty_t, bool, std::int32_t, std::int64_t, float, std::string>;

    resource::path path;
    resource::property_type type;
    value_type value;
};

struct copy_args
{
    resource::path source;
    resource::path destination;
};

struct move_args
{
    resource::path source;
    resource::path destination;
};

struct rename_args
{
    resource::path path;
    std::string name;
};

struct reorder_args
{
    resource::path path;
    std::size_t position;
};

struct del_args
{
    resource::path path;
};

[[nodiscard]] result<nlohmann::ordered_json> dispatch(std::string_view method, const nlohmann::json& params);

[[nodiscard]] std::optional<diff_args> parse_diff_args(const nlohmann::json& params);
[[nodiscard]] result<nlohmann::ordered_json> diff(const diff_args& args);
[[nodiscard]] std::optional<exists_args> parse_exists_args(const nlohmann::json& params);
[[nodiscard]] result<nlohmann::ordered_json> exists(const exists_args& args);
[[nodiscard]] std::optional<get_args> parse_get_args(const nlohmann::json& params);
[[nodiscard]] result<nlohmann::ordered_json> get(const get_args& args);
[[nodiscard]] std::optional<tree_args> parse_tree_args(const nlohmann::json& params);
[[nodiscard]] result<nlohmann::ordered_json> tree(const tree_args& args);

[[nodiscard]] std::optional<import_args> parse_import_args(const nlohmann::json& params);
[[nodiscard]] result<nlohmann::ordered_json> import(const import_args& args);
[[nodiscard]] std::optional<set_args> parse_set_args(const nlohmann::json& params);
[[nodiscard]] result<nlohmann::ordered_json> set(const set_args& args);

[[nodiscard]] std::optional<copy_args> parse_copy_args(const nlohmann::json& params);
[[nodiscard]] result<nlohmann::ordered_json> copy(const copy_args& args);
[[nodiscard]] std::optional<move_args> parse_move_args(const nlohmann::json& params);
[[nodiscard]] result<nlohmann::ordered_json> move(const move_args& args);
[[nodiscard]] std::optional<rename_args> parse_rename_args(const nlohmann::json& params);
[[nodiscard]] result<nlohmann::ordered_json> rename(const rename_args& args);
[[nodiscard]] std::optional<reorder_args> parse_reorder_args(const nlohmann::json& params);
[[nodiscard]] result<nlohmann::ordered_json> reorder(const reorder_args& args);

[[nodiscard]] std::optional<del_args> parse_del_args(const nlohmann::json& params);
[[nodiscard]] result<nlohmann::ordered_json> del(const del_args& args);
} // namespace nori::cli::command
