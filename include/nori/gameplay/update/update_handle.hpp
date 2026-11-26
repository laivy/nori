#pragma once

#include <cstdint>

namespace nori::gameplay
{
struct update_handle
{
    std::uint64_t scheduler{};
    std::uint64_t id{};

    explicit operator bool() const noexcept
    {
        return scheduler != 0 && id != 0;
    }

    bool operator==(const update_handle&) const = default;
};
} // namespace nori::gameplay
