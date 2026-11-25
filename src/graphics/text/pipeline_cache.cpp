#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

#include "resource/pipeline_manager.hpp"
#include "resource/sampler_manager.hpp"
#include "shader/text_ps.hpp"
#include "shader/text_vs.hpp"
#include "text/layout.hpp"
#include "text/pipeline_cache.hpp"

namespace nori::graphics::text
{
pipeline_cache::~pipeline_cache()
{
    for (const auto& e : entries_)
    {
        pipelines_->destroy(e.pipeline);
    }
    pipelines_->destroy(vertex_shader_);
    pipelines_->destroy(pixel_shader_);
    samplers_->destroy(sampler_);
}

std::unique_ptr<pipeline_cache> pipeline_cache::create(resource::pipeline_manager* pipelines, resource::sampler_manager* samplers)
{
    const shader_handle vertex_shader{pipelines->create_shader(std::as_bytes(std::span{text_vs}))};
    if (!vertex_shader)
    {
        return nullptr;
    }

    const shader_handle pixel_shader{pipelines->create_shader(std::as_bytes(std::span{text_ps}))};
    if (!pixel_shader)
    {
        pipelines->destroy(vertex_shader);
        return nullptr;
    }

    const sampler_handle sampler{samplers->create(sampler_desc{
        sampler_filter::linear,
        sampler_filter::linear,
        sampler_filter::point,
        sampler_address_mode::clamp,
        sampler_address_mode::clamp,
        sampler_address_mode::clamp,
        1,
        0.0f,
        0.0f,
    })};
    if (!sampler)
    {
        pipelines->destroy(pixel_shader);
        pipelines->destroy(vertex_shader);
        return nullptr;
    }

    return std::unique_ptr<pipeline_cache>{new pipeline_cache{pipelines, samplers, vertex_shader, pixel_shader, sampler}};
}

std::optional<render_state> pipeline_cache::prepare(const resource::render_target_signature& target, shader_handle vertex_shader, shader_handle pixel_shader)
{
    if (!vertex_shader)
    {
        vertex_shader = vertex_shader_;
    }
    if (!pixel_shader)
    {
        pixel_shader = pixel_shader_;
    }

    const auto it{std::ranges::find_if(entries_, [&target, vertex_shader, pixel_shader](const entry& e)
    {
        return target == e.signature &&
            vertex_shader == e.vertex_shader &&
            pixel_shader == e.pixel_shader;
    })};
    if (it != entries_.end())
    {
        return render_state{it->pipeline, sampler_};
    }

    constexpr std::array bindings{
        shader_binding{shader_binding_type::constant_buffer, 0, shader_stage::vertex | shader_stage::pixel},
        shader_binding{shader_binding_type::texture_srv, 0, shader_stage::pixel},
        shader_binding{shader_binding_type::sampler, 0, shader_stage::pixel},
        shader_binding{shader_binding_type::constant_buffer, 1, shader_stage::vertex | shader_stage::pixel},
    };
    constexpr std::array elements{
        input_element{"POSITION", 0, input_element_format::float2, offsetof(vertex, position)},
        input_element{"TEXCOORD", 0, input_element_format::float2, offsetof(vertex, uv)},
        input_element{"COLOR", 0, input_element_format::float4, offsetof(vertex, color)},
        input_element{"COLOR", 1, input_element_format::float4, offsetof(vertex, outline_color)},
        input_element{"TEXCOORD", 1, input_element_format::uint1, offsetof(vertex, mode)},
        input_element{"TEXCOORD", 2, input_element_format::float1, offsetof(vertex, cluster)},
        input_element{"TEXCOORD", 3, input_element_format::float2, offsetof(vertex, pivot)},
    };
    const bool uses_default_shaders{vertex_shader == vertex_shader_ && pixel_shader == pixel_shader_};
    const std::size_t binding_count{uses_default_shaders ? 3u : 4u};
    const std::vector<render_target_blend_desc> blends(
        target.render_target_formats.size(),
        render_target_blend_desc{
            true,
            blend_factor::one,
            blend_factor::inverse_source_alpha,
            blend_op::add,
            blend_factor::one,
            blend_factor::inverse_source_alpha,
            blend_op::add,
            color_write_mask::all,
        }
    );
    const pipeline_handle pipeline{pipelines_->create_pipeline(graphics_pipeline_desc{
        .vertex_shader = vertex_shader,
        .pixel_shader = pixel_shader,
        .bindings = std::span{bindings}.first(binding_count),
        .render_target_formats = target.render_target_formats,
        .depth_stencil_format = target.depth_stencil_format,
        .layout = {sizeof(vertex), elements},
        .topology = primitive_topology::triangle_list,
        .render_target_blends = blends,
        .sample_count = target.sample_count,
    })};
    if (!pipeline)
    {
        return std::nullopt;
    }
    entries_.push_back(entry{target, vertex_shader, pixel_shader, pipeline});
    return render_state{pipeline, sampler_};
}

void pipeline_cache::invalidate(shader_handle shader)
{
    std::erase_if(entries_, [this, shader](const entry& e)
    {
        if (shader != e.vertex_shader && shader != e.pixel_shader)
        {
            return false;
        }
        pipelines_->destroy(e.pipeline);
        return true;
    });
}

pipeline_cache::pipeline_cache(resource::pipeline_manager* pipelines, resource::sampler_manager* samplers, shader_handle vertex_shader, shader_handle pixel_shader, sampler_handle sampler) :
    pipelines_{pipelines},
    samplers_{samplers},
    vertex_shader_{vertex_shader},
    pixel_shader_{pixel_shader},
    sampler_{sampler}
{
}
} // namespace nori::graphics::text
