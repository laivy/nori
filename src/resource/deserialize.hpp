#pragma once
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <nori/resource/error_code.hpp>
#include "format.hpp"

namespace nori::resource
{
struct deserialized_data
{
    std::vector<property> properties;
    std::vector<std::uint32_t> top_level_prop_indices;
    std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> child_indices;
};

result<deserialized_data> deserialize(std::string_view path);
} // namespace nori::resource
