#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <nori/core/endian.hpp>
#include <nori/resource/operations.hpp>

#include "model.hpp"

namespace nori::resource
{
namespace
{
constexpr std::uint32_t missing_index{std::numeric_limits<std::uint32_t>::max()};
constexpr std::uint32_t format_version{1};
constexpr std::size_t transform_size{10 * sizeof(float)}; // struct nori::resource::model::transform

class model_reader
{
public:
    explicit model_reader(std::span<const std::byte> data) :
        data_{data},
        is_valid_{true}
    {
    }

    template <typename T>
    T read()
    {
        T value{};
        const std::span<const std::byte> bytes{take(sizeof(T))};
        if (!bytes.empty())
        {
            std::memcpy(&value, bytes.data(), bytes.size());
        }
        return nori::core::to_endian(value, std::endian::little);
    }

    bool read_tag(std::string_view tag)
    {
        return std::ranges::equal(take(tag.size()), std::as_bytes(std::span{tag}));
    }

    model_reader read_section(std::string_view tag)
    {
        if (!read_tag(tag))
        {
            is_valid_ = false;
        }
        return model_reader{take(read<std::uint64_t>())};
    }

    std::string read_string()
    {
        const std::span<const std::byte> bytes{take(read<std::uint32_t>())};
        if (bytes.empty())
        {
            return {};
        }
        return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
    }

    template <typename F>
    auto read_array(std::uint64_t count, std::size_t element_size, F read_item)
    {
        std::vector<std::invoke_result_t<F, model_reader&>> values;
        if (count > data_.size() / element_size)
        {
            is_valid_ = false;
        }
        if (is_valid_)
        {
            values.reserve(static_cast<std::size_t>(count));
            for (std::uint64_t i{}; i < count && is_valid_; ++i)
            {
                values.push_back(read_item(*this));
            }
        }
        return values;
    }

    bool finished() const
    {
        return is_valid_ && data_.empty();
    }

private:
    std::span<const std::byte> take(std::uint64_t size)
    {
        if (!is_valid_ || size > data_.size())
        {
            is_valid_ = false;
            return {};
        }
        const std::size_t count{static_cast<std::size_t>(size)};
        const std::span<const std::byte> bytes{data_.first(count)};
        data_ = data_.subspan(count);
        return bytes;
    }

    std::span<const std::byte> data_;
    bool is_valid_;
};

std::uint32_t read_index(model_reader& reader)
{
    return reader.read<std::uint32_t>();
}

std::optional<std::uint32_t> read_optional_index(model_reader& reader)
{
    const std::uint32_t index{read_index(reader)};
    if (index == missing_index)
    {
        return std::nullopt;
    }
    return index;
}

nori::core::vec3 read_vec3(model_reader& reader)
{
    return {reader.read<float>(), reader.read<float>(), reader.read<float>()};
}

nori::core::vec4 read_vec4(model_reader& reader)
{
    return {reader.read<float>(), reader.read<float>(), reader.read<float>(), reader.read<float>()};
}

model::transform read_transform(model_reader& reader)
{
    return {read_vec3(reader), read_vec4(reader), read_vec3(reader)};
}

model::node read_node(model_reader& reader)
{
    model::node value{
        .name = reader.read_string(),
        .parent_index = read_optional_index(reader),
        .mesh_index = read_optional_index(reader),
        .local_transform = read_transform(reader),
        .material_indices = {}
    };
    value.material_indices = reader.read_array(read_index(reader), sizeof(std::uint32_t), read_index);
    return value;
}

model::material read_material(model_reader& reader)
{
    return {
        .name = reader.read_string(),
        .base_color = read_vec4(reader),
        .roughness = reader.read<float>(),
        .metalness = reader.read<float>(),
        .emission = read_vec3(reader),
        .opacity = reader.read<float>(),
        .base_color_texture_index = read_optional_index(reader),
        .normal_texture_index = read_optional_index(reader),
        .roughness_texture_index = read_optional_index(reader),
        .metalness_texture_index = read_optional_index(reader),
        .emission_texture_index = read_optional_index(reader),
        .opacity_texture_index = read_optional_index(reader)
    };
}

model::vertex read_vertex(model_reader& reader)
{
    return {
        .position = read_vec3(reader),
        .normal = read_vec3(reader),
        .tangent = read_vec3(reader),
        .tangent_sign = reader.read<float>(),
        .uv = {reader.read<float>(), reader.read<float>()},
        .joint_node_indices = {read_index(reader), read_index(reader), read_index(reader), read_index(reader)},
        .joint_weights = {reader.read<float>(), reader.read<float>(), reader.read<float>(), reader.read<float>()}
    };
}

model::joint read_joint(model_reader& reader)
{
    const std::uint32_t node_index{read_index(reader)};
    return {
        .node_index = node_index,
        .inverse_bind_matrix = {
            reader.read<float>(), reader.read<float>(), reader.read<float>(), 0.0f,
            reader.read<float>(), reader.read<float>(), reader.read<float>(), 0.0f,
            reader.read<float>(), reader.read<float>(), reader.read<float>(), 0.0f,
            reader.read<float>(), reader.read<float>(), reader.read<float>(), 1.0f
        }
    };
}

result<model::mesh> read_mesh(const binary& bytes, std::size_t node_count)
{
    model_reader reader{bytes.data};
    if (!reader.read_tag("NMSH") || reader.read<std::uint32_t>() != format_version)
    {
        return std::unexpected{error_code::invalid_format};
    }
    std::string name{reader.read_string()};
    const std::uint32_t vertex_count{read_index(reader)};
    const std::uint32_t index_count{read_index(reader)};
    const std::uint32_t triangle_count{read_index(reader)};
    const std::uint32_t joint_count{read_index(reader)};
    if (std::uint64_t{triangle_count} * 3 != index_count)
    {
        return std::unexpected{error_code::invalid_format};
    }
    model::mesh value{
        .name = std::move(name),
        .vertices = reader.read_array(vertex_count, 16 * sizeof(float) + 4 * sizeof(std::uint32_t), read_vertex),
        .indices = reader.read_array(index_count, sizeof(std::uint32_t), read_index),
        .triangle_material_slots = reader.read_array(triangle_count, sizeof(std::uint32_t), read_index),
        .joints = reader.read_array(joint_count, sizeof(std::uint32_t) + 12 * sizeof(float), read_joint)
    };
    const auto invalid_joint = [node_count](std::uint32_t index)
    {
        return index >= node_count;
    };
    if (!reader.finished() ||
        std::ranges::any_of(value.indices, [vertex_count](std::uint32_t index)
    {
        return index >= vertex_count;
    }) ||
        std::ranges::any_of(value.joints, invalid_joint, &model::joint::node_index))
    {
        return std::unexpected{error_code::invalid_format};
    }
    for (const auto& vertex : value.vertices)
    {
        if (std::ranges::any_of(vertex.joint_node_indices, [&](std::uint32_t index)
        {
            return index != missing_index && invalid_joint(index);
        }))
        {
            return std::unexpected{error_code::invalid_format};
        }
    }
    return value;
}

result<model::animation> read_animation(const binary& bytes, std::size_t node_count)
{
    model_reader reader{bytes.data};
    if (!reader.read_tag("NANI") || reader.read<std::uint32_t>() != format_version)
    {
        return std::unexpected{error_code::invalid_format};
    }
    std::string name{reader.read_string()};
    const float rate{reader.read<float>()};
    const std::uint32_t frame_count{read_index(reader)};
    const std::uint32_t stored_node_count{read_index(reader)};
    if (!std::isfinite(rate) || rate <= 0.0f || frame_count == 0 || stored_node_count != node_count)
    {
        return std::unexpected{error_code::invalid_format};
    }
    std::vector<model::transform> samples{reader.read_array(std::uint64_t{frame_count} * stored_node_count, transform_size, read_transform)};
    if (!reader.finished())
    {
        return std::unexpected{error_code::invalid_format};
    }
    return model::animation{std::move(name), rate, frame_count, std::move(samples)};
}

std::string read_name(model_reader& reader)
{
    return reader.read_string();
}

std::string read_texture_name(model_reader& reader)
{
    std::string name{reader.read_string()};
    reader.read_string();
    return name;
}

bool has_valid_references(const model& value, std::size_t mesh_count, std::size_t texture_count)
{
    for (const auto& node : value.nodes)
    {
        if ((node.parent_index && *node.parent_index >= value.nodes.size()) ||
            (node.mesh_index && *node.mesh_index >= mesh_count) ||
            std::ranges::any_of(node.material_indices, [&](std::uint32_t index)
        {
            return index >= value.materials.size();
        }))
        {
            return false;
        }
    }
    for (const auto& material : value.materials)
    {
        for (const auto index : {material.base_color_texture_index, material.normal_texture_index, material.roughness_texture_index, material.metalness_texture_index, material.emission_texture_index, material.opacity_texture_index})
        {
            if (index && *index >= texture_count)
            {
                return false;
            }
        }
    }
    return true;
}

template <typename T>
result<void> read_children(nori::resource::handle parent, std::string_view category, const std::vector<std::string>& names, std::vector<T>& values, std::size_t node_count, result<T> (*read_child)(const binary&, std::size_t))
{
    values.reserve(names.size());
    for (const auto& name : names)
    {
        const auto bytes{nori::resource::try_get<binary>(parent, std::string{category} + name)};
        if (!bytes)
        {
            return std::unexpected{bytes.error()};
        }
        auto value{read_child(*bytes, node_count)};
        if (!value)
        {
            return std::unexpected{value.error()};
        }
        values.push_back(std::move(*value));
    }
    return {};
}
} // namespace

result<model> read_model(handle target)
{
    if (target == root_of(target))
    {
        return std::unexpected{error_code::type_mismatch};
    }

    const auto bytes{try_get<binary>(target)};
    if (!bytes)
    {
        return std::unexpected{bytes.error()};
    }

    model_reader manifest{bytes->data};
    if (!manifest.read_tag("NMDL"))
    {
        return std::unexpected{error_code::type_mismatch};
    }
    if (manifest.read<std::uint32_t>() != format_version || read_index(manifest) != 5)
    {
        return std::unexpected{error_code::invalid_format};
    }

    model_reader nodes{manifest.read_section("NODE")};
    model_reader materials{manifest.read_section("MATL")};
    model_reader textures{manifest.read_section("TEXR")};
    model_reader meshes{manifest.read_section("MESH")};
    model_reader animations{manifest.read_section("ANIM")};

    model value;
    value.nodes = nodes.read_array(read_index(nodes), 4 * sizeof(std::uint32_t) + transform_size, read_node);
    value.materials = materials.read_array(read_index(materials), 10 * sizeof(float) + 7 * sizeof(std::uint32_t), read_material);
    const std::vector<std::string> texture_names{textures.read_array(read_index(textures), 2 * sizeof(std::uint32_t), read_texture_name)};
    const std::vector<std::string> mesh_names{meshes.read_array(read_index(meshes), sizeof(std::uint32_t), read_name)};
    const std::vector<std::string> animation_names{animations.read_array(read_index(animations), sizeof(std::uint32_t), read_name)};
    if (!manifest.finished() || !nodes.finished() || !materials.finished() || !textures.finished() || !meshes.finished() || !animations.finished() || !has_valid_references(value, mesh_names.size(), texture_names.size()))
    {
        return std::unexpected{error_code::invalid_format};
    }

    value.textures.reserve(texture_names.size());
    for (const auto& name : texture_names)
    {
        const auto texture{try_get(target, "texture/" + name)};
        if (!texture)
        {
            return std::unexpected{texture.error()};
        }
        if (type_of(*texture) != property_type::image)
        {
            return std::unexpected{error_code::type_mismatch};
        }
        value.textures.push_back(*texture);
    }
    if (const auto res{read_children(target, "mesh/", mesh_names, value.meshes, value.nodes.size(), read_mesh)}; !res)
    {
        return std::unexpected{res.error()};
    }
    if (const auto res{read_children(target, "animation/", animation_names, value.animations, value.nodes.size(), read_animation)}; !res)
    {
        return std::unexpected{res.error()};
    }
    return value;
}
} // namespace nori::resource
