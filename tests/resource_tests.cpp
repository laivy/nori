#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <gtest/gtest.h>
#include <nori/resource.hpp>

namespace
{
template <typename T>
void write_value(std::ostream& out, const T& value)
{
    out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

void write_single_property_file(
    const std::filesystem::path& path,
    std::uint32_t version,
    std::span<const std::uint32_t> children,
    std::uint8_t value_type
)
{
    std::ofstream out{path, std::ios::binary};
    constexpr std::array signature{'n', 'o', 'r', 'i'};
    out.write(signature.data(), signature.size());
    write_value(out, version);
    write_value(out, std::uint32_t{1});

    const std::streamoff binary_begin{
        static_cast<std::streamoff>(signature.size() + sizeof(version) + sizeof(std::uint32_t) +
                                    sizeof(std::streamoff) + sizeof(std::uint32_t) +
                                    sizeof(std::uint32_t) * children.size() + sizeof(std::streamoff) * 2)
    };
    constexpr std::string_view name{"value"};
    const std::streamoff value_position{
        binary_begin + static_cast<std::streamoff>(sizeof(std::uint8_t) + sizeof(std::uint32_t) + name.size())
    };
    write_value(out, binary_begin);
    write_value(out, static_cast<std::uint32_t>(children.size()));
    out.write(
        reinterpret_cast<const char*>(children.data()),
        static_cast<std::streamsize>(children.size_bytes())
    );
    write_value(out, binary_begin);
    write_value(out, value_position);
    write_value(out, std::uint8_t{4});
    write_value(out, static_cast<std::uint32_t>(name.size()));
    out.write(name.data(), static_cast<std::streamsize>(name.size()));
    write_value(out, value_type);
}

class resource_test : public testing::Test
{
protected:
    resource_test() :
        directory_{std::filesystem::temp_directory_path() / "nori-resource-tests"},
        runtime_{nori::resource::runtime::specification{.mount_path = directory_}}
    {
        std::filesystem::create_directories(directory_);
    }

    ~resource_test() override
    {
        std::error_code error;
        std::filesystem::remove_all(directory_, error);
    }

    std::filesystem::path directory_;
    nori::resource::runtime runtime_;
};

TEST_F(resource_test, query_and_typed_getter_contracts)
{
    const nori::resource::handle root{nori::resource::create("values.nori")};
    const nori::resource::handle value{nori::resource::create("values.nori/count")};
    ASSERT_TRUE(root);
    ASSERT_TRUE(value);

    nori::resource::set_value(value, std::int32_t{42});

    EXPECT_EQ(nori::resource::get_int32(value), 42);
    EXPECT_EQ(nori::resource::get_int32("values.nori/count"), 42);
    EXPECT_FALSE(nori::resource::get_int32("values.nori/missing"));
    EXPECT_FALSE(nori::resource::get("values.nori/missing"));
    EXPECT_FALSE(nori::resource::get_parent(root));
    EXPECT_TRUE(nori::resource::get_children(value).empty());
}

TEST_F(resource_test, relationship_failures_are_reported_without_mutation)
{
    const nori::resource::handle root{nori::resource::create("tree.nori")};
    const nori::resource::handle child{nori::resource::create("tree.nori/child")};
    const nori::resource::handle sibling{nori::resource::create("tree.nori/sibling")};
    const nori::resource::handle other{nori::resource::create("other.nori")};
    const nori::resource::handle conflicting{nori::resource::create("other.nori/child")};

    const auto rename_result{nori::resource::set_name(child, "sibling")};
    ASSERT_FALSE(rename_result);
    EXPECT_EQ(rename_result.error(), nori::resource::error_code::name_conflict);
    EXPECT_EQ(nori::resource::get("tree.nori/child"), child);

    const auto cycle_result{nori::resource::set_parent(root, child)};
    ASSERT_FALSE(cycle_result);
    EXPECT_EQ(cycle_result.error(), nori::resource::error_code::invalid_relationship);

    const auto move_result{nori::resource::set_parent(conflicting, root)};
    ASSERT_FALSE(move_result);
    EXPECT_EQ(move_result.error(), nori::resource::error_code::name_conflict);
    EXPECT_EQ(nori::resource::get_parent(conflicting), other);
    EXPECT_EQ(nori::resource::get_parent(sibling), root);
}

TEST(resource_round_trip, serializes_and_loads_values)
{
    const std::filesystem::path directory{std::filesystem::temp_directory_path() / "nori-resource-round-trip"};
    const std::filesystem::path file{directory / "round-trip.nori"};
    std::filesystem::create_directories(directory);
    {
        nori::resource::runtime runtime{nori::resource::runtime::specification{}};
        const std::string root_path{file.generic_string()};
        const nori::resource::handle root{nori::resource::create(root_path)};
        const nori::resource::handle value{nori::resource::create(root_path + "/message")};
        nori::resource::set_value(value, std::string{"hello"});
        ASSERT_TRUE(nori::resource::serialize(file, root));
    }
    {
        nori::resource::runtime runtime{nori::resource::runtime::specification{}};
        const nori::resource::handle value{nori::resource::get(file.generic_string() + "/message")};
        ASSERT_TRUE(value);
        EXPECT_EQ(nori::resource::get_string(value), "hello");
    }
    std::error_code error;
    std::filesystem::remove_all(directory, error);
}

TEST_F(resource_test, reports_io_failure_and_hides_invalid_files_from_queries)
{
    const nori::resource::handle root{nori::resource::create("broken.nori")};
    const auto save_result{nori::resource::serialize(directory_ / "missing" / "broken.nori", root)};
    ASSERT_FALSE(save_result);
    EXPECT_EQ(save_result.error(), nori::resource::error_code::io_failure);

    const std::filesystem::path invalid_file{directory_ / "invalid.nori"};
    {
        std::ofstream out{invalid_file, std::ios::binary};
        constexpr std::array invalid_signature{'b', 'a', 'd', '!'};
        out.write(invalid_signature.data(), invalid_signature.size());
        const std::uint32_t version{100};
        out.write(reinterpret_cast<const char*>(&version), sizeof(version));
    }
    EXPECT_FALSE(nori::resource::get("invalid.nori"));

    const std::filesystem::path truncated_file{directory_ / "truncated.nori"};
    {
        std::ofstream out{truncated_file, std::ios::binary};
        constexpr std::array signature{'n', 'o', 'r', 'i'};
        out.write(signature.data(), signature.size());
    }
    EXPECT_FALSE(nori::resource::get("truncated.nori"));

    write_single_property_file(directory_ / "version.nori", 999, {}, 0);
    EXPECT_FALSE(nori::resource::get("version.nori"));

    constexpr std::array invalid_child{std::uint32_t{7}};
    write_single_property_file(directory_ / "index.nori", 100, invalid_child, 0);
    EXPECT_FALSE(nori::resource::get("index.nori"));

    constexpr std::array cyclic_child{std::uint32_t{0}};
    write_single_property_file(directory_ / "cycle.nori", 100, cyclic_child, 0);
    EXPECT_FALSE(nori::resource::get("cycle.nori"));

    write_single_property_file(directory_ / "type.nori", 100, {}, 255);
    EXPECT_FALSE(nori::resource::get("type.nori"));
}

#ifndef NDEBUG
TEST(resource_contracts, assert_on_runtime_and_handle_contract_violations)
{
    EXPECT_DEATH((void)nori::resource::create("missing-runtime.nori"), "");
    EXPECT_DEATH(
        {
            nori::resource::runtime first{nori::resource::runtime::specification{}};
            nori::resource::runtime second{nori::resource::runtime::specification{}};
        },
        ""
    );
    EXPECT_DEATH(
        {
            nori::resource::runtime runtime{nori::resource::runtime::specification{}};
            (void)nori::resource::get_name({});
        },
        ""
    );
    EXPECT_DEATH(
        {
            nori::resource::runtime runtime{nori::resource::runtime::specification{}};
            const nori::resource::handle value{nori::resource::create("type.nori/value")};
            nori::resource::set_value(value, std::int32_t{1});
            (void)nori::resource::get_string(value);
        },
        ""
    );
}
#endif
} // namespace
