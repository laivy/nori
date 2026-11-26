#pragma once
#include <cstdint>

#include <nori/core/handle.hpp>

namespace nori::gameplay::detail
{
struct object_tag;
}

namespace nori::gameplay
{
struct object_handle
{
    [[nodiscard]] bool operator==(const object_handle&) const noexcept = default;

    std::uint64_t world_id;
    core::basic_handle<detail::object_tag> slot;
};
} // namespace nori::gameplay
