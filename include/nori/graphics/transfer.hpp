#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

#include <nori/core/handle.hpp>
#include <nori/graphics/texture.hpp>

namespace nori::graphics::detail
{
struct readback_tag;

class readback_deleter
{
public:
    void operator()(core::basic_handle<readback_tag> handle) const noexcept;
};
} // namespace nori::graphics::detail

namespace nori::graphics
{
using readback_handle = core::basic_handle<detail::readback_tag>;
using unique_readback_handle = core::unique_handle<readback_handle, detail::readback_deleter>;

enum class blit_filter
{
    point,
    linear
};

struct texture_readback
{
    texture_format format;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t row_pitch;
    std::vector<std::byte> data;
};

enum class readback_status
{
    pending,
    ready,
    failed
};

struct readback_result
{
    readback_status status;
    texture_readback value;
};
} // namespace nori::graphics
