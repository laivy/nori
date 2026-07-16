#pragma once
#include <span>
#include <vector>
#include <nori/core/types.hpp>
#include <nori/resource/handle.hpp>
#include <nori/graphics/pipeline.hpp>
#include <nori/graphics/render_target.hpp>

namespace nori::graphics
{
struct transform
{
    core::float3 position;
    core::float3 rotation;
    core::float3 scale;
};

struct model
{
    resource::handle resource;
    transform transform;
};

struct quad
{
    core::float2 position;
    core::float2 size;
    core::float4 color;
    resource::handle texture;
};

struct fullscreen_draw
{
    pipeline_handle pipeline;
    render_texture_handle source;
};

class draw_list
{
public:
    void add(const model& value);
    void add(const quad& value);
    void add(const fullscreen_draw& value);
    void clear();

    [[nodiscard]]
    std::span<const model> models() const noexcept;

    [[nodiscard]]
    std::span<const quad> quads() const noexcept;

    [[nodiscard]]
    std::span<const fullscreen_draw> fullscreen_draws() const noexcept;

private:
    std::vector<model> models_;
    std::vector<quad> quads_;
    std::vector<fullscreen_draw> fullscreen_draws_;
};
} // namespace nori::graphics
