#pragma once
#include <cstdint>
#include <functional>

namespace nori::core
{
template <typename Tag>
struct handle
{
    constexpr bool operator==(const handle& other) const
    {
        return index == other.index && generation == other.generation;
    }

    constexpr operator bool() const
    {
        return index != 0;
    }

    std::uint32_t index{};
    std::uint32_t generation{};
};
} // namespace nori::core

namespace std
{
template <typename Tag>
struct hash<nori::core::handle<Tag>>
{
    std::size_t operator()(nori::core::handle<Tag> handle) const noexcept
    {
        return (static_cast<std::size_t>(handle.index) << 32) ^ handle.generation;
    }
};
} // namespace std
