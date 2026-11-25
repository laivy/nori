#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <ufbx.h>

#include <nori/asset.hpp>
#include <nori/core/encoding.hpp>
#include <nori/core/endian.hpp>
#include <nori/core/vector.hpp>

#include "file_io.hpp"
#include "image_decode.hpp"

namespace
{
using scene_pointer = std::unique_ptr<ufbx_scene, decltype(&ufbx_free_scene)>;

constexpr std::uint32_t model_format_version{1};
constexpr std::uint32_t missing_index{std::numeric_limits<std::uint32_t>::max()};
constexpr std::size_t maximum_animation_frames{100000};

class binary_writer final
{
public:
    void write_bytes(const std::span<const std::byte> bytes)
    {
        data_.append_range(bytes);
    }

    void write_tag(const std::string_view tag)
    {
        write_bytes(std::as_bytes(std::span{tag.data(), tag.size()}));
    }

    void write_u32(const std::uint32_t value)
    {
        const std::uint32_t encoded{nori::core::to_endian(value, std::endian::little)};
        write_bytes(std::as_bytes(std::span{&encoded, 1}));
    }

    void write_u64(const std::uint64_t value)
    {
        const std::uint64_t encoded{nori::core::to_endian(value, std::endian::little)};
        write_bytes(std::as_bytes(std::span{&encoded, 1}));
    }

    void write_f32(const float value)
    {
        write_u32(std::bit_cast<std::uint32_t>(value));
    }

    void write_string(const ufbx_string value)
    {
        write_string(
            value.length > 0 ? std::string_view{value.data, value.length} : std::string_view{}
        );
    }

    void write_string(const std::string_view value)
    {
        write_u32(static_cast<std::uint32_t>(value.size()));
        write_bytes(std::as_bytes(std::span{value.data(), value.size()}));
    }

    std::size_t begin_section(const std::string_view tag)
    {
        write_tag(tag);
        const std::size_t size_position{data_.size()};
        write_u64(0);
        return size_position;
    }

    void end_section(const std::size_t size_position)
    {
        const std::uint64_t size{data_.size() - size_position - sizeof(std::uint64_t)};
        const std::uint64_t encoded{nori::core::to_endian(size, std::endian::little)};
        std::ranges::copy(
            std::as_bytes(std::span{&encoded, 1}), data_.begin() + size_position
        );
    }

    nori::resource::binary finish()
    {
        return nori::resource::binary{.data = std::move(data_)};
    }

private:
    std::vector<std::byte> data_;
};

struct cooked_vertex
{
    nori::core::vec3 position;
    nori::core::vec3 normal;
    nori::core::vec3 tangent;
    float tangent_sign;
    nori::core::vec2 uv;
    std::array<std::uint32_t, 4> joints;
    std::array<float, 4> weights;
};

struct texture_recipe
{
    std::size_t file_index;
    nori::resource::image_format format;
    std::string name;
    std::string source_name;
};

struct cooked_texture_set
{
    std::vector<nori::asset::cooked_texture> textures;
    std::vector<texture_recipe> recipes;
    std::vector<std::array<std::uint32_t, 6>> material_indices;
};

std::uint32_t to_u32(const std::size_t value)
{
    return static_cast<std::uint32_t>(value);
}

float tangent_sign(const ufbx_vec3 normal, const ufbx_vec3 tangent, const ufbx_vec3 bitangent)
{
    const ufbx_vec3 cross{
        normal.y * tangent.z - normal.z * tangent.y,
        normal.z * tangent.x - normal.x * tangent.z,
        normal.x * tangent.y - normal.y * tangent.x
    };
    const ufbx_real dot{cross.x * bitangent.x + cross.y * bitangent.y + cross.z * bitangent.z};
    return dot < 0.0 ? -1.0f : 1.0f;
}

std::string mesh_name(const std::size_t index)
{
    return std::to_string(index);
}

nori::core::vec2 to_vec2(ufbx_vec2 value)
{
    return {static_cast<float>(value.x), static_cast<float>(value.y)};
}

nori::core::vec3 to_vec3(ufbx_vec3 value)
{
    return {static_cast<float>(value.x), static_cast<float>(value.y), static_cast<float>(value.z)};
}

void write_vec2(binary_writer& writer, nori::core::vec2 value)
{
    writer.write_f32(value.x);
    writer.write_f32(value.y);
}

void write_vec3(binary_writer& writer, nori::core::vec3 value)
{
    writer.write_f32(value.x);
    writer.write_f32(value.y);
    writer.write_f32(value.z);
}

void write_quat(binary_writer& writer, const ufbx_quat value)
{
    writer.write_f32(static_cast<float>(value.x));
    writer.write_f32(static_cast<float>(value.y));
    writer.write_f32(static_cast<float>(value.z));
    writer.write_f32(static_cast<float>(value.w));
}

void write_transform(binary_writer& writer, const ufbx_transform value)
{
    write_vec3(writer, to_vec3(value.translation));
    write_quat(writer, value.rotation);
    write_vec3(writer, to_vec3(value.scale));
}

void write_matrix(binary_writer& writer, const ufbx_matrix& value)
{
    for (const ufbx_real component : value.v)
    {
        writer.write_f32(static_cast<float>(component));
    }
}

cooked_vertex make_vertex(const ufbx_mesh& mesh, const std::uint32_t index, const ufbx_skin_deformer* skin)
{
    const ufbx_vec3 position{ufbx_get_vertex_vec3(&mesh.vertex_position, index)};
    const ufbx_vec3 normal{ufbx_get_vertex_vec3(&mesh.vertex_normal, index)};
    const ufbx_vec2 uv{
        mesh.vertex_uv.exists ? ufbx_get_vertex_vec2(&mesh.vertex_uv, index) : ufbx_vec2{}
    };
    const ufbx_vec3 tangent{
        mesh.vertex_tangent.exists ? ufbx_get_vertex_vec3(&mesh.vertex_tangent, index) : ufbx_vec3{}
    };
    const ufbx_vec3 bitangent{
        mesh.vertex_bitangent.exists ? ufbx_get_vertex_vec3(&mesh.vertex_bitangent, index) : ufbx_vec3{}
    };
    cooked_vertex result{
        .position = to_vec3(position),
        .normal = to_vec3(normal),
        .tangent = to_vec3(tangent),
        .tangent_sign = mesh.vertex_tangent.exists && mesh.vertex_bitangent.exists ?
            tangent_sign(normal, tangent, bitangent) :
            1.0f,
        .uv = to_vec2(uv),
        .joints = {missing_index, missing_index, missing_index, missing_index}
    };
    if (skin == nullptr)
    {
        return result;
    }

    const std::uint32_t logical_vertex{mesh.vertex_indices.data[index]};
    const ufbx_skin_vertex weights{skin->vertices.data[logical_vertex]};
    const std::size_t count{std::min<std::size_t>(weights.num_weights, result.weights.size())};
    float total{};
    for (std::size_t influence{0}; influence < count; ++influence)
    {
        const ufbx_skin_weight weight{skin->weights.data[weights.weight_begin + influence]};
        const ufbx_skin_cluster* cluster{skin->clusters.data[weight.cluster_index]};
        result.joints[influence] = cluster->bone_node->typed_id;
        result.weights[influence] = static_cast<float>(weight.weight);
        total += result.weights[influence];
    }
    if (total > 0.0f)
    {
        for (float& weight : result.weights)
        {
            weight /= total;
        }
    }
    return result;
}

void write_vertex(binary_writer& writer, const cooked_vertex& vertex)
{
    write_vec3(writer, vertex.position);
    write_vec3(writer, vertex.normal);
    write_vec3(writer, vertex.tangent);
    writer.write_f32(vertex.tangent_sign);
    write_vec2(writer, vertex.uv);
    for (const std::uint32_t joint : vertex.joints)
    {
        writer.write_u32(joint);
    }
    for (const float weight : vertex.weights)
    {
        writer.write_f32(weight);
    }
}

nori::asset::result<cooked_texture_set> cook_textures(const ufbx_scene& scene, const std::filesystem::path& source_path, const nori::asset::texture_compression_quality quality)
{
    cooked_texture_set result;
    result.material_indices.resize(scene.materials.count);
    for (auto& indices : result.material_indices)
    {
        indices.fill(missing_index);
    }

    auto register_texture = [&scene, &result](const ufbx_material_map& map, const nori::resource::image_format format) -> nori::asset::result<std::uint32_t>
    {
        if (!map.texture_enabled || map.texture == nullptr || !map.texture->has_file)
        {
            return missing_index;
        }
        const std::size_t file_index{map.texture->file_index};
        if (file_index >= scene.texture_files.count)
        {
            return std::unexpected{nori::asset::failure{
                .code = nori::asset::error_code::invalid_format,
                .diagnostics = "material texture file index is invalid"
            }};
        }
        const auto it{std::ranges::find_if(result.recipes, [file_index, format](const texture_recipe& recipe)
        {
            return recipe.file_index == file_index && recipe.format == format;
        })};
        if (it != result.recipes.end())
        {
            return static_cast<std::uint32_t>(it - result.recipes.begin());
        }
        const ufbx_texture_file& file{scene.texture_files.data[file_index]};
        const std::string source_name{
            file.relative_filename.length > 0 ? file.relative_filename.data : "",
            file.relative_filename.length
        };
        result.recipes.push_back(texture_recipe{.file_index = file_index, .format = format, .name = std::to_string(result.recipes.size()), .source_name = source_name});
        return static_cast<std::uint32_t>(result.recipes.size() - 1);
    };

    for (const ufbx_material* material : scene.materials)
    {
        auto& indices{result.material_indices[material->typed_id]};
        const std::array registrations{
            register_texture(material->pbr.base_color, nori::resource::image_format::bc7_unorm_srgb),
            register_texture(material->pbr.normal_map, nori::resource::image_format::bc5_unorm),
            register_texture(material->pbr.roughness, nori::resource::image_format::bc7_unorm),
            register_texture(material->pbr.metalness, nori::resource::image_format::bc7_unorm),
            register_texture(material->pbr.emission_color, nori::resource::image_format::bc7_unorm_srgb),
            register_texture(material->pbr.opacity, nori::resource::image_format::bc7_unorm)
        };
        for (const auto [slot, registration] : registrations | std::views::enumerate)
        {
            if (!registration)
            {
                return std::unexpected{registration.error()};
            }
            indices[slot] = *registration;
        }
    }

    result.textures.reserve(result.recipes.size());
    for (const texture_recipe& recipe : result.recipes)
    {
        const ufbx_texture_file& texture{scene.texture_files.data[recipe.file_index]};
        std::vector<std::byte> external_data;
        std::span<const std::byte> data;
        if (texture.content.data != nullptr && texture.content.size > 0)
        {
            data = {
                static_cast<const std::byte*>(texture.content.data),
                texture.content.size
            };
        }
        else
        {
            const ufbx_string filename{texture.relative_filename};
            if (filename.length == 0)
            {
                return std::unexpected{nori::asset::failure{
                    .code = nori::asset::error_code::invalid_format,
                    .diagnostics = "texture has no embedded data or relative filename"
                }};
            }
            std::string path_text{filename.data, filename.length};
            std::ranges::replace(path_text, '\\', '/');
            const std::filesystem::path relative_path{nori::core::utf8_to_path(path_text)};
            const bool has_drive{path_text.size() >= 2 && std::isalpha(static_cast<unsigned char>(path_text[0])) && path_text[1] == ':'};
            if (!relative_path.is_relative() || relative_path.has_root_name() || has_drive)
            {
                return std::unexpected{nori::asset::failure{
                    .code = nori::asset::error_code::invalid_format,
                    .diagnostics = "texture filename is not relative: " + path_text
                }};
            }
            const std::filesystem::path path{std::filesystem::absolute(source_path.parent_path() / relative_path).lexically_normal()};
            auto read{nori::asset::read_file(path)};
            if (!read)
            {
                nori::asset::failure failure{read.error()};
                failure.diagnostics = nori::core::path_to_utf8(path);
                return std::unexpected{std::move(failure)};
            }
            external_data = std::move(*read);
            data = external_data;
        }

        auto image{nori::asset::decode_image(data, recipe.format, quality)};
        if (!image)
        {
            nori::asset::failure failure{std::move(image.error())};
            const std::string source_name{
                texture.relative_filename.length > 0 ? texture.relative_filename.data : "",
                texture.relative_filename.length
            };
            if (!source_name.empty())
            {
                failure.diagnostics = source_name +
                    (failure.diagnostics.empty() ? "" : ": " + failure.diagnostics);
            }
            return std::unexpected{std::move(failure)};
        }
        result.textures.push_back(nori::asset::cooked_texture{.name = recipe.name, .image = std::move(*image)});
    }
    return result;
}

nori::asset::result<nori::resource::binary> serialize_model_manifest(const ufbx_scene& scene, const cooked_texture_set& textures, std::span<const nori::asset::cooked_binary> animations)
{
    if (scene.nodes.count > missing_index || scene.materials.count > missing_index || scene.meshes.count > missing_index || animations.size() > missing_index)
    {
        return std::unexpected{nori::asset::failure{.code = nori::asset::error_code::invalid_format}};
    }

    binary_writer writer;
    writer.write_tag("NMDL");
    writer.write_u32(model_format_version);
    writer.write_u32(5);

    const std::size_t nodes_section{writer.begin_section("NODE")};
    writer.write_u32(to_u32(scene.nodes.count));
    for (const ufbx_node* node : scene.nodes)
    {
        writer.write_string(node->name);
        writer.write_u32(node->parent != nullptr ? node->parent->typed_id : missing_index);
        writer.write_u32(node->mesh != nullptr ? node->mesh->typed_id : missing_index);
        write_transform(writer, node->local_transform);
        writer.write_u32(to_u32(node->materials.count));
        for (const ufbx_material* material : node->materials)
        {
            writer.write_u32(material->typed_id);
        }
    }
    writer.end_section(nodes_section);

    const std::size_t materials_section{writer.begin_section("MATL")};
    writer.write_u32(to_u32(scene.materials.count));
    for (const ufbx_material* material : scene.materials)
    {
        writer.write_string(material->name);
        write_vec3(writer, to_vec3(material->pbr.base_color.value_vec3));
        writer.write_f32(static_cast<float>(material->pbr.base_color.value_vec4.w));
        writer.write_f32(static_cast<float>(material->pbr.roughness.value_real));
        writer.write_f32(static_cast<float>(material->pbr.metalness.value_real));
        write_vec3(writer, to_vec3(material->pbr.emission_color.value_vec3));
        writer.write_f32(static_cast<float>(material->pbr.opacity.value_real));
        for (const std::uint32_t index : textures.material_indices[material->typed_id])
        {
            writer.write_u32(index);
        }
    }
    writer.end_section(materials_section);

    const std::size_t textures_section{writer.begin_section("TEXR")};
    writer.write_u32(to_u32(textures.recipes.size()));
    for (const texture_recipe& recipe : textures.recipes)
    {
        writer.write_string(recipe.name);
        writer.write_string(recipe.source_name);
    }
    writer.end_section(textures_section);

    const std::size_t meshes_section{writer.begin_section("MESH")};
    writer.write_u32(to_u32(scene.meshes.count));
    for (std::size_t index{0}; index < scene.meshes.count; ++index)
    {
        writer.write_string(mesh_name(index));
    }
    writer.end_section(meshes_section);

    const std::size_t animations_section{writer.begin_section("ANIM")};
    writer.write_u32(to_u32(animations.size()));
    for (const auto& animation : animations)
    {
        writer.write_string(animation.name);
    }
    writer.end_section(animations_section);
    return writer.finish();
}

nori::asset::result<std::vector<nori::asset::cooked_binary>> serialize_meshes(const ufbx_scene& scene)
{
    std::vector<nori::asset::cooked_binary> result;
    result.reserve(scene.meshes.count);
    for (const auto [mesh_index, source] : scene.meshes | std::views::enumerate)
    {
        const ufbx_mesh& mesh{*source};
        if (mesh.num_triangles > missing_index / 3 || mesh.max_face_triangles > missing_index / 3)
        {
            return std::unexpected{nori::asset::failure{.code = nori::asset::error_code::invalid_format}};
        }
        const ufbx_skin_deformer* skin{mesh.skin_deformers.count > 0 ? mesh.skin_deformers.data[0] : nullptr};
        binary_writer writer;
        writer.write_tag("NMSH");
        writer.write_u32(1);
        writer.write_string(mesh.name);
        writer.write_u32(to_u32(mesh.num_triangles * 3));
        writer.write_u32(to_u32(mesh.num_triangles * 3));
        writer.write_u32(to_u32(mesh.num_triangles));
        writer.write_u32(skin != nullptr ? to_u32(skin->clusters.count) : 0);

        std::vector<std::uint32_t> indices(mesh.max_face_triangles * 3);
        std::vector<cooked_vertex> vertices;
        vertices.reserve(mesh.num_triangles * 3);
        std::vector<std::uint32_t> output_indices;
        output_indices.reserve(mesh.num_triangles * 3);
        std::vector<std::uint32_t> triangle_materials;
        triangle_materials.reserve(mesh.num_triangles);
        for (const auto [face_index, face] : mesh.faces | std::views::enumerate)
        {
            const std::uint32_t triangle_count{ufbx_triangulate_face(
                indices.data(), indices.size(), &mesh, face
            )};
            const std::uint32_t material{mesh.face_material.count > 0 ? mesh.face_material.data[face_index] : 0};
            for (std::uint32_t triangle{0}; triangle < triangle_count; ++triangle)
            {
                for (std::uint32_t corner{0}; corner < 3; ++corner)
                {
                    vertices.push_back(make_vertex(mesh, indices[triangle * 3 + corner], skin));
                    output_indices.push_back(to_u32(output_indices.size()));
                }
                triangle_materials.push_back(material);
            }
        }
        if (vertices.size() != mesh.num_triangles * 3)
        {
            return std::unexpected{nori::asset::failure{
                .code = nori::asset::error_code::invalid_format,
                .diagnostics = "triangulated mesh size mismatch"
            }};
        }
        for (const cooked_vertex& vertex : vertices)
        {
            write_vertex(writer, vertex);
        }
        for (const std::uint32_t index : output_indices)
        {
            writer.write_u32(index);
        }
        for (const std::uint32_t material : triangle_materials)
        {
            writer.write_u32(material);
        }
        if (skin != nullptr)
        {
            for (const ufbx_skin_cluster* cluster : skin->clusters)
            {
                writer.write_u32(cluster->bone_node->typed_id);
                write_matrix(writer, cluster->geometry_to_bone);
            }
        }
        result.push_back({.name = mesh_name(mesh_index), .binary = writer.finish()});
    }
    return result;
}

nori::asset::result<scene_pointer> load_scene(const std::filesystem::path& path)
{
    const auto source{nori::asset::read_file(path)};
    if (!source)
    {
        nori::asset::failure error{source.error()};
        error.diagnostics = nori::core::path_to_utf8(path);
        return std::unexpected{std::move(error)};
    }
    const std::string filename{nori::core::path_to_utf8(path)};
    ufbx_load_opts options{};
    options.filename = ufbx_string{filename.data(), filename.size()};
    options.target_axes = ufbx_axes_left_handed_y_up;
    options.handedness_conversion_axis = UFBX_MIRROR_AXIS_Z;
    options.target_unit_meters = 1.0;
    options.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
    options.geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_MODIFY_GEOMETRY;
    options.generate_missing_normals = true;
    options.normalize_normals = true;
    options.normalize_tangents = true;
    options.use_blender_pbr_material = true;
    options.ignore_missing_external_files = true;
    options.node_depth_limit = 512;
    ufbx_error error{};
    scene_pointer scene{ufbx_load_memory(source->data(), source->size(), &options, &error), &ufbx_free_scene};
    if (!scene)
    {
        return std::unexpected{nori::asset::failure{
            .code = error.type == UFBX_ERROR_UNRECOGNIZED_FILE_FORMAT ? nori::asset::error_code::unsupported_format : nori::asset::error_code::invalid_format,
            .diagnostics = filename + ": " + std::string{error.description.data, error.description.length}
        }};
    }
    return scene;
}

std::string_view node_name(const ufbx_node& node)
{
    return {node.name.data, node.name.length};
}

const ufbx_node* bone_parent(const ufbx_node& node)
{
    const ufbx_node* parent{node.parent};
    while (parent != nullptr && parent->bone == nullptr)
    {
        parent = parent->parent;
    }
    return parent;
}

bool has_animation(const ufbx_anim_stack& stack, const ufbx_node& node, const char* name)
{
    for (const ufbx_anim_layer* layer : stack.layers)
    {
        if (ufbx_find_anim_prop(layer, &node.element, name) != nullptr)
        {
            return true;
        }
    }
    return false;
}

nori::asset::result<nori::asset::cooked_binary> serialize_animation(const ufbx_scene& model, const ufbx_scene& scene, const nori::asset::animation_source& source)
{
    const std::string filename{nori::core::path_to_utf8(source.path)};
    auto fail = [&filename](std::string message) -> nori::asset::result<nori::asset::cooked_binary>
    {
        return std::unexpected{nori::asset::failure{.code = nori::asset::error_code::invalid_format, .diagnostics = filename + ": " + message}};
    };
    if (scene.anim_stacks.count != 1)
    {
        return fail("expected exactly one animation clip");
    }
    const ufbx_anim_stack& stack{*scene.anim_stacks.data[0]};

    struct mapped_node
    {
        const ufbx_node* node;
        bool translation;
        bool rotation;
        bool scale;
    };

    std::vector<mapped_node> mapped(model.nodes.count);
    for (const ufbx_node* node : model.nodes)
    {
        if (node->bone == nullptr)
        {
            continue;
        }
        const std::string_view name{node_name(*node)};
        if (name.empty())
        {
            return fail("model bone has no name");
        }
        std::size_t model_count{};
        std::size_t source_count{};
        const ufbx_node* match{};
        for (const ufbx_node* candidate : model.nodes)
        {
            model_count += candidate->bone != nullptr && node_name(*candidate) == name;
        }
        for (const ufbx_node* candidate : scene.nodes)
        {
            if (node_name(*candidate) == name)
            {
                ++source_count;
                match = candidate;
            }
        }
        if (model_count != 1 || source_count != 1)
        {
            return fail("missing or duplicate bone: " + std::string{name});
        }
        const ufbx_node* model_parent{bone_parent(*node)};
        const ufbx_node* source_parent{match->parent};
        while (source_parent != nullptr && source_parent->bone == nullptr && std::ranges::find_if(model.nodes, [source_parent](const ufbx_node* candidate)
        {
            return candidate->bone != nullptr && node_name(*candidate) == node_name(*source_parent);
        }) == model.nodes.end())
        {
            source_parent = source_parent->parent;
        }
        if ((model_parent == nullptr) != (source_parent == nullptr) ||
            (model_parent != nullptr && node_name(*model_parent) != node_name(*source_parent)))
        {
            return fail("bone parent mismatch: " + std::string{name});
        }
        mapped[node->typed_id] = {
            .node = match,
            .translation = has_animation(stack, *match, "Lcl Translation"),
            .rotation = has_animation(stack, *match, "Lcl Rotation"),
            .scale = has_animation(stack, *match, "Lcl Scaling")
        };
    }
    for (const ufbx_node* node : scene.nodes)
    {
        if (node->bone == nullptr)
        {
            continue;
        }
        const auto name{node_name(*node)};
        const auto found{std::ranges::find_if(model.nodes, [name](const ufbx_node* candidate)
        {
            return candidate->bone != nullptr && node_name(*candidate) == name;
        })};
        if (found == model.nodes.end())
        {
            return fail("animation bone is absent from model: " + std::string{name});
        }
    }
    const double frames_per_second{scene.settings.frames_per_second > 0.0 ? scene.settings.frames_per_second : 30.0};
    const double duration{std::max(0.0, stack.time_end - stack.time_begin)};
    const double frames{std::floor(duration * frames_per_second + 0.5) + 1};
    if (!std::isfinite(frames) || frames > maximum_animation_frames)
    {
        return fail("animation exceeds frame limit");
    }
    const std::size_t frame_count{static_cast<std::size_t>(frames)};
    binary_writer writer;
    writer.write_tag("NANI");
    writer.write_u32(1);
    writer.write_string(source.name);
    writer.write_f32(static_cast<float>(frames_per_second));
    writer.write_u32(to_u32(frame_count));
    writer.write_u32(to_u32(model.nodes.count));
    for (std::size_t frame{0}; frame < frame_count; ++frame)
    {
        const double time{std::min(stack.time_end, stack.time_begin + static_cast<double>(frame) / frames_per_second)};
        for (const ufbx_node* node : model.nodes)
        {
            ufbx_transform transform{node->local_transform};
            const mapped_node& mapping{mapped[node->typed_id]};
            if (mapping.node != nullptr && (mapping.translation || mapping.rotation || mapping.scale))
            {
                const ufbx_transform sample{ufbx_evaluate_transform(stack.anim, mapping.node, time)};
                if (mapping.translation)
                {
                    transform.translation = sample.translation;
                }
                if (mapping.rotation)
                {
                    transform.rotation = sample.rotation;
                }
                if (mapping.scale)
                {
                    transform.scale = sample.scale;
                }
            }
            write_transform(writer, transform);
        }
    }
    return nori::asset::cooked_binary{.name = source.name, .binary = writer.finish()};
}
} // namespace

namespace nori::asset
{
result<cooked_model> cook_model(const std::filesystem::path& source_path, const texture_compression_quality quality, const std::span<const animation_source> sources)
{
    const auto scene{load_scene(source_path)};
    if (!scene)
    {
        return std::unexpected{scene.error()};
    }

    auto textures{cook_textures(**scene, source_path, quality)};
    if (!textures)
    {
        return std::unexpected{std::move(textures.error())};
    }

    auto meshes{serialize_meshes(**scene)};
    if (!meshes)
    {
        return std::unexpected{std::move(meshes.error())};
    }

    std::vector<cooked_binary> animations;
    animations.reserve(sources.size());
    for (const animation_source& source : sources)
    {
        const auto animation_scene{load_scene(source.path)};
        if (!animation_scene)
        {
            return std::unexpected{animation_scene.error()};
        }
        auto animation{serialize_animation(**scene, **animation_scene, source)};
        if (!animation)
        {
            return std::unexpected{std::move(animation.error())};
        }
        animations.push_back(std::move(*animation));
    }

    auto manifest{serialize_model_manifest(**scene, *textures, animations)};
    if (!manifest)
    {
        return std::unexpected{std::move(manifest.error())};
    }

    return cooked_model{
        .manifest = std::move(*manifest),
        .meshes = std::move(*meshes),
        .textures = std::move(textures->textures),
        .animations = std::move(animations)
    };
}
} // namespace nori::asset
