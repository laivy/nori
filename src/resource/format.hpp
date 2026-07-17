#pragma once
#include <array>
#include <cstdint>
#include <ios>
#include <string>
#include <variant>
#include <vector>
#include "image.hpp"

namespace nori::resource
{
struct property
{
    using value_type = std::variant<
        std::monostate,
        std::int32_t,
        std::int64_t,
        float,
        std::string,
        image_asset
    >;

    std::string name;
    value_type value;
};

struct file
{
    static constexpr std::array signature{'n', 'o', 'r', 'i'};
    static constexpr std::uint32_t version{1'0'0};

    struct header_data
    {
        std::array<char, 4> signature;
        std::uint32_t version;
    };

    struct hierarchy_entry
    {
        std::streamoff name_pos;
        std::vector<std::uint32_t> child_indices;
    };

    struct chunk_entry
    {
        std::streamoff name_pos;
        std::streamoff value_pos;
    };

    header_data header;
    std::vector<hierarchy_entry> hierarchies;
};
} // namespace nori::resource
