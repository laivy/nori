#pragma once

#include <concepts>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>
#include <variant>

#include <gtest/gtest.h>

#include <nori/resource.hpp>

#include "document_manager.hpp"
#include "document_workspace.hpp"
#include "exit_workflow.hpp"
#include "log.hpp"
#include "preview.hpp"

namespace
{
inline std::vector<nori::resource::handle> children_of_or_fail(nori::resource::handle target)
{
    auto res{nori::resource::children_of(target)};
    EXPECT_TRUE(res) << (res ? "" : nori::resource::to_string(res.error()));
    return res ? std::move(*res) : std::vector<nori::resource::handle>{};
}

using nori::editor::command;
using nori::editor::document_editor;
using nori::editor::document_manager;
using nori::editor::hierarchy_state;
using nori::editor::inspector_state;
using nori::editor::log;
using nori::editor::preview;
using nori::editor::preview_state;
using nori::editor::selection_manager;
static_assert(std::same_as<decltype(std::declval<const document_editor&>().selection()), const selection_manager&>);
static_assert(std::same_as<decltype(std::declval<const document_manager&>().documents()), std::span<const document_editor>>);
static_assert(std::same_as<decltype(std::declval<const document_manager&>().find_root(nori::resource::handle{})), const document_editor*>);
static_assert(std::same_as<decltype(std::declval<const document_manager&>().find(nori::resource::handle{})), const document_editor*>);
static_assert(std::same_as<decltype(std::declval<const document_manager&>().active()), const document_editor*>);

nori::resource::result<nori::resource::handle> create_relative(nori::resource::handle parent, std::string_view subpath)
{
    const nori::resource::path target{subpath};
    if (target.has_file_path())
    {
        return std::unexpected{nori::resource::error_code::invalid_path};
    }

    nori::resource::handle current{parent};
    for (const std::string_view name : nori::resource::subpath_iterator{target})
    {
        if (const auto res{nori::resource::try_get(current, name)})
        {
            current = *res;
            continue;
        }
        else if (res.error() != nori::resource::error_code::not_found)
        {
            return std::unexpected{res.error()};
        }

        const nori::resource::handle created{nori::resource::create(current, name, nori::resource::empty)};
        if (!created)
        {
            return std::unexpected{nori::resource::error_code::name_conflict};
        }
        current = created;
    }
    return current;
}

class noritor_test_fixture : public testing::Test
{
protected:
    noritor_test_fixture() :
        base_directory_{std::filesystem::temp_directory_path() / "noritor-command-tests"}
    {
    }

    void SetUp() override
    {
        const ::testing::TestInfo* info{::testing::UnitTest::GetInstance()->current_test_info()};
        std::string name{info != nullptr ? std::string{info->test_suite_name()} + "." + info->name() : "unknown"};
        for (char& c : name)
        {
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_'))
            {
                c = '_';
            }
        }
        directory_ = base_directory_ / name;
        original_current_path_ = std::filesystem::current_path();
        std::error_code ec;
        std::filesystem::remove_all(directory_, ec);
        std::filesystem::create_directories(directory_, ec);
        ASSERT_TRUE(nori::resource::initialize(directory_));
        root_ = nori::resource::create_root();
        log::clear();
    }

    void TearDown() override
    {
        log::clear();
        nori::resource::shutdown();
        std::error_code ec;
        std::filesystem::current_path(original_current_path_, ec);
        std::filesystem::remove_all(directory_, ec);
    }

    template <typename T>
    nori::resource::handle create_value(std::string_view path, T value)
    {
        const auto created{create_relative(root_, path)};
        if (!created)
        {
            ADD_FAILURE() << "create_relative failed: " << nori::resource::to_string(created.error());
            return {};
        }
        if constexpr (std::same_as<std::remove_cvref_t<T>, std::monostate>)
        {
            nori::resource::set(*created, nori::resource::empty);
        }
        else
        {
            nori::resource::set(*created, value);
        }
        return *created;
    }

    nori::resource::handle create_integer(std::string_view name, std::int32_t value)
    {
        return create_value(name, value);
    }

    std::int32_t integer(std::string_view name)
    {
        return nori::resource::try_get<std::int32_t>(root_, name).value();
    }

    nori::resource::handle property(std::string_view name)
    {
        return nori::resource::try_get(root_, name).value();
    }

    std::filesystem::path base_directory_;
    std::filesystem::path directory_;
    std::filesystem::path original_current_path_;
    nori::resource::handle root_;
};
} // namespace
