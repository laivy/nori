#include <array>
#include <expected>
#include <string_view>

#include "command.hpp"

namespace
{
struct entry
{
    std::string_view name;
    nori::cli::result<nlohmann::ordered_json> (*execute)(const nlohmann::json&);
};

template <auto Parse, auto Execute>
nori::cli::result<nlohmann::ordered_json> run(const nlohmann::json& params)
{
    const auto args{Parse(params)};
    if (!args)
    {
        return std::unexpected{nori::cli::error_code::invalid_argument};
    }
    return Execute(*args);
}

constexpr auto entries{std::to_array<entry>({
    // 조회
    {"diff", run<&nori::cli::command::parse_diff_args, &nori::cli::command::diff>},
    {"exists", run<&nori::cli::command::parse_exists_args, &nori::cli::command::exists>},
    {"get", run<&nori::cli::command::parse_get_args, &nori::cli::command::get>},
    {"tree", run<&nori::cli::command::parse_tree_args, &nori::cli::command::tree>},

    // 수정
    {"import", run<&nori::cli::command::parse_import_args, &nori::cli::command::import>},
    {"set", run<&nori::cli::command::parse_set_args, &nori::cli::command::set>},

    // 구조
    {"copy", run<&nori::cli::command::parse_copy_args, &nori::cli::command::copy>},
    {"move", run<&nori::cli::command::parse_move_args, &nori::cli::command::move>},
    {"rename", run<&nori::cli::command::parse_rename_args, &nori::cli::command::rename>},
    {"reorder", run<&nori::cli::command::parse_reorder_args, &nori::cli::command::reorder>},

    // 삭제
    {"del", run<&nori::cli::command::parse_del_args, &nori::cli::command::del>},
})};
} // namespace

namespace nori::cli::command
{
result<nlohmann::ordered_json> dispatch(std::string_view method, const nlohmann::json& params)
{
    const auto it{std::ranges::find(entries, method, &entry::name)};
    if (it == entries.end())
    {
        return std::unexpected{error_code::unknown_method};
    }
    return it->execute(params);
}
} // namespace nori::cli::command
