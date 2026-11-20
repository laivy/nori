#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
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
    ASSERT_TRUE(call("import", {{"type", "model"}, {"path", lhs_ + "::cube"}, {"source", NORI_UFBX_CUBE_PATH}}));
    const nori::resource::binary_view model{nori::resource::get<nori::resource::binary_view>(lhs_ + "::cube")};
    ASSERT_GE(model.data.size(), 12u);
    EXPECT_EQ(std::to_integer<char>(model.data[0]), 'N');
    EXPECT_EQ(std::to_integer<char>(model.data[1]), 'M');
    EXPECT_EQ(std::to_integer<char>(model.data[2]), 'D');
    EXPECT_EQ(std::to_integer<char>(model.data[3]), 'L');

    const auto mesh{nori::resource::try_get<nori::resource::binary_view>(lhs_ + "::cube/meshes/mesh-0000")};
    ASSERT_TRUE(mesh);
    ASSERT_GE(mesh->data.size(), 8u);

    ASSERT_TRUE(call("set", {{"path", lhs_ + "::cube/stale"}, {"type", "string"}, {"value", "old"}}));
    ASSERT_TRUE(call("import", {{"type", "model"}, {"path", lhs_ + "::cube"}, {"source", NORI_UFBX_CUBE_PATH}, {"quality", "quick"}}));
    EXPECT_TRUE(nori::resource::try_get(lhs_ + "::cube/stale"));
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
    ASSERT_TRUE(call("import", {{"type", "model"}, {"path", lhs_ + "::triangle"}, {"source", source.generic_string()}}));
    const auto mesh{nori::resource::get<nori::resource::binary_view>(lhs_ + "::triangle/meshes/mesh-0000")};
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
    ASSERT_TRUE(call("import", {{"type", "model"}, {"path", path}, {"source", NORI_UFBX_AXES_PATH}}));
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
    const auto invalid_model{call("import", {{"type", "model"}, {"path", path}, {"source", binary_path_.generic_string()}})};
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
