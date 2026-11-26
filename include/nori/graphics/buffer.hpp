#pragma once
#include <cstdint>
#include <utility>

#include <nori/core/handle.hpp>

namespace nori::graphics::detail
{
struct buffer_tag;

class buffer_deleter
{
public:
    void operator()(core::basic_handle<buffer_tag> handle) const noexcept;
};
} // namespace nori::graphics::detail

namespace nori::graphics
{
using buffer_handle = core::basic_handle<detail::buffer_tag>;
using unique_buffer_handle = core::unique_handle<buffer_handle, detail::buffer_deleter>;

enum class buffer_usage : std::uint32_t
{
    none = 0,
    vertex = 1 << 0,
    index = 1 << 1,
    constant = 1 << 2,
    copy_source = 1 << 3,
    copy_destination = 1 << 4
};

constexpr buffer_usage operator|(buffer_usage lhs, buffer_usage rhs) noexcept
{
    return buffer_usage{std::to_underlying(lhs) | std::to_underlying(rhs)};
}

constexpr buffer_usage operator&(buffer_usage lhs, buffer_usage rhs) noexcept
{
    return buffer_usage{std::to_underlying(lhs) & std::to_underlying(rhs)};
}

enum class buffer_access
{
    gpu_only,
    cpu_write
};

struct buffer_desc
{
    std::uint64_t size;
    buffer_usage usage;
    buffer_access access;
};

// begin_frame, end_frame 사이에서만 유효한 버퍼
struct frame_upload_buffer_view
{
    buffer_handle buffer;
    std::uint64_t offset;
    std::uint64_t size;
};
} // namespace nori::graphics
