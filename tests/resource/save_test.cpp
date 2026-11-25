#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <nori/core/encoding.hpp>
#include <nori/resource.hpp>

#include "resource_test_helpers.hpp"

namespace
{
TEST_F(save_test, saving_large_values_preserves_views_and_round_trips_the_payloads)
{
    const nori::resource::handle root{nori::resource::create_root()};
    const std::vector<std::byte> image_data(1024 * 1024, std::byte{7});
    const std::vector<std::byte> binary_data(1024 * 1024, std::byte{42});
    const nori::resource::handle icon{nori::resource::create(root, "icon", nori::resource::image{nori::resource::image_format::rgba8_unorm, 512, 512, 1, image_data})};
    const nori::resource::handle payload{nori::resource::create(root, "payload", nori::resource::binary{binary_data})};
    ASSERT_TRUE(icon);
    ASSERT_TRUE(payload);
    const auto icon_before{nori::resource::get<nori::resource::image_view>(icon)};
    const auto payload_before{nori::resource::get<nori::resource::binary_view>(payload)};

    ASSERT_TRUE(nori::resource::save_as(root, "large.nori"));
    ASSERT_TRUE(nori::resource::save(root));
    ASSERT_TRUE(nori::resource::save_as(root, "large-copy.nori"));
    EXPECT_EQ(nori::resource::get<nori::resource::image_view>(icon).data.data(), icon_before.data.data());
    EXPECT_EQ(nori::resource::get<nori::resource::binary_view>(payload).data.data(), payload_before.data.data());
    ASSERT_TRUE(nori::resource::unload(root));

    for (const auto file : {"large.nori", "large-copy.nori"})
    {
        const auto document{nori::resource::get(file)};
        const auto reloaded_icon{nori::resource::get<nori::resource::image_view>(document, "icon")};
        EXPECT_EQ(reloaded_icon.format, nori::resource::image_format::rgba8_unorm);
        EXPECT_EQ(reloaded_icon.width, 512u);
        EXPECT_EQ(reloaded_icon.height, 512u);
        EXPECT_EQ(reloaded_icon.mip_levels, 1u);
        EXPECT_TRUE(std::ranges::equal(reloaded_icon.data, image_data));
        EXPECT_TRUE(std::ranges::equal(nori::resource::get<nori::resource::binary_view>(document, "payload").data, binary_data));
    }
}

TEST_F(save_test, save_as_requires_an_independent_root)
{
    const nori::resource::handle parent{nori::resource::create_root()};
    const nori::resource::handle child{nori::resource::create(parent, "child", nori::resource::empty)};
    ASSERT_TRUE(child);
    const auto original_path{nori::resource::path_of(child)};

    const auto saved{nori::resource::save_as(child, "child.nori")};
    ASSERT_FALSE(saved);
    EXPECT_EQ(saved.error(), nori::resource::error_code::invalid_relationship);
    EXPECT_EQ(nori::resource::parent_of(child), parent);
    EXPECT_EQ(nori::resource::path_of(child), original_path);
    EXPECT_TRUE(std::filesystem::is_empty(directory_));
}

TEST_F(save_test, fully_loaded_document_saves_from_memory)
{
    const nori::resource::handle document{nori::resource::create_root()};
    const nori::resource::handle value{nori::resource::create(document, "value", std::string_view{"preserved"})};
    ASSERT_TRUE(value);
    ASSERT_TRUE(nori::resource::save_as(document, "original.nori"));
    nori::resource::shutdown();
    ASSERT_TRUE(nori::resource::initialize(directory_));

    const auto loaded{nori::resource::get("original.nori")};
    ASSERT_TRUE(nori::resource::load(loaded));
    std::filesystem::resize_file(directory_ / "original.nori", 0);
    ASSERT_TRUE(nori::resource::save_as(loaded, "memory.nori"));
    nori::resource::shutdown();
    ASSERT_TRUE(nori::resource::initialize(directory_));
    EXPECT_EQ(nori::resource::get<std::string>("memory.nori::value"), "preserved");
}

TEST_F(save_test, fully_loaded_copy_survives_either_save_order)
{
    for (const bool source_first : {false, true})
    {
        ASSERT_TRUE(nori::resource::initialize(directory_));
        const nori::resource::handle source{nori::resource::create_root()};
        const nori::resource::handle destination{nori::resource::create_root()};
        const nori::resource::handle branch{nori::resource::create(source, "branch", std::int32_t{7})};
        const nori::resource::handle value{nori::resource::create(branch, "value", std::string_view{"preserved"})};
        ASSERT_TRUE(branch);
        ASSERT_TRUE(value);
        ASSERT_TRUE(nori::resource::save_as(source, "move-source.nori"));
        ASSERT_TRUE(nori::resource::save_as(destination, "move-destination.nori"));
        nori::resource::shutdown();
        ASSERT_TRUE(nori::resource::initialize(directory_));
        const auto loaded_source{nori::resource::get("move-source.nori")};
        const auto loaded_destination{nori::resource::get("move-destination.nori")};
        const auto loaded_branch{nori::resource::get(loaded_source, "branch")};
        ASSERT_TRUE(nori::resource::load(loaded_source));
        ASSERT_TRUE(nori::resource::load(loaded_destination));
        const auto copied_branch{nori::resource::copy(loaded_branch, loaded_destination)};
        ASSERT_TRUE(copied_branch);
        ASSERT_TRUE(nori::resource::save(source_first ? loaded_source : loaded_destination));
        ASSERT_TRUE(nori::resource::save(source_first ? loaded_destination : loaded_source));
        EXPECT_EQ(nori::resource::get<std::string>(loaded_branch, "value"), "preserved");
        EXPECT_EQ(nori::resource::get<std::string>(*copied_branch, "value"), "preserved");
        const auto container{nori::resource::create(loaded_destination, "container", nori::resource::empty)};
        const auto copied{nori::resource::copy(*copied_branch, container)};
        ASSERT_TRUE(copied);
        EXPECT_EQ(nori::resource::root_of(*copied), loaded_destination);
        nori::resource::remove(loaded_destination);
        nori::resource::shutdown();
        ASSERT_TRUE(nori::resource::initialize(directory_));
        EXPECT_EQ(nori::resource::get<std::int32_t>("move-source.nori::branch"), 7);
        EXPECT_EQ(nori::resource::get<std::string>("move-source.nori::branch/value"), "preserved");
        EXPECT_EQ(nori::resource::get<std::int32_t>("move-destination.nori::branch"), 7);
        EXPECT_EQ(nori::resource::get<std::string>("move-destination.nori::branch/value"), "preserved");
    }
}

#ifdef _WIN32
TEST_F(save_test, failed_replacement_preserves_file_binding_and_changes)
{
    const nori::resource::handle document{nori::resource::create_root()};
    const nori::resource::handle value{nori::resource::create(document, "value", std::int32_t{7})};
    ASSERT_TRUE(value);
    ASSERT_TRUE(nori::resource::save_as(document, "readonly.nori"));
    const auto file{directory_ / "readonly.nori"};
    const auto binding{nori::resource::path_of(document)};
    const auto bytes = [&file]()
    {
        std::ifstream input{file, std::ios::binary};
        return std::vector<char>{std::istreambuf_iterator<char>{input}, {}};
    };
    const auto original{bytes()};
    nori::resource::set(value, std::int32_t{99});
    std::error_code ec;
    std::filesystem::permissions(file, std::filesystem::perms::owner_read, ec);
    ASSERT_FALSE(ec);
    const auto failed{nori::resource::save(document)};
    std::filesystem::permissions(file, std::filesystem::perms::owner_all, ec);
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.error(), nori::resource::error_code::io_failure);
    EXPECT_EQ(bytes(), original);
    EXPECT_EQ(nori::resource::path_of(document), binding);
    EXPECT_EQ(nori::resource::get<std::int32_t>(value), 99);
    ASSERT_TRUE(nori::resource::save(document));
    ASSERT_TRUE(nori::resource::unload(document));
    EXPECT_EQ(nori::resource::get<std::int32_t>("readonly.nori::value"), 99);
}
#endif

TEST_F(save_test, reloaded_document_can_be_saved)
{
    nori::resource::handle document{create_resource("source.nori")};
    const nori::resource::handle child{create_resource("source.nori::child")};
    nori::resource::set(child, std::int32_t{42});
    ASSERT_TRUE(nori::resource::save(document));
    const auto read_bytes = [](const std::filesystem::path& file)
    {
        std::ifstream input{file, std::ios::binary};
        return std::vector<char>{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    };
    const std::filesystem::path source{directory_ / "source.nori"};
    const std::filesystem::path destination{directory_ / "destination.nori"};
    write_single_property_file(destination, 1, 0, static_cast<std::uint8_t>(nori::resource::property_type::int32), "untouched", 99);
    const std::vector<char> source_before{read_bytes(source)};
    ASSERT_TRUE(nori::resource::load(document));
    ASSERT_TRUE(nori::resource::save(document));
    ASSERT_TRUE(nori::resource::save_as(document, "destination.nori"));
    EXPECT_EQ(nori::resource::path_of(document), bound_resource_path(destination));
    EXPECT_EQ(read_bytes(source), source_before);
    EXPECT_EQ(read_bytes(destination), source_before);

    EXPECT_EQ(nori::resource::get<std::int32_t>(child), 42);
    ASSERT_TRUE(nori::resource::unload(document));
    document = nori::resource::get("destination.nori");
    ASSERT_TRUE(nori::resource::load(document));
    ASSERT_TRUE(nori::resource::save(document));
    EXPECT_EQ(read_bytes(source), source_before);
    EXPECT_EQ(read_bytes(destination), source_before);
    EXPECT_EQ(nori::resource::try_get("destination.nori"), document);
    EXPECT_EQ(nori::resource::get<std::int32_t>("destination.nori::child"), 42);
    for (const auto& entry : std::filesystem::directory_iterator{directory_})
    {
        EXPECT_NE(entry.path().extension(), ".tmp");
    }
}

TEST_F(save_test, string_deduplication)
{
    const nori::resource::handle root{create_resource("interned.nori")};
    const nori::resource::handle same{create_resource("interned.nori::same")};
    nori::resource::set(same, std::string{"same"});
    const nori::resource::handle first{create_resource("interned.nori::first")};
    nori::resource::set(first, std::string{"later"});
    const nori::resource::handle later{create_resource("interned.nori::later")};
    nori::resource::set(later, std::string{"later"});
    ASSERT_TRUE(nori::resource::save(root));

    std::ifstream in{directory_ / "interned.nori", std::ios::binary};
    ASSERT_TRUE(in);
    in.seekg(4 + sizeof(std::uint32_t));
    ASSERT_EQ(read_value<std::uint32_t>(in), 3u);
    in.seekg(40);
    const auto pool{read_value<std::uint64_t>(in)};
    in.seekg(56);

    struct offsets
    {
        std::uint32_t name;
        std::uint64_t value;
    };

    std::array<offsets, 3> entries;
    for (offsets& entry : entries)
    {
        entry.name = read_value<std::uint32_t>(in);
        read_value<std::uint32_t>(in);
        read_value<std::uint32_t>(in);
        entry.value = read_value<std::uint64_t>(in);
        read_value<std::uint64_t>(in);
    }

    EXPECT_EQ(entries[0].name, 0u);
    EXPECT_EQ(entries[1].name, 1u);
    EXPECT_EQ(entries[2].name, 2u);
    EXPECT_EQ(entries[0].value, 0u);
    EXPECT_EQ(entries[1].value, entries[2].value);
    in.seekg(static_cast<std::streamoff>(pool));
    std::string values(9, '\0');
    in.read(values.data(), values.size());
    EXPECT_EQ(values, "samelater");
}

TEST_F(save_test, child_order)
{
    const nori::resource::handle parent{create_resource("scalar.nori::parent")};
    const nori::resource::handle first{create_resource("scalar.nori::parent/first")};
    const nori::resource::handle second{create_resource("scalar.nori::parent/second")};
    nori::resource::set(parent, std::int32_t{7});
    nori::resource::set(first, std::string{"first"});
    nori::resource::set(second, std::string{"second"});
    ASSERT_TRUE(nori::resource::reorder(second, 0));

    ASSERT_TRUE(nori::resource::save_as(nori::resource::get("scalar.nori"), directory_ / "scalar.nori"));
    nori::resource::shutdown();
    ASSERT_TRUE(nori::resource::initialize(directory_));

    const auto loaded_parent{nori::resource::try_get("scalar.nori::parent")};
    ASSERT_TRUE(loaded_parent);
    EXPECT_EQ(nori::resource::get<std::int32_t>(*loaded_parent), 7);
    const std::vector<nori::resource::handle> loaded_children{children_of_or_fail(*loaded_parent)};
    ASSERT_EQ(loaded_children.size(), 2);
    EXPECT_EQ(nori::resource::name_of(loaded_children[0]), "second");
    EXPECT_EQ(nori::resource::name_of(loaded_children[1]), "first");
    EXPECT_EQ(nori::resource::get<std::string>(loaded_children[0]), "second");
}

TEST_F(save_test, save_as_path)
{
    const nori::resource::handle created_root{nori::resource::create_root()};
    const nori::resource::handle created_folder{nori::resource::create(created_root, "folder", nori::resource::empty)};
    const nori::resource::handle created_value{nori::resource::create(created_folder, "value", std::int32_t{42})};
    ASSERT_TRUE(created_root);
    ASSERT_TRUE(created_folder);
    ASSERT_TRUE(created_value);
    const nori::resource::handle root{created_root};
    const nori::resource::handle value{created_value};
    EXPECT_TRUE(nori::resource::path_of(root).empty());
    EXPECT_EQ(nori::resource::path_of(value).string(), "folder/value");

    const std::filesystem::path destination{directory_ / "after.nori"};
    const std::string destination_path{nori::core::path_to_utf8(destination)};
    ASSERT_TRUE(nori::resource::save_as(root, destination));
    EXPECT_TRUE(std::filesystem::exists(destination));
    const auto reloaded_value{nori::resource::try_get("after.nori::folder/value")};
    ASSERT_TRUE(reloaded_value);
    EXPECT_EQ(*reloaded_value, value);
    EXPECT_EQ(nori::resource::get<std::int32_t>(value), 42);
    EXPECT_EQ(nori::resource::path_of(root), bound_resource_path(destination));
    EXPECT_EQ(nori::resource::path_of(value), bound_resource_path(destination, "folder/value"));
    EXPECT_EQ(nori::resource::get("after.nori"), root);
    EXPECT_EQ(nori::resource::get(destination_path), root);
    EXPECT_EQ(nori::resource::get("after.nori::folder"), created_folder);
    EXPECT_EQ(nori::resource::get(destination_path + "::folder/value"), value);
    EXPECT_EQ(nori::resource::get<std::int32_t>(value), 42);
    EXPECT_EQ(nori::resource::try_get("before.nori").error(), nori::resource::error_code::not_found);
    EXPECT_EQ(nori::resource::try_get("before.nori::folder/value").error(), nori::resource::error_code::not_found);

    nori::resource::set(value, std::int32_t{84});
    ASSERT_TRUE(nori::resource::save(root));
    nori::resource::shutdown();
    ASSERT_TRUE(nori::resource::initialize(directory_));
    EXPECT_EQ(nori::resource::get<std::int32_t>("after.nori::folder/value"), 84);
}

TEST_F(save_test, save_as_name_conflict)
{
    const nori::resource::handle root{create_resource("source.nori")};
    const nori::resource::handle child{create_resource("source.nori::child")};
    const nori::resource::handle conflict{create_resource("conflict.nori")};
    const auto conflict_result{nori::resource::save_as(root, "conflict.nori")};
    ASSERT_FALSE(conflict_result);
    EXPECT_EQ(conflict_result.error(), nori::resource::error_code::name_conflict);
    EXPECT_TRUE(std::filesystem::exists(directory_ / "conflict.nori"));

    EXPECT_EQ(nori::resource::get("source.nori"), root);
    EXPECT_EQ(nori::resource::get("source.nori::child"), child);
    EXPECT_EQ(nori::resource::get("conflict.nori"), conflict);
}

TEST_F(save_test, save_as_failure)
{
    const nori::resource::handle root{create_resource("source.nori")};
    const nori::resource::handle child{create_resource("source.nori::child")};
    nori::resource::set(child, std::int32_t{42});

    const auto missing_result{nori::resource::save_as(root, "missing/after.nori")};
    ASSERT_FALSE(missing_result);
    EXPECT_EQ(missing_result.error(), nori::resource::error_code::io_failure);
    EXPECT_EQ(nori::resource::get<std::int32_t>(child), 42);
    EXPECT_EQ(nori::resource::get("source.nori"), root);
    EXPECT_EQ(nori::resource::get("source.nori::child"), child);
    EXPECT_EQ(nori::resource::try_get("missing/after.nori").error(), nori::resource::error_code::not_found);

    const std::filesystem::path destination{directory_ / "blocked.nori"};
    std::filesystem::create_directory(destination);
    const auto blocked_result{nori::resource::save_as(root, "blocked.nori")};
    ASSERT_FALSE(blocked_result);
    EXPECT_EQ(blocked_result.error(), nori::resource::error_code::io_failure);
    EXPECT_TRUE(std::filesystem::is_directory(destination));
    EXPECT_EQ(nori::resource::get("source.nori"), root);
    EXPECT_EQ(nori::resource::get("source.nori::child"), child);

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator{directory_})
    {
        EXPECT_NE(entry.path().extension(), ".tmp");
    }
}
} // namespace
