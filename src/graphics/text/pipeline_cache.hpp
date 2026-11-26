#pragma once
#include <memory>
#include <optional>
#include <vector>

#include <nori/graphics/pipeline.hpp>
#include <nori/graphics/sampler.hpp>

#include "resource/render_target_signature.hpp"

namespace nori::graphics::resource
{
class pipeline_manager;
class sampler_manager;
} // namespace nori::graphics::resource

namespace nori::graphics::text
{
struct render_state
{
    pipeline_handle pipeline;
    sampler_handle sampler;
};

class pipeline_cache
{
public:
    ~pipeline_cache();

    pipeline_cache(const pipeline_cache&) = delete;
    pipeline_cache& operator=(const pipeline_cache&) = delete;

    static std::unique_ptr<pipeline_cache> create(resource::pipeline_manager* pipelines, resource::sampler_manager* samplers);

    std::optional<render_state> prepare(const resource::render_target_signature& target, shader_handle vertex_shader = {}, shader_handle pixel_shader = {});
    void invalidate(shader_handle shader);

private:
    struct entry
    {
        resource::render_target_signature signature;
        shader_handle vertex_shader;
        shader_handle pixel_shader;
        pipeline_handle pipeline;
    };

    pipeline_cache(resource::pipeline_manager* pipelines, resource::sampler_manager* samplers, shader_handle vertex_shader, shader_handle pixel_shader, sampler_handle sampler);

    resource::pipeline_manager* const pipelines_;
    resource::sampler_manager* const samplers_;
    std::vector<entry> entries_;
    shader_handle vertex_shader_;
    shader_handle pixel_shader_;
    sampler_handle sampler_;
};
} // namespace nori::graphics::text
