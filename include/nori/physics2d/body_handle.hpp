#pragma once

#include <cstdint>

#include <nori/core/handle.hpp>

namespace nori::physics2d::detail
{
struct body_tag;
}

namespace nori::physics2d
{
struct body_handle
{
    [[nodiscard]] bool operator==(const body_handle&) const noexcept = default;

    std::uint64_t scene_id;
    core::basic_handle<detail::body_tag> slot;
};
}
