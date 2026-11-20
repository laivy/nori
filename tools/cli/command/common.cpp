#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include <nori/resource.hpp>

#include "common.hpp"

namespace
{
std::string_view to_string(nori::resource::property_type type) noexcept
{
    switch (type)
    {
    case nori::resource::property_type::empty:
    {
        return "empty";
    }
    case nori::resource::property_type::boolean:
    {
        return "bool";
    }
    case nori::resource::property_type::int32:
    {
        return "int32";
    }
    case nori::resource::property_type::int64:
    {
        return "int64";
    }
    case nori::resource::property_type::float32:
    {
        return "float";
    }
    case nori::resource::property_type::string:
    {
        return "string";
    }
    case nori::resource::property_type::image:
    {
        return "image";
    }
    case nori::resource::property_type::binary:
    {
        return "binary";
    }
    default:
        std::unreachable();
    }
}

std::string_view to_string(nori::resource::image_format format) noexcept
{
    switch (format)
    {
    case nori::resource::image_format::rgba8_unorm:
    {
        return "rgba8";
    }
    case nori::resource::image_format::bc5_unorm:
    {
        return "bc5";
    }
    case nori::resource::image_format::bc7_unorm:
    {
        return "bc7";
    }
    case nori::resource::image_format::bc7_unorm_srgb:
    {
        return "bc7-srgb";
    }
    default:
        return "unknown";
    }
}

double to_json_number(float value)
{
    std::array<char, 64> buffer{};
    const auto [end, error]{std::to_chars(buffer.data(), buffer.data() + buffer.size(), value)};
    if (error != std::errc{})
    {
        return static_cast<double>(value);
    }

    double result{};
    const auto [parsed_end, parse_error]{std::from_chars(buffer.data(), end, result)};
    if (parse_error != std::errc{} || parsed_end != end)
    {
        return static_cast<double>(value);
    }
    return result;
}
} // namespace

namespace nori::cli::command
{
nlohmann::ordered_json to_json(resource::handle handle)
{
    const resource::path path{resource::path_of(handle)};
    if (path.has_file_path() && !path.has_subpath())
    {
        return nlohmann::ordered_json::object();
    }

    const resource::property_type type{resource::type_of(handle)};
    switch (type)
    {
    case resource::property_type::empty:
    {
        return nlohmann::ordered_json{{"type", to_string(type)}};
    }
    case resource::property_type::boolean:
    {
        return nlohmann::ordered_json{{"type", to_string(type)}, {"value", resource::get<bool>(handle)}};
    }
    case resource::property_type::int32:
    {
        return nlohmann::ordered_json{{"type", to_string(type)}, {"value", resource::get<std::int32_t>(handle)}};
    }
    case resource::property_type::int64:
    {
        return nlohmann::ordered_json{{"type", to_string(type)}, {"value", resource::get<std::int64_t>(handle)}};
    }
    case resource::property_type::float32:
    {
        return nlohmann::ordered_json{{"type", to_string(type)}, {"value", to_json_number(resource::get<float>(handle))}};
    }
    case resource::property_type::string:
    {
        return nlohmann::ordered_json{{"type", to_string(type)}, {"value", resource::get<std::string>(handle)}};
    }
    case resource::property_type::image:
    {
        const auto image{resource::get<resource::image_view>(handle)};
        return nlohmann::ordered_json{
            {"type", to_string(type)},
            {"format", to_string(image.format)},
            {"width", image.width},
            {"height", image.height},
            {"mip_levels", image.mip_levels}
        };
    }
    case resource::property_type::binary:
    {
        const auto binary{resource::get<resource::binary_view>(handle)};
        return nlohmann::ordered_json{{"type", to_string(type)}, {"size", binary.data.size()}};
    }
    default:
        std::unreachable();
    }
}

error_code to_error_code(resource::error_code code) noexcept
{
    switch (code)
    {
    case resource::error_code::not_found:
    {
        return error_code::not_found;
    }
    case resource::error_code::invalid_path:
    {
        return error_code::invalid_argument;
    }
    case resource::error_code::type_mismatch:
    {
        return error_code::type_mismatch;
    }
    case resource::error_code::io_failure:
    {
        return error_code::io_error;
    }
    case resource::error_code::invalid_format:
    {
        return error_code::invalid_format;
    }
    case resource::error_code::name_conflict:
    {
        return error_code::already_exists;
    }
    case resource::error_code::invalid_relationship:
    {
        return error_code::invalid_argument;
    }
    default:
        std::unreachable();
    }
}

bool has_valid_param_keys(const nlohmann::json& params, std::initializer_list<std::string_view> required, std::initializer_list<std::string_view> optional)
{
    for (const std::string_view name : required)
    {
        if (!params.contains(name))
        {
            return false;
        }
    }

    for (const auto& [name, _] : params.items())
    {
        if (!std::ranges::contains(required, name) && !std::ranges::contains(optional, name))
        {
            return false;
        }
    }
    return true;
}

result<std::size_t> parse_position(const nlohmann::json& value)
{
    if (value.is_number_unsigned())
    {
        const auto parsed{value.get<std::uint64_t>()};
        if (std::in_range<std::size_t>(parsed))
        {
            return static_cast<std::size_t>(parsed);
        }
    }
    else if (value.is_number_integer())
    {
        const auto parsed{value.get<std::int64_t>()};
        if (std::in_range<std::size_t>(parsed))
        {
            return static_cast<std::size_t>(parsed);
        }
    }
    return std::unexpected{error_code::invalid_argument};
}

result<void> prepare_edit(resource::handle target)
{
    if (const auto res{resource::load(resource::root_of(target))}; !res)
    {
        return std::unexpected{to_error_code(res.error())};
    }
    return {};
}

result<resource::handle> create(const resource::path& target)
{
    if (!target.has_file_path())
    {
        return std::unexpected{error_code::invalid_argument};
    }

    if (const auto found{resource::try_get(target)})
    {
        if (const auto res{prepare_edit(*found)}; !res)
        {
            return std::unexpected{res.error()};
        }
        return *found;
    }
    else if (found.error() != resource::error_code::not_found)
    {
        return std::unexpected{to_error_code(found.error())};
    }

    resource::handle document{};
    const resource::path file_path{target.file_path()};
    if (const auto res{resource::try_get(file_path)})
    {
        document = *res;
    }
    else if (res.error() == resource::error_code::not_found)
    {
        const resource::handle created{resource::create_root()};
        document = created;
    }
    else
    {
        return std::unexpected{to_error_code(res.error())};
    }

    if (const auto res{prepare_edit(document)}; !res)
    {
        return std::unexpected{res.error()};
    }
    resource::handle current{document};
    for (const std::string_view name : resource::subpath_iterator{target})
    {
        if (const auto res{resource::try_get(current, name)})
        {
            current = *res;
            continue;
        }
        else if (res.error() != resource::error_code::not_found)
        {
            return std::unexpected{to_error_code(res.error())};
        }

        const resource::handle created{resource::create(current, name, resource::empty)};
        if (!created)
        {
            return std::unexpected{error_code::already_exists};
        }
        current = created;
    }
    return current;
}

result<resource::handle> create(resource::handle parent, const resource::path& target)
{
    if (target.has_file_path())
    {
        return std::unexpected{error_code::invalid_argument};
    }

    if (const auto res{prepare_edit(parent)}; !res)
    {
        return std::unexpected{res.error()};
    }
    resource::handle current{parent};
    for (const std::string_view name : resource::subpath_iterator{target})
    {
        if (const auto res{resource::try_get(current, name)})
        {
            current = *res;
            continue;
        }
        else if (res.error() != resource::error_code::not_found)
        {
            return std::unexpected{to_error_code(res.error())};
        }

        const resource::handle created{resource::create(current, name, resource::empty)};
        if (!created)
        {
            return std::unexpected{error_code::already_exists};
        }
        current = created;
    }
    return current;
}

bool is_root_path(const resource::path& target)
{
    if (!target.has_file_path())
    {
        return false;
    }
    std::size_t depth{0};
    for (const std::string_view component : resource::subpath_iterator{target})
    {
        if (component == ".")
        {
            continue;
        }
        if (component == "..")
        {
            if (depth == 0)
            {
                return false;
            }
            --depth;
            continue;
        }
        ++depth;
    }
    return depth == 0;
}

result<void> save(resource::handle target, std::filesystem::path file_path)
{
    const resource::handle root{resource::root_of(target)};
    const resource::path current_path{resource::path_of(root)};
    if (!current_path.empty())
    {
        if (const auto res{resource::save(root)}; !res)
        {
            return std::unexpected{to_error_code(res.error())};
        }
        return {};
    }

    if (file_path.has_parent_path())
    {
        std::error_code error;
        std::filesystem::create_directories(file_path.parent_path(), error);
        if (error)
        {
            return std::unexpected{error_code::io_error};
        }
    }
    if (const auto res{resource::save_as(root, file_path)}; !res)
    {
        return std::unexpected{to_error_code(res.error())};
    }
    return {};
}
} // namespace nori::cli::command
