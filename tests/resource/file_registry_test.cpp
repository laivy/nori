#include <array>
#include <barrier>
#include <filesystem>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include <nori/resource.hpp>

#include "resource_test_helpers.hpp"

namespace
{
class file_registry_test : public resource_test
{
};

TEST_F(file_registry_test, normalized_paths_resolve_to_the_bound_root)
{
    std::filesystem::create_directories(directory_ / "alias");
    const nori::resource::handle root{nori::resource::create_root()};
    EXPECT_TRUE(nori::resource::path_of(root).empty());
    ASSERT_TRUE(nori::resource::save_as(root, "alias/../tree.nori"));
    EXPECT_EQ(nori::resource::path_of(root), bound_resource_path(directory_ / "tree.nori"));
    EXPECT_EQ(nori::resource::try_get("./tree.nori"), root);
    EXPECT_EQ(nori::resource::try_get("alias/../tree.nori"), root);
    nori::resource::remove(root);
    const auto reopened{nori::resource::try_get("tree.nori")};
    ASSERT_TRUE(reopened);
    EXPECT_NE(*reopened, root);
}

TEST_F(file_registry_test, invalid_file_paths_preserve_the_unbound_root)
{
    const nori::resource::handle root{nori::resource::create_root()};
    for (const auto path : {"", "branch", "branch/child", "::branch", "tree.txt", "tree.txt::branch", "tree.nori::branch.nori"})
    {
        const auto result{nori::resource::save_as(root, path)};
        ASSERT_FALSE(result) << path;
        EXPECT_EQ(result.error(), nori::resource::error_code::invalid_path) << path;
        EXPECT_TRUE(nori::resource::path_of(root).empty());
    }
    std::string path{"tree.nori"};
    path.push_back('\0');
    path += ".nori";
    const auto result{nori::resource::save_as(root, path)};
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), nori::resource::error_code::invalid_path);
    EXPECT_TRUE(nori::resource::path_of(root).empty());
    ASSERT_TRUE(nori::resource::save_as(root, "valid.nori"));
    EXPECT_EQ(nori::resource::try_get("valid.nori"), root);
}

TEST_F(file_registry_test, file_identity_distinguishes_aliases_and_missing_paths)
{
    const auto file{directory_ / "tree.nori"};
    const auto alias{directory_ / "alias.nori"};
    const nori::resource::handle root{nori::resource::create_root()};
    const nori::resource::handle other{nori::resource::create_root()};
    ASSERT_TRUE(nori::resource::save_as(root, "tree.nori"));
    ASSERT_TRUE(nori::resource::save_as(other, "other.nori"));
    std::error_code ec;
    std::filesystem::create_hard_link(file, alias, ec);
    ASSERT_FALSE(ec) << ec.message();
    EXPECT_EQ(nori::resource::try_get("alias.nori"), root);
    EXPECT_EQ(nori::resource::try_get("other.nori"), other);
    const auto missing{nori::resource::try_get("missing.nori")};
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error(), nori::resource::error_code::not_found);
    ASSERT_TRUE(std::filesystem::remove(file));
    EXPECT_EQ(nori::resource::try_get("tree.nori"), root);
    const auto reopened_alias{nori::resource::try_get("alias.nori")};
    ASSERT_TRUE(reopened_alias);
    EXPECT_NE(*reopened_alias, root);
}

TEST_F(file_registry_test, save_as_updates_the_same_roots_hard_link_path)
{
    const auto file{directory_ / "tree.nori"};
    const auto alias{directory_ / "alias.nori"};
    const nori::resource::handle root{nori::resource::create_root()};
    ASSERT_TRUE(nori::resource::save_as(root, file));
    std::error_code ec;
    std::filesystem::create_hard_link(file, alias, ec);
    ASSERT_FALSE(ec) << ec.message();

    ASSERT_TRUE(nori::resource::save_as(root, alias));
    EXPECT_EQ(nori::resource::path_of(root), bound_resource_path(alias));
    EXPECT_EQ(nori::resource::try_get(nori::resource::path{alias}), root);
}

TEST_F(file_registry_test, concurrent_first_lookups_share_one_root_across_equivalent_paths)
{
    const nori::resource::handle original{nori::resource::create_root()};
    const nori::resource::handle value{nori::resource::create(original, "value", std::int32_t{42})};
    ASSERT_TRUE(value);
    ASSERT_TRUE(nori::resource::save_as(original, "tree.nori"));
    std::error_code ec;
    std::filesystem::create_hard_link(directory_ / "tree.nori", directory_ / "alias.nori", ec);
    ASSERT_FALSE(ec) << ec.message();
    nori::resource::remove(original);

    constexpr std::size_t worker_count{6};
    std::barrier start{static_cast<std::ptrdiff_t>(worker_count)};
    std::array<nori::resource::result<nori::resource::handle>, worker_count> results;
    std::array<std::thread, worker_count> workers;
    const std::array<std::string, worker_count> paths{
        "tree.nori::value", "./tree.nori::value", "alias.nori::value", "./tree.nori::value", "alias.nori::value", "tree.nori::value"
    };
    for (std::size_t index{0}; index < worker_count; ++index)
    {
        workers[index] = std::thread{[&, index]()
        {
            start.arrive_and_wait();
            results[index] = nori::resource::try_get(paths[index]);
        }};
    }
    for (auto& worker : workers)
    {
        worker.join();
    }

    for (const auto& result : results)
    {
        ASSERT_TRUE(result);
        EXPECT_EQ(*result, *results.front());
        EXPECT_EQ(nori::resource::get<std::int32_t>(*result), 42);
    }
}

TEST_F(file_registry_test, successful_lookup_survives_concurrent_failed_path_candidates)
{
    const nori::resource::handle original{nori::resource::create_root()};
    const nori::resource::handle value{nori::resource::create(original, "value", std::int32_t{17})};
    ASSERT_TRUE(value);
    ASSERT_TRUE(nori::resource::save_as(original, "tree.nori"));
    nori::resource::remove(original);

    constexpr std::size_t worker_count{6};
    std::barrier start{static_cast<std::ptrdiff_t>(worker_count)};
    std::array<nori::resource::result<nori::resource::handle>, worker_count> results;
    std::array<std::thread, worker_count> workers;
    for (std::size_t index{0}; index < worker_count; ++index)
    {
        workers[index] = std::thread{[&, index]()
        {
            start.arrive_and_wait();
            const std::string target{index % 2 == 0 ? "tree.nori::value" : "tree.nori::missing"};
            results[index] = nori::resource::try_get(target);
        }};
    }
    for (auto& worker : workers)
    {
        worker.join();
    }

    nori::resource::handle successful_root{};
    for (std::size_t index{0}; index < worker_count; ++index)
    {
        if (index % 2 == 0)
        {
            ASSERT_TRUE(results[index]);
            if (!successful_root)
            {
                successful_root = nori::resource::root_of(*results[index]);
            }
            EXPECT_EQ(nori::resource::root_of(*results[index]), successful_root);
            EXPECT_EQ(nori::resource::get<std::int32_t>(*results[index]), 17);
        }
        else
        {
            ASSERT_FALSE(results[index]);
            EXPECT_EQ(results[index].error(), nori::resource::error_code::not_found);
        }
    }
    const auto still_bound{nori::resource::try_get("./tree.nori::value")};
    ASSERT_TRUE(still_bound);
    EXPECT_EQ(*still_bound, nori::resource::get("tree.nori::value"));
    EXPECT_EQ(nori::resource::root_of(*still_bound), successful_root);
}

TEST_F(file_registry_test, unrelated_hierarchy_lookup_save_and_removal_can_run_together)
{
    const nori::resource::handle source_root{nori::resource::create_root()};
    const nori::resource::handle source_value{nori::resource::create(source_root, "value", std::int32_t{23})};
    ASSERT_TRUE(source_value);
    ASSERT_TRUE(nori::resource::save_as(source_root, "source.nori"));
    nori::resource::remove(source_root);

    const nori::resource::handle removed_root{nori::resource::create_root()};
    const nori::resource::handle removed_value{nori::resource::create(removed_root, "value", std::int32_t{5})};
    ASSERT_TRUE(removed_value);
    ASSERT_TRUE(nori::resource::save_as(removed_root, "removed.nori"));

    std::barrier start{3};
    nori::resource::result<nori::resource::handle> lookup_result;
    nori::resource::result<void> save_result;
    nori::resource::result<void> unload_result;
    std::thread lookup_worker{[&]()
    {
        start.arrive_and_wait();
        lookup_result = nori::resource::try_get("source.nori");
    }};
    std::thread save_worker{[&]()
    {
        start.arrive_and_wait();
        const nori::resource::handle save_root{nori::resource::create_root()};
        const nori::resource::handle save_value{nori::resource::create(save_root, "value", std::int32_t{31})};
        if (!save_value)
        {
            save_result = std::unexpected{nori::resource::error_code::name_conflict};
            nori::resource::remove(save_root);
            return;
        }
        nori::resource::set(save_value, std::int32_t{32});
        save_result = nori::resource::save_as(save_root, "destination.nori");
        unload_result = nori::resource::unload(save_root);
    }};
    std::thread remove_worker{[&]()
    {
        start.arrive_and_wait();
        nori::resource::remove(removed_root);
    }};
    lookup_worker.join();
    save_worker.join();
    remove_worker.join();

    ASSERT_TRUE(lookup_result);
    EXPECT_EQ(nori::resource::get<std::int32_t>(*lookup_result, "value"), 23);
    EXPECT_TRUE(save_result);
    EXPECT_TRUE(unload_result);
    EXPECT_EQ(nori::resource::get<std::int32_t>("destination.nori::value"), 32);
    const auto reopened_removed{nori::resource::try_get("removed.nori")};
    ASSERT_TRUE(reopened_removed);
    EXPECT_NE(*reopened_removed, removed_root);
}
} // namespace
