#pragma once
#include <algorithm>
#include <cstddef>
#include <span>
#include <vector>

namespace nori::resource
{
struct binary
{
    constexpr bool operator==(const binary&) const = default;

    std::vector<std::byte> data;
};

struct binary_view
{
    constexpr bool operator==(const binary_view& rhs) const noexcept
    {
        return std::ranges::equal(data, rhs.data);
    }

    std::span<const std::byte> data;
};
} // namespace nori::resource
