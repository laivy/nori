#pragma once
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include <nori/asset/error_code.hpp>
#include <nori/resource/binary.hpp>
#include <nori/resource/image.hpp>

namespace nori::asset
{
enum class texture_compression_quality : std::uint8_t
{
    quick,
    normal
};

struct cooked_texture
{
    std::string name;
    resource::image image;
};

struct cooked_binary
{
    std::string name;
    resource::binary binary;
};

struct cooked_model
{
    resource::binary manifest;
    std::vector<cooked_binary> meshes;
    std::vector<cooked_texture> textures;
    std::vector<cooked_binary> animations;
};

struct animation_source
{
    std::string name;
    std::filesystem::path path;
};

result<resource::image> cook_image(const std::filesystem::path& source_path, resource::image_format format, texture_compression_quality quality = texture_compression_quality::quick);
result<cooked_model> cook_model(const std::filesystem::path& source_path, texture_compression_quality quality = texture_compression_quality::quick, std::span<const animation_source> animations = {});
result<resource::binary> cook_shader(const std::filesystem::path& source_path);
result<resource::binary> cook_binary(const std::filesystem::path& source_path);
} // namespace nori::asset
