#include <concepts>
#include <cstdio>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include <nlohmann/json.hpp>

#include <nori/asset.hpp>
#include <nori/core/encoding.hpp>
#include <nori/resource.hpp>

#include "command.hpp"
#include "common.hpp"

namespace
{
using cooked_asset = std::variant<nori::resource::image, nori::resource::binary, nori::asset::cooked_model>;

nori::cli::error_code to_error_code(nori::asset::error_code code) noexcept
{
    switch (code)
    {
    case nori::asset::error_code::io_failure:
    {
        return nori::cli::error_code::io_error;
    }
    case nori::asset::error_code::invalid_format:
    {
        return nori::cli::error_code::invalid_format;
    }
    case nori::asset::error_code::unsupported_format:
    {
        return nori::cli::error_code::unsupported_format;
    }
    case nori::asset::error_code::unsupported_platform:
    {
        return nori::cli::error_code::unsupported_platform;
    }
    default:
        std::unreachable();
    }
}

std::optional<nori::cli::command::import_type> parse_import_type(std::string_view value)
{
    if (value == "binary")
    {
        return nori::cli::command::import_type::binary;
    }
    if (value == "image")
    {
        return nori::cli::command::import_type::image;
    }
    if (value == "model")
    {
        return nori::cli::command::import_type::model;
    }
    if (value == "shader")
    {
        return nori::cli::command::import_type::shader;
    }
    return std::nullopt;
}

std::optional<nori::resource::image_format> parse_image_format(std::string_view value)
{
    if (value == "rgba8")
    {
        return nori::resource::image_format::rgba8_unorm;
    }
    if (value == "bc5")
    {
        return nori::resource::image_format::bc5_unorm;
    }
    if (value == "bc7")
    {
        return nori::resource::image_format::bc7_unorm;
    }
    if (value == "bc7-srgb")
    {
        return nori::resource::image_format::bc7_unorm_srgb;
    }
    return std::nullopt;
}

std::optional<nori::asset::texture_compression_quality> parse_quality(std::string_view value)
{
    if (value == "quick")
    {
        return nori::asset::texture_compression_quality::quick;
    }
    if (value == "normal")
    {
        return nori::asset::texture_compression_quality::normal;
    }
    return std::nullopt;
}

nori::cli::result<cooked_asset> cook(nori::cli::command::import_type type, const std::filesystem::path& source, std::optional<nori::resource::image_format> format, nori::asset::texture_compression_quality quality, std::span<const nori::asset::animation_source> animations)
{
    switch (type)
    {
    case nori::cli::command::import_type::binary:
    {
        auto value{nori::asset::cook_binary(source)};
        if (!value)
        {
            return std::unexpected{to_error_code(value.error().code)};
        }
        return cooked_asset{std::move(*value)};
    }
    case nori::cli::command::import_type::image:
    {
        auto value{nori::asset::cook_image(source, *format, quality)};
        if (!value)
        {
            return std::unexpected{to_error_code(value.error().code)};
        }
        return cooked_asset{std::move(*value)};
    }
    case nori::cli::command::import_type::model:
    {
        auto value{nori::asset::cook_model(source, quality, animations)};
        if (!value)
        {
            if (!value.error().diagnostics.empty())
            {
                std::fputs(value.error().diagnostics.c_str(), stderr);
                std::fputc('\n', stderr);
            }
            return std::unexpected{to_error_code(value.error().code)};
        }
        return cooked_asset{std::move(*value)};
    }
    case nori::cli::command::import_type::shader:
    {
        auto value{nori::asset::cook_shader(source)};
        if (!value)
        {
            return std::unexpected{to_error_code(value.error().code)};
        }
        return cooked_asset{std::move(*value)};
    }
    default:
        std::unreachable();
    }
}

nori::cli::result<void> store_binaries(nori::resource::handle parent, std::string_view category, std::span<const nori::asset::cooked_binary> values)
{
    for (const nori::asset::cooked_binary& value : values)
    {
        const auto target{nori::cli::command::create(parent, std::string{category} + "/" + value.name)};
        if (!target)
        {
            return std::unexpected{target.error()};
        }
        nori::resource::set(*target, value.binary);
    }
    return {};
}

nori::cli::result<void> store_model(nori::resource::handle target, const nori::asset::cooked_model& model)
{
    for (const char* category : {"mesh", "animation", "texture"})
    {
        if (const auto previous{nori::resource::try_get(target, category)}; previous)
        {
            nori::resource::remove(*previous);
        }
    }
    nori::resource::set(target, model.manifest);
    if (const nori::cli::result<void> res{store_binaries(target, "mesh", model.meshes)}; !res)
    {
        return res;
    }
    if (const nori::cli::result<void> res{store_binaries(target, "animation", model.animations)}; !res)
    {
        return res;
    }
    for (const nori::asset::cooked_texture& texture : model.textures)
    {
        const auto texture_target{nori::cli::command::create(target, std::string{"texture/"} + texture.name)};
        if (!texture_target)
        {
            return std::unexpected{texture_target.error()};
        }
        nori::resource::set(*texture_target, texture.image);
    }
    return {};
}
} // namespace

namespace nori::cli::command
{
std::optional<import_args> parse_import_args(const nlohmann::json& params)
{
    if (!has_valid_param_keys(params, {"type", "path", "source"}, {"format", "quality"}))
    {
        return std::nullopt;
    }
    if (!params.at("type").is_string() || !params.at("path").is_string())
    {
        return std::nullopt;
    }
    if ((params.contains("format") && !params.at("format").is_string()) || (params.contains("quality") && !params.at("quality").is_string()))
    {
        return std::nullopt;
    }

    const auto type{parse_import_type(params.at("type").get_ref<const std::string&>())};
    if (!type)
    {
        return std::nullopt;
    }

    std::filesystem::path source;
    std::vector<asset::animation_source> animations;
    const auto& raw_source{params.at("source")};
    if (*type == import_type::model)
    {
        if (!raw_source.is_object() || !has_valid_param_keys(raw_source, {"model"}, {"animation"}) || !raw_source.at("model").is_string() || raw_source.at("model").get_ref<const std::string&>().empty() || raw_source.at("model").get_ref<const std::string&>().contains('\0'))
        {
            return std::nullopt;
        }
        source = core::utf8_to_path(raw_source.at("model").get_ref<const std::string&>());
        if (raw_source.contains("animation"))
        {
            if (!raw_source.at("animation").is_object())
            {
                return std::nullopt;
            }
            for (const auto& [name, path] : raw_source.at("animation").items())
            {
                if (!resource::is_valid_property_name(name) || name.find('\\') != std::string::npos || !path.is_string() || path.get_ref<const std::string&>().empty() || path.get_ref<const std::string&>().contains('\0'))
                {
                    return std::nullopt;
                }
                animations.push_back({.name = name, .path = core::utf8_to_path(path.get_ref<const std::string&>())});
            }
        }
    }
    else
    {
        if (!raw_source.is_string() || raw_source.get_ref<const std::string&>().empty())
        {
            return std::nullopt;
        }
        source = core::utf8_to_path(raw_source.get_ref<const std::string&>());
    }
    if (params.contains("format") && *type != import_type::image)
    {
        return std::nullopt;
    }
    if (params.contains("quality") && *type != import_type::image && *type != import_type::model)
    {
        return std::nullopt;
    }

    std::optional<resource::image_format> format;
    if (*type == import_type::image)
    {
        const std::string_view value{params.contains("format") ? params.at("format").get_ref<const std::string&>() : std::string_view{"rgba8"}};
        format = parse_image_format(value);
        if (!format)
        {
            return std::nullopt;
        }
    }

    asset::texture_compression_quality quality{asset::texture_compression_quality::quick};
    if (*type == import_type::image || *type == import_type::model)
    {
        const std::string_view value{params.contains("quality") ? params.at("quality").get_ref<const std::string&>() : std::string_view{"quick"}};
        const auto parsed{parse_quality(value)};
        if (!parsed)
        {
            return std::nullopt;
        }
        quality = *parsed;
    }

    return import_args{
        *type,
        std::move(source),
        resource::path{params.at("path").get_ref<const std::string&>()},
        format,
        quality,
        std::move(animations)
    };
}

result<nlohmann::ordered_json> import(const import_args& args)
{
    const auto cooked{cook(args.type, args.source, args.format, args.quality, args.animations)};
    if (!cooked)
    {
        return std::unexpected{cooked.error()};
    }

    if (args.destination.has_file_path() && !args.destination.has_subpath())
    {
        return std::unexpected{error_code::invalid_argument};
    }
    if (is_root_path(args.destination))
    {
        return std::unexpected{error_code::invalid_argument};
    }
    const auto target{create(args.destination)};
    if (!target)
    {
        return std::unexpected{target.error()};
    }
    if (*target == resource::root_of(*target))
    {
        return std::unexpected{error_code::invalid_argument};
    }

    const result<void> stored{std::visit([target = *target](const auto& value) -> result<void>
    {
        using value_type = std::remove_cvref_t<decltype(value)>;
        if constexpr (std::same_as<value_type, asset::cooked_model>)
        {
            return store_model(target, value);
        }
        else
        {
            resource::set(target, value);
            return {};
        }
    }, *cooked)};
    if (!stored)
    {
        return std::unexpected{stored.error()};
    }
    if (const result<void> res{save(*target, args.destination.file_path())}; !res)
    {
        return std::unexpected{res.error()};
    }
    return nlohmann::ordered_json(nullptr);
}
} // namespace nori::cli::command
