#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include <nori/core/endian.hpp>
#include <nori/resource.hpp>

#include "cli_test_helpers.hpp"

namespace
{
void write_text(const std::filesystem::path& path, std::string_view value)
{
    std::ofstream output{path, std::ios::binary};
    output.write(value.data(), static_cast<std::streamsize>(value.size()));
}

std::uint32_t read_u32(nori::resource::binary_view binary, std::size_t offset)
{
    std::uint32_t value{};
    std::memcpy(&value, binary.data.data() + offset, sizeof(value));
    return nori::core::to_endian(value, std::endian::little);
}

#ifdef _WIN32
std::vector<unsigned char> decode_base64(std::string_view value)
{
    constexpr std::string_view alphabet{
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
    };
    std::vector<unsigned char> result;
    std::uint32_t accumulator{};
    int bit_count{};
    for (const char character : value)
    {
        if (character == '=')
        {
            break;
        }
        const std::size_t index{alphabet.find(character)};
        if (index == std::string_view::npos)
        {
            continue;
        }
        accumulator = (accumulator << 6) | static_cast<std::uint32_t>(index);
        bit_count += 6;
        if (bit_count >= 8)
        {
            bit_count -= 8;
            result.push_back(static_cast<unsigned char>(accumulator >> bit_count));
            accumulator &= (1u << bit_count) - 1u;
        }
    }
    return result;
}
#endif

class import_test : public cli_test_fixture
{
};

TEST_F(import_test, import_binary)
{
    write_bytes(binary_path_, {0x01, 0x02, 0x03, 0x04});
    const auto imported{call("import", {{"type", "binary"}, {"path", lhs_ + "::data"}, {"source", binary_path_.generic_string()}})};
    ASSERT_TRUE(imported);
    EXPECT_TRUE(imported->is_null());

    const auto value{call("get", {{"path", lhs_ + "::data"}})};
    ASSERT_TRUE(value);
    EXPECT_EQ(value->at("type"), "binary");
    EXPECT_EQ(value->at("size"), 4u);
}

TEST_F(import_test, import_model)
{
    ASSERT_TRUE(call("import", {{"type", "model"}, {"path", lhs_ + "::cube"}, {"source", {{"model", NORI_UFBX_CUBE_PATH}}}}));
    const nori::resource::binary_view model{nori::resource::get<nori::resource::binary_view>(lhs_ + "::cube")};
    ASSERT_GE(model.data.size(), 12u);
    EXPECT_EQ(std::to_integer<char>(model.data[0]), 'N');
    EXPECT_EQ(std::to_integer<char>(model.data[1]), 'M');
    EXPECT_EQ(std::to_integer<char>(model.data[2]), 'D');
    EXPECT_EQ(std::to_integer<char>(model.data[3]), 'L');
    EXPECT_EQ(read_u32(model, 4), 1u);

    const auto mesh{nori::resource::try_get<nori::resource::binary_view>(lhs_ + "::cube/mesh/0")};
    ASSERT_TRUE(mesh);
    ASSERT_GE(mesh->data.size(), 8u);

    ASSERT_TRUE(call("set", {{"path", lhs_ + "::cube/stale"}, {"type", "string"}, {"value", "old"}}));
    ASSERT_TRUE(call("import", {{"type", "model"}, {"path", lhs_ + "::cube"}, {"source", {{"model", NORI_UFBX_CUBE_PATH}}}, {"quality", "quick"}}));
    EXPECT_TRUE(nori::resource::try_get(lhs_ + "::cube/stale"));

    nori::resource::shutdown();
    ASSERT_TRUE(nori::resource::initialize());
    const nori::resource::handle root{nori::resource::get(lhs_)};
    const nori::resource::handle target{nori::resource::get(root, "cube")};
    EXPECT_EQ(nori::resource::type_of(target), nori::resource::property_type::binary);
    const auto decoded{nori::resource::try_get<nori::resource::model>(lhs_ + "::cube")};
    ASSERT_TRUE(decoded);
    ASSERT_EQ(decoded->nodes.size(), 2u);
    ASSERT_EQ(decoded->meshes.size(), 1u);
    EXPECT_FALSE(decoded->nodes[0].parent_index);
    EXPECT_EQ(decoded->nodes[1].parent_index, 0u);
    EXPECT_EQ(decoded->nodes[1].mesh_index, 0u);
    const nori::resource::model::mesh& geometry{decoded->meshes.front()};
    EXPECT_EQ(geometry.indices.size(), 36u);
    EXPECT_EQ(geometry.triangle_material_slots.size(), 12u);
    EXPECT_EQ(geometry.vertices.front().joint_node_indices.front(), std::numeric_limits<std::uint32_t>::max());
    EXPECT_EQ(nori::resource::get<nori::resource::model>(target).meshes.front().indices, geometry.indices);
    EXPECT_EQ(nori::resource::get<nori::resource::model>(root, "cube").nodes[1].name, decoded->nodes[1].name);
}

TEST_F(import_test, model_query_reports_type_format_and_lookup_errors)
{
    using nori::resource::binary;
    using nori::resource::error_code;
    using nori::resource::model;
    using nori::resource::try_get;
    const std::string path{lhs_ + "::cube"};
    ASSERT_TRUE(call("import", {{"type", "model"}, {"path", path}, {"source", {{"model", NORI_UFBX_CUBE_PATH}}}}));
    const nori::resource::handle target{nori::resource::get(path)};
    const auto manifest{nori::resource::get<binary>(target)};
    nori::resource::set(target, binary{});
    EXPECT_EQ(try_get<model>(target), std::unexpected{error_code::type_mismatch});
    binary unsupported{manifest};
    unsupported.data[4] = std::byte{2};
    nori::resource::set(target, unsupported);
    EXPECT_EQ(try_get<model>(target), std::unexpected{error_code::invalid_format});
    nori::resource::set(target, manifest);
    nori::resource::remove(nori::resource::get(path + "/mesh/0"));
    EXPECT_EQ(try_get<model>(target), std::unexpected{error_code::not_found});
}

TEST_F(import_test, model_animation_is_named_and_sampled_in_model_node_order)
{
    const std::string path{lhs_ + "::character"};
    const nlohmann::json source{{"model", NORI_UFBX_SKIN_PATH}, {"animation", {{"walk", NORI_UFBX_SKIN_PATH}}}};
    const auto imported{call("import", {{"type", "model"}, {"path", path}, {"source", source}})};
    ASSERT_TRUE(imported);
    const auto animation{nori::resource::get<nori::resource::binary_view>(path + "/animation/walk")};
    ASSERT_GE(animation.data.size(), 28u);
    EXPECT_EQ(std::to_integer<char>(animation.data[0]), 'N');
    EXPECT_EQ(std::to_integer<char>(animation.data[3]), 'I');
    const std::size_t name_size{read_u32(animation, 8)};
    const std::size_t header_size{24 + name_size};
    const std::uint32_t frames{read_u32(animation, 16 + name_size)};
    const std::uint32_t nodes{read_u32(animation, 20 + name_size)};
    ASSERT_GT(frames, 1u);
    ASSERT_GT(nodes, 0u);
    ASSERT_EQ(animation.data.size(), header_size + static_cast<std::size_t>(frames) * nodes * 40);
    const auto* first{animation.data.data() + header_size};
    EXPECT_NE(std::memcmp(first, first + (frames - 1) * nodes * 40, nodes * 40), 0);

    nori::resource::shutdown();
    ASSERT_TRUE(nori::resource::initialize());
    const auto decoded{nori::resource::try_get<nori::resource::model>(path)};
    ASSERT_TRUE(decoded);
    ASSERT_EQ(decoded->nodes.size(), nodes);
    ASSERT_EQ(decoded->animations.size(), 1u);
    const nori::resource::model::animation& clip{decoded->animations.front()};
    EXPECT_EQ(clip.name, "walk");
    EXPECT_EQ(clip.frame_count, frames);
    ASSERT_EQ(clip.local_transforms.size(), static_cast<std::size_t>(frames) * nodes);
    const auto clip_bytes{nori::resource::get<nori::resource::binary>(path + "/animation/walk")};
    const nori::resource::model::transform& sample{clip.local_transforms[(frames - 1) * nodes + nodes - 1]};
    EXPECT_FLOAT_EQ(sample.translation.x, std::bit_cast<float>(read_u32({clip_bytes.data}, header_size + (clip.local_transforms.size() - 1) * 40)));

    ASSERT_FALSE(decoded->meshes.empty());
    const nori::resource::model::mesh& geometry{decoded->meshes.front()};
    ASSERT_FALSE(geometry.joints.empty());
    const nori::resource::model::joint& joint{geometry.joints.front()};
    EXPECT_LT(joint.node_index, nodes);
    const auto mesh_bytes{nori::resource::get<nori::resource::binary>(path + "/mesh/0")};
    const std::size_t skin_offset{mesh_bytes.data.size() - geometry.joints.size() * 52};
    EXPECT_EQ(joint.node_index, read_u32({mesh_bytes.data}, skin_offset));
    EXPECT_FLOAT_EQ(joint.inverse_bind_matrix.m41, std::bit_cast<float>(read_u32({mesh_bytes.data}, skin_offset + 40)));
    EXPECT_FLOAT_EQ(joint.inverse_bind_matrix.m44, 1.0f);

    ASSERT_TRUE(call("import", {{"type", "model"}, {"path", path}, {"source", {{"model", NORI_UFBX_SKIN_PATH}}}}));
    EXPECT_FALSE(nori::resource::try_get(path + "/animation/walk"));
    EXPECT_TRUE(nori::resource::get<nori::resource::model>(path).animations.empty());
}

TEST_F(import_test, model_rejects_invalid_animation_sources)
{
    const std::string path{lhs_ + "::character"};
    const auto no_clip{call("import", {{"type", "model"}, {"path", path}, {"source", {{"model", NORI_UFBX_SKIN_PATH}, {"animation", {{"idle", NORI_UFBX_CUBE_PATH}}}}}})};
    ASSERT_FALSE(no_clip);
    EXPECT_EQ(no_clip.error(), nori::cli::error_code::invalid_format);
    const auto multiple_clips{call("import", {{"type", "model"}, {"path", path}, {"source", {{"model", NORI_UFBX_SKIN_PATH}, {"animation", {{"idle", NORI_UFBX_MULTI_ANIM_PATH}}}}}})};
    ASSERT_FALSE(multiple_clips);
    EXPECT_EQ(multiple_clips.error(), nori::cli::error_code::invalid_format);
    const auto mismatch{call("import", {{"type", "model"}, {"path", path}, {"source", {{"model", NORI_UFBX_SKIN_PATH}, {"animation", {{"idle", NORI_UFBX_OTHER_SKIN_PATH}}}}}})};
    ASSERT_FALSE(mismatch);
    EXPECT_EQ(mismatch.error(), nori::cli::error_code::invalid_format);
    const auto invalid_name{call("import", {{"type", "model"}, {"path", path}, {"source", {{"model", NORI_UFBX_SKIN_PATH}, {"animation", {{"a::b", NORI_UFBX_SKIN_PATH}}}}}})};
    ASSERT_FALSE(invalid_name);
    EXPECT_EQ(invalid_name.error(), nori::cli::error_code::invalid_argument);
}

TEST_F(import_test, model_uses_default_pose_for_unanimated_bones_and_checks_parent)
{
    std::ifstream input{NORI_UFBX_SKIN_PATH, std::ios::binary};
    ASSERT_TRUE(input);
    std::string source{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    const auto bone{source.find("Model::Bone005")};
    ASSERT_NE(bone, std::string::npos);
    const auto translation{source.find("27.136417388916", bone)};
    ASSERT_NE(translation, std::string::npos);
    source.replace(translation, std::string_view{"27.136417388916"}.size(), "900.0");
    const auto animation_path{workdir_ / "animation.fbx"};
    write_text(animation_path, source);
    const std::string path{lhs_ + "::character"};
    const auto imported{call("import", {{"type", "model"}, {"path", path}, {"source", {{"model", NORI_UFBX_SKIN_PATH}, {"animation", {{"idle", animation_path.generic_string()}}}}}})};
    ASSERT_TRUE(imported);
    const auto model{nori::resource::get<nori::resource::binary_view>(path)};
    const auto animation{nori::resource::get<nori::resource::binary_view>(path + "/animation/idle")};
    const std::uint32_t node_count{read_u32(model, 24)};
    std::size_t offset{28};
    std::size_t bone_index{node_count};
    std::size_t transform_offset{};
    for (std::size_t index{}; index < node_count; ++index)
    {
        const std::size_t length{read_u32(model, offset)};
        const std::string_view name{reinterpret_cast<const char*>(model.data.data() + offset + 4), length};
        const std::size_t transform{offset + 4 + length + 8};
        if (name == "Bone005")
        {
            bone_index = index;
            transform_offset = transform;
        }
        const std::size_t material_count_offset{transform + 40};
        offset = material_count_offset + 4 + 4 * read_u32(model, material_count_offset);
    }
    ASSERT_LT(bone_index, node_count);
    const std::size_t animation_header{24 + read_u32(animation, 8)};
    ASSERT_EQ(read_u32(animation, 20 + read_u32(animation, 8)), node_count);
    EXPECT_EQ(std::memcmp(model.data.data() + transform_offset, animation.data.data() + animation_header + bone_index * 40, 40), 0);

    const std::string old_parent{"C: \"OO\",2426744500928,2426744483392"};
    const auto parent{source.find(old_parent)};
    ASSERT_NE(parent, std::string::npos);
    source.replace(parent, old_parent.size(), "C: \"OO\",2426744500928,2426744498736");
    write_text(animation_path, source);
    const auto mismatch{call("import", {{"type", "model"}, {"path", path}, {"source", {{"model", NORI_UFBX_SKIN_PATH}, {"animation", {{"idle", animation_path.generic_string()}}}}}})};
    ASSERT_FALSE(mismatch);
    EXPECT_EQ(mismatch.error(), nori::cli::error_code::invalid_format);

    source.replace(parent, std::string_view{"C: \"OO\",2426744500928,2426744498736"}.size(), old_parent);
    const auto bone_name{source.find("Model::Bone005")};
    ASSERT_NE(bone_name, std::string::npos);
    source.replace(bone_name, std::string_view{"Model::Bone005"}.size(), "Model::Bone004");
    write_text(animation_path, source);
    const auto duplicate{call("import", {{"type", "model"}, {"path", path}, {"source", {{"model", NORI_UFBX_SKIN_PATH}, {"animation", {{"idle", animation_path.generic_string()}}}}}})};
    ASSERT_FALSE(duplicate);
    EXPECT_EQ(duplicate.error(), nori::cli::error_code::invalid_format);
}

TEST_F(import_test, model_maps_animation_nodes_when_source_order_differs)
{
    std::ifstream input{NORI_UFBX_SKIN_PATH, std::ios::binary};
    ASSERT_TRUE(input);
    std::string source{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    const auto objects{source.find("Objects:  {")};
    ASSERT_NE(objects, std::string::npos);
    source.insert(objects + std::string_view{"Objects:  {"}.size(), "\n\tModel: 999999999999, \"Model::Dummy\", \"Null\" {\n\t\tVersion: 232\n\t}");
    const auto connections{source.find("Connections:  {")};
    ASSERT_NE(connections, std::string::npos);
    source.insert(connections + std::string_view{"Connections:  {"}.size(), "\n\tC: \"OO\",999999999999,0");
    const auto reordered{workdir_ / "reordered.fbx"};
    write_text(reordered, source);
    const std::string path{lhs_ + "::character"};
    const auto imported{call("import", {{"type", "model"}, {"path", path}, {"source", {{"model", NORI_UFBX_SKIN_PATH}, {"animation", {{"one", NORI_UFBX_SKIN_PATH}, {"two", reordered.generic_string()}}}}}})};
    ASSERT_TRUE(imported);
    const auto first{nori::resource::get<nori::resource::binary_view>(path + "/animation/one")};
    const auto second{nori::resource::get<nori::resource::binary_view>(path + "/animation/two")};
    ASSERT_EQ(first.data.size(), second.data.size());
    ASSERT_GT(first.data.size(), 15u);
    EXPECT_EQ(std::memcmp(first.data.data() + 15, second.data.data() + 15, first.data.size() - 15), 0);
}

TEST_F(import_test, import_model_preserves_vertex_components)
{
    const auto source{workdir_ / "triangle.obj"};
    write_text(source,
        "o triangle\n"
        "v 1.25 2.5 3.75\n"
        "v 4.5 2.5 3.75\n"
        "v 1.25 6.5 3.75\n"
        "vn 0 0 1\n"
        "vt 0.25 0.5\n"
        "vt 0.75 0.5\n"
        "vt 0.25 1\n"
        "f 1/1/1 2/2/1 3/3/1\n");
    ASSERT_TRUE(call("import", {{"type", "model"}, {"path", lhs_ + "::triangle"}, {"source", {{"model", source.generic_string()}}}}));
    const auto mesh{nori::resource::get<nori::resource::binary_view>(lhs_ + "::triangle/mesh/0")};
    ASSERT_GE(mesh.data.size(), 12u);
    const std::size_t header_size{28u + read_u32(mesh, 8)};
    ASSERT_EQ(mesh.data.size(), header_size + 3u * 80u + 4u * 4u);
    EXPECT_EQ(read_u32(mesh, 4), 1u);
    EXPECT_EQ(read_u32(mesh, header_size - 16), 3u);
    EXPECT_EQ(read_u32(mesh, header_size - 12), 3u);
    EXPECT_EQ(read_u32(mesh, header_size - 8), 1u);
    EXPECT_EQ(read_u32(mesh, header_size - 4), 0u);
    constexpr std::array<std::array<float, 12>, 3> expected{{
        {1.25f, 2.5f, 3.75f, 0, 0, 1, 0, 0, 0, 1, 0.25f, 0.5f},
        {4.5f, 2.5f, 3.75f, 0, 0, 1, 0, 0, 0, 1, 0.75f, 0.5f},
        {1.25f, 6.5f, 3.75f, 0, 0, 1, 0, 0, 0, 1, 0.25f, 1},
    }};
    for (std::size_t vertex{0}; vertex < expected.size(); ++vertex)
    {
        const std::size_t offset{header_size + vertex * 80};
        for (std::size_t component{0}; component < expected[vertex].size(); ++component)
        {
            EXPECT_FLOAT_EQ(std::bit_cast<float>(read_u32(mesh, offset + component * 4)), expected[vertex][component]);
        }
        for (std::size_t influence{0}; influence < 4; ++influence)
        {
            EXPECT_EQ(read_u32(mesh, offset + 48 + influence * 4), UINT32_MAX);
            EXPECT_FLOAT_EQ(std::bit_cast<float>(read_u32(mesh, offset + 64 + influence * 4)), 0.0f);
        }
        EXPECT_EQ(read_u32(mesh, header_size + 240 + vertex * 4), vertex);
    }
}

TEST_F(import_test, import_model_preserves_up_direction_when_mirroring)
{
    const std::string path{lhs_ + "::plane"};
    ASSERT_TRUE(call("import", {{"type", "model"}, {"path", path}, {"source", {{"model", NORI_UFBX_AXES_PATH}}}}));
    const auto model{nori::resource::get<nori::resource::binary_view>(path)};
    constexpr std::size_t node_count_offset{24};
    constexpr std::size_t root_node_offset{node_count_offset + 4};
    constexpr std::size_t plane_node_offset{root_node_offset + 56};
    constexpr std::size_t plane_translation_offset{plane_node_offset + 4 + 5 + 8};
    ASSERT_GE(model.data.size(), plane_translation_offset + 3 * sizeof(float));
    ASSERT_EQ(read_u32(model, node_count_offset), 2u);
    ASSERT_EQ(read_u32(model, root_node_offset), 0u);
    ASSERT_EQ(read_u32(model, plane_node_offset), 5u);
    EXPECT_FLOAT_EQ(std::bit_cast<float>(read_u32(model, plane_translation_offset)), 1.0f);
    EXPECT_FLOAT_EQ(std::bit_cast<float>(read_u32(model, plane_translation_offset + 4)), 3.0f);
    EXPECT_FLOAT_EQ(std::bit_cast<float>(read_u32(model, plane_translation_offset + 8)), 2.0f);
}

#ifdef _WIN32
TEST_F(import_test, model_imports_external_textures)
{
    const auto embedded_result{call("import", {{"type", "model"}, {"path", lhs_ + "::embedded"}, {"source", {{"model", NORI_UFBX_TEXTURED_PATH}}}})};
    ASSERT_TRUE(embedded_result);
    EXPECT_TRUE(nori::resource::try_get<nori::resource::image_view>(lhs_ + "::embedded/texture/0"));
    std::ifstream input{NORI_UFBX_TEXTURED_PATH, std::ios::binary};
    ASSERT_TRUE(input);
    const std::string embedded{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    const std::string external{std::regex_replace(embedded, std::regex{"Content:[[:space:]]*,[[:space:]]*\"[^\"]*\""}, "")};
    ASSERT_NE(external.size(), embedded.size());
    const auto model_path{workdir_ / "model.fbx"};
    write_text(model_path, external);
    const auto texture_dir{workdir_ / "textures"};
    ASSERT_TRUE(std::filesystem::create_directories(texture_dir));
    const auto diffuse{texture_dir / "checkerboard_diffuse.png"};
    const auto emission{texture_dir / "checkerboard_emissive.png"};
    std::filesystem::copy_file(std::filesystem::path{NORI_UFBX_TEXTURE_DIR} / diffuse.filename(), diffuse);
    std::filesystem::copy_file(std::filesystem::path{NORI_UFBX_TEXTURE_DIR} / emission.filename(), emission);
    const std::string target{lhs_ + "::textured"};
    const nlohmann::json params{{"type", "model"}, {"path", target}, {"source", {{"model", model_path.generic_string()}}}};
    const auto imported{call("import", params)};
    ASSERT_TRUE(imported);
    EXPECT_TRUE(nori::resource::try_get<nori::resource::image_view>(target + "/texture/0"));
    EXPECT_TRUE(nori::resource::try_get<nori::resource::image_view>(target + "/texture/1"));
    nori::resource::shutdown();
    ASSERT_TRUE(nori::resource::initialize());
    const auto decoded{nori::resource::try_get<nori::resource::model>(target)};
    ASSERT_TRUE(decoded);
    ASSERT_EQ(decoded->textures.size(), 2u);
    EXPECT_EQ(decoded->textures[0], nori::resource::get(target + "/texture/0"));
    EXPECT_EQ(decoded->textures[1], nori::resource::get(target + "/texture/1"));
    ASSERT_FALSE(decoded->materials.empty());
    EXPECT_EQ(decoded->materials.front().base_color_texture_index, 0u);
    EXPECT_EQ(decoded->materials.front().emission_texture_index, 1u);
    std::filesystem::remove(diffuse);
    const auto missing{call("import", params)};
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error(), nori::cli::error_code::io_error);
    EXPECT_TRUE(nori::resource::try_get<nori::resource::image_view>(target + "/texture/0"));

    std::string absolute{external};
    const std::string relative_name{"textures\\checkerboard_diffuse.png"};
    const std::string absolute_name{"C:/other/checkerboard_diffuse.png"};
    for (std::size_t position{}; (position = absolute.find(relative_name, position)) != std::string::npos; position += absolute_name.size())
    {
        absolute.replace(position, relative_name.size(), absolute_name);
    }
    write_text(model_path, absolute);
    const auto invalid_path{call("import", params)};
    ASSERT_FALSE(invalid_path);
    EXPECT_EQ(invalid_path.error(), nori::cli::error_code::invalid_format);
}

TEST_F(import_test, import_image)
{
    constexpr std::string_view red_png{
        "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGP4z8DwHwAFAAH/"
        "iZk9HQAAAABJRU5ErkJggg=="
    };
    write_bytes(image_path_, decode_base64(red_png));

    ASSERT_TRUE(call("import", {{"type", "image"}, {"path", lhs_ + "::image"}, {"source", image_path_.generic_string()}}));
    const auto image{nori::resource::get<nori::resource::image_view>(lhs_ + "::image")};
    EXPECT_EQ(image.width, 1u);
    EXPECT_EQ(image.height, 1u);
    EXPECT_EQ(image.mip_levels, 1u);

    ASSERT_TRUE(call("import", {{"type", "image"}, {"path", lhs_ + "::bc7"}, {"source", image_path_.generic_string()}, {"format", "bc7-srgb"}, {"quality", "normal"}}));
    EXPECT_EQ(
        nori::resource::get<nori::resource::image_view>(lhs_ + "::bc7").format,
        nori::resource::image_format::bc7_unorm_srgb
    );

    const auto bad_format{call("import", {{"type", "image"}, {"path", lhs_ + "::bad-format"}, {"source", image_path_.generic_string()}, {"format", "bc3"}})};
    ASSERT_FALSE(bad_format);
    EXPECT_EQ(bad_format.error(), nori::cli::error_code::invalid_argument);
}

TEST_F(import_test, import_shader)
{
    write_text(shader_path_, "float4 main(float4 position : POSITION) : SV_Position { return position; }");
    ASSERT_TRUE(call("import", {{"type", "shader"}, {"path", lhs_ + "::vertex"}, {"source", shader_path_.generic_string()}}));
    const auto binary{nori::resource::get<nori::resource::binary_view>(lhs_ + "::vertex")};
    ASSERT_GE(binary.data.size(), 4u);
    EXPECT_EQ(std::to_integer<char>(binary.data[0]), 'D');
    EXPECT_EQ(std::to_integer<char>(binary.data[1]), 'X');
    EXPECT_EQ(std::to_integer<char>(binary.data[2]), 'B');
    EXPECT_EQ(std::to_integer<char>(binary.data[3]), 'C');
}

TEST_F(import_test, import_rejects_invalid_shader)
{
    ASSERT_TRUE(call("set", {{"path", lhs_ + "::shader"}, {"type", "string"}, {"value", "preserved"}}));
    write_text(shader_path_, "this is not valid HLSL");
    const auto invalid{call("import", {{"type", "shader"}, {"path", lhs_ + "::shader"}, {"source", shader_path_.generic_string()}})};
    ASSERT_FALSE(invalid);
    EXPECT_EQ(invalid.error(), nori::cli::error_code::invalid_format);
    EXPECT_EQ(nori::resource::get<std::string>(lhs_ + "::shader"), "preserved");
}
#endif

TEST_F(import_test, import_rejects_invalid_input)
{
    const std::string path{lhs_ + "::preserved"};
    ASSERT_TRUE(call("set", {{"path", path}, {"type", "string"}, {"value", "value"}}));

    const auto unknown_type{call("import", {{"type", "texture"}, {"path", path}, {"source", binary_path_.generic_string()}})};
    ASSERT_FALSE(unknown_type);
    EXPECT_EQ(unknown_type.error(), nori::cli::error_code::invalid_argument);
    EXPECT_EQ(nori::resource::get<std::string>(path), "value");

    const auto missing_file{call("import", {{"type", "binary"}, {"path", path}, {"source", "does-not-exist.bin"}})};
    ASSERT_FALSE(missing_file);
    EXPECT_EQ(missing_file.error(), nori::cli::error_code::io_error);
    EXPECT_EQ(nori::resource::get<std::string>(path), "value");

    write_bytes(binary_path_, {0, 1, 2, 3});
    const auto invalid_model{call("import", {{"type", "model"}, {"path", path}, {"source", {{"model", binary_path_.generic_string()}}}})};
    ASSERT_FALSE(invalid_model);
    EXPECT_EQ(invalid_model.error(), nori::cli::error_code::unsupported_format);
    EXPECT_EQ(nori::resource::get<std::string>(path), "value");
}

TEST_F(import_test, import_preserves_children_on_type_change)
{
    const std::string path{lhs_ + "::value"};
    ASSERT_TRUE(call("set", {{"path", path}, {"type", "string"}, {"value", "value"}}));
    ASSERT_TRUE(call("set", {{"path", path + "/child"}, {"type", "string"}, {"value", "child"}}));
    write_bytes(binary_path_, {0, 1, 2, 3});

    ASSERT_TRUE(call("import", {{"type", "binary"}, {"path", path}, {"source", binary_path_.generic_string()}}));
    EXPECT_TRUE(call("exists", {{"path", path + "/child"}})->get<bool>());

    ASSERT_TRUE(call("set", {{"path", path + "/keep"}, {"type", "string"}, {"value", "keep"}}));
    ASSERT_TRUE(call("import", {{"type", "binary"}, {"path", path}, {"source", binary_path_.generic_string()}}));
    EXPECT_TRUE(call("exists", {{"path", path + "/keep"}})->get<bool>());
}
} // namespace
