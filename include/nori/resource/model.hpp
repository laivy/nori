#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nori/core/matrix.hpp>
#include <nori/core/vector.hpp>
#include <nori/resource/handle.hpp>

namespace nori::resource
{
struct model
{
    struct transform
    {
        core::vec3 translation;
        core::vec4 rotation;
        core::vec3 scale;
    };

    struct node
    {
        std::string name;
        std::optional<std::uint32_t> parent_index;
        std::optional<std::uint32_t> mesh_index;
        transform local_transform;
        std::vector<std::uint32_t> material_indices;
    };

    struct vertex
    {
        core::vec3 position;
        core::vec3 normal;
        core::vec3 tangent;
        float tangent_sign;
        core::vec2 uv;
        std::array<std::uint32_t, 4> joint_node_indices;
        std::array<float, 4> joint_weights;
    };

    struct joint
    {
        std::uint32_t node_index;
        core::mat4 inverse_bind_matrix;
    };

    struct mesh
    {
        std::string name;
        std::vector<vertex> vertices;
        std::vector<std::uint32_t> indices;
        std::vector<std::uint32_t> triangle_material_slots;
        std::vector<joint> joints;
    };

    struct material
    {
        std::string name;
        core::vec4 base_color;
        float roughness;
        float metalness;
        core::vec3 emission;
        float opacity;
        std::optional<std::uint32_t> base_color_texture_index;
        std::optional<std::uint32_t> normal_texture_index;
        std::optional<std::uint32_t> roughness_texture_index;
        std::optional<std::uint32_t> metalness_texture_index;
        std::optional<std::uint32_t> emission_texture_index;
        std::optional<std::uint32_t> opacity_texture_index;
    };

    struct animation
    {
        std::string name;
        float frames_per_second;
        std::uint32_t frame_count;
        std::vector<transform> local_transforms;
    };

    std::vector<node> nodes;
    std::vector<mesh> meshes;
    std::vector<material> materials;
    std::vector<handle> textures;
    std::vector<animation> animations;
};
}
