#pragma once
#include <cstdint>

#include <nori/core/handle.hpp>

namespace nori::graphics::detail
{
struct sampler_tag;

class sampler_deleter
{
public:
    void operator()(core::basic_handle<sampler_tag> handle) const noexcept;
};
} // namespace nori::graphics::detail

namespace nori::graphics
{
using sampler_handle = core::basic_handle<detail::sampler_tag>;
using unique_sampler_handle = core::unique_handle<sampler_handle, detail::sampler_deleter>;

enum class sampler_filter
{
    point,
    linear,
    anisotropic
};

enum class sampler_address_mode
{
    wrap,
    mirror,
    clamp,
    border
};

struct sampler_desc
{
    sampler_filter min_filter;
    sampler_filter mag_filter;
    sampler_filter mip_filter;
    sampler_address_mode address_u;
    sampler_address_mode address_v;
    sampler_address_mode address_w;
    std::uint32_t maximum_anisotropy;
    float minimum_lod;
    float maximum_lod;
};
} // namespace nori::graphics
