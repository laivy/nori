#include <array>
#include <utility>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <nori/resource.hpp>

#include "command/common.hpp"
#include "protocol.hpp"

namespace
{
TEST(protocol_test, response)
{
    nlohmann::ordered_json success(nori::cli::make_response(nlohmann::ordered_json{{"type", "string"}, {"value", "hero"}}));
    EXPECT_EQ(success.dump(), R"({"result":{"type":"string","value":"hero"}})");

    nlohmann::ordered_json failure(nori::cli::make_response(std::unexpected{nori::cli::error_code::not_found}));
    EXPECT_EQ(failure.dump(), R"({"error":{"code":"not_found"}})");
}

TEST(protocol_test, error_mapping)
{
    constexpr std::array resource_mappings{
        std::pair{nori::resource::error_code::not_found, nori::cli::error_code::not_found},
        std::pair{nori::resource::error_code::invalid_path, nori::cli::error_code::invalid_argument},
        std::pair{nori::resource::error_code::type_mismatch, nori::cli::error_code::type_mismatch},
        std::pair{nori::resource::error_code::io_failure, nori::cli::error_code::io_error},
        std::pair{nori::resource::error_code::invalid_format, nori::cli::error_code::invalid_format},
        std::pair{nori::resource::error_code::name_conflict, nori::cli::error_code::already_exists},
        std::pair{nori::resource::error_code::invalid_relationship, nori::cli::error_code::invalid_argument},
    };
    for (const auto& [resource_error, cli_error] : resource_mappings)
    {
        SCOPED_TRACE(nori::resource::to_string(resource_error));
        EXPECT_EQ(nori::cli::command::to_error_code(resource_error), cli_error);
    }
}

TEST(protocol_test, unknown_method)
{
    const auto unknown{nori::cli::execute(nlohmann::json{{"method", "bogus"}, {"params", nlohmann::json::object()}})};
    ASSERT_FALSE(unknown);
    EXPECT_EQ(unknown.error(), nori::cli::error_code::unknown_method);
}
} // namespace
