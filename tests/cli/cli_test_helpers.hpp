#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <nori/resource.hpp>

#include "protocol.hpp"

namespace
{
inline std::vector<nori::resource::handle> children_of_or_fail(nori::resource::handle target)
{
    auto res{nori::resource::children_of(target)};
    EXPECT_TRUE(res) << (res ? "" : nori::resource::to_string(res.error()));
    return res ? std::move(*res) : std::vector<nori::resource::handle>{};
}

class cli_test_fixture : public testing::Test
{
protected:
    cli_test_fixture() :
        lhs_path_{"nori-cli-command-lhs.nori"},
        rhs_path_{"nori-cli-command-rhs.nori"},
        binary_path_{"nori-cli-command-input.bin"},
        image_path_{"nori-cli-command-input.png"},
        shader_path_{"nori-cli-command-input.vs.hlsl"},
        lhs_{lhs_path_.generic_string()},
        rhs_{rhs_path_.generic_string()}
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
        original_path_ = std::filesystem::current_path();
        workdir_ = std::filesystem::temp_directory_path() / "nori-cli-tests" / name;
        std::error_code ec;
        std::filesystem::remove_all(workdir_, ec);
        std::filesystem::create_directories(workdir_, ec);
        std::filesystem::current_path(workdir_, ec);
        ASSERT_TRUE(nori::resource::initialize());
    }

    void TearDown() override
    {
        nori::resource::shutdown();
        std::error_code error;
        std::filesystem::current_path(original_path_, error);
        std::filesystem::remove_all(workdir_, error);
    }

    nori::cli::result<nlohmann::ordered_json> call(std::string_view method, nlohmann::json params)
    {
        return nori::cli::execute(nlohmann::json{{"method", method}, {"params", std::move(params)}});
    }

    std::filesystem::path lhs_path_;
    std::filesystem::path rhs_path_;
    std::filesystem::path binary_path_;
    std::filesystem::path image_path_;
    std::filesystem::path shader_path_;
    std::string lhs_;
    std::string rhs_;
    std::filesystem::path original_path_;
    std::filesystem::path workdir_;
};

inline void write_bytes(const std::filesystem::path& path, const std::vector<unsigned char>& bytes)
{
    std::ofstream output{path, std::ios::binary};
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}
} // namespace
