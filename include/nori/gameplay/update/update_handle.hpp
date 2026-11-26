#pragma once
#include <cstdint>

namespace nori::gameplay
{
struct update_handle
{
    [[nodiscard]] constexpr bool operator==(const update_handle&) const noexcept = default;

    explicit constexpr operator bool() const noexcept
    {
        return scheduler != 0 && id != 0;
    }

    std::uint64_t scheduler;
    std::uint64_t id;
};
} // namespace nori::gameplay
