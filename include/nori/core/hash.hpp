#pragma once
#include <cstddef>

namespace nori::core
{
constexpr void hash_combine(std::size_t& seed, std::size_t value) noexcept
{
    seed ^= value + 0x9E3779B97F4A7C15ull + (seed << 6) + (seed >> 2);
}
} // namespace nori::core
