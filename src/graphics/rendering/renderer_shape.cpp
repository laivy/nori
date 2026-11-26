#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>

#include "rendering/renderer.hpp"
#include "resource/buffer_manager.hpp"
#include "resource/pipeline_manager.hpp"
#include "shader/line_ps.hpp"
#include "shader/line_vs.hpp"
#include "shader/rect_ps.hpp"
#include "shader/rect_vs.hpp"

namespace
{
struct line_vertex
{
    nori::core::vec2 position;
    nori::core::vec2 local;
};

struct line_constants
{
    nori::core::vec2 target_size;
    nori::core::vec2 half_size;
    nori::core::vec4 color;
};

struct rect_vertex
{
    nori::core::vec2 position;
    nori::core::vec4 color;
};

struct rect_constants
{
    nori::core::vec2 target_size;
    nori::core::vec2 offset;
    nori::core::vec4 tint;
};
} // namespace

namespace nori::graphics::rendering
{
bool renderer::render_line(const line_draw_desc& desc, core::vec2 viewport_size)
{
    assert(active_pass_);

    if (desc.width == 0.0f || desc.start == desc.end || viewport_size.x == 0.0f || viewport_size.y == 0.0f)
    {
        return true;
    }
    const auto target_size{active_pass_->target_info.size};
    const double delta_x{static_cast<double>(desc.end.x) - desc.start.x};
    const double delta_y{static_cast<double>(desc.end.y) - desc.start.y};
    const auto length{std::hypot(delta_x, delta_y)};
    const auto tangent_x{delta_x / length};
    const auto tangent_y{delta_y / length};
    const auto padding_x{(std::abs(tangent_x) * target_size.x / viewport_size.x + std::abs(tangent_y) * target_size.y / viewport_size.y) * 0.5};
    const auto padding_y{(std::abs(tangent_y) * target_size.x / viewport_size.x + std::abs(tangent_x) * target_size.y / viewport_size.y) * 0.5};
    const auto radius_x{(std::abs(tangent_x) * target_size.x + std::abs(tangent_y) * target_size.y) * 0.5 + padding_x};
    const auto radius_y{(std::abs(tangent_y) * target_size.x + std::abs(tangent_x) * target_size.y) * 0.5 + padding_y};
    const auto center_projection{(target_size.x * tangent_x + target_size.y * tangent_y) * 0.5};
    const auto first{desc.start.x * tangent_x + desc.start.y * tangent_y - center_projection};
    const auto last{desc.end.x * tangent_x + desc.end.y * tangent_y - center_projection};
    const auto normal_offset{(desc.start.y * delta_x - desc.start.x * delta_y) / length + (target_size.x * tangent_y - target_size.y * tangent_x) * 0.5};
    const auto half_width{static_cast<double>(desc.width) * 0.5};
    // 보이지 않는 끝을 AA 영역 바깥에서 잘라 GPU 보간 좌표의 정밀도 유지
    const auto min_x{std::max(first, -radius_x)};
    const auto max_x{std::min(last, radius_x)};
    const auto min_y{std::max(normal_offset - half_width, -radius_y)};
    const auto max_y{std::min(normal_offset + half_width, radius_y)};
    if (min_x >= max_x || min_y >= max_y)
    {
        return true;
    }
    const std::array elements{
        input_element{"POSITION", 0, input_element_format::float2, offsetof(line_vertex, position)},
        input_element{"TEXCOORD", 0, input_element_format::float2, offsetof(line_vertex, local)},
    };
    const auto pipeline{shape_pipeline(line_resources_, {sizeof(line_vertex), elements}, std::as_bytes(std::span{line_vs}), std::as_bytes(std::span{line_ps}))};
    if (!pipeline)
    {
        return false;
    }
    const core::vec2 tangent{static_cast<float>(tangent_x), static_cast<float>(tangent_y)};
    const core::vec2 normal{-tangent.y, tangent.x};
    const core::vec2 half_size{static_cast<float>((max_x - min_x) * 0.5), static_cast<float>((max_y - min_y) * 0.5)};
    const core::vec2 padding{static_cast<float>(padding_x), static_cast<float>(padding_y)};
    const auto center_x{(min_x + max_x) * 0.5};
    const auto center_y{(min_y + max_y) * 0.5};
    const core::vec2 center{
        static_cast<float>(target_size.x * 0.5 + tangent_x * center_x - tangent_y * center_y),
        static_cast<float>(target_size.y * 0.5 + tangent_y * center_x + tangent_x * center_y),
    };
    const auto vertex = [&](float x, float y) -> line_vertex
    {
        return {center + tangent * x + normal * y, {x, y}};
    };
    const auto left{-half_size.x - padding.x};
    const auto right{half_size.x + padding.x};
    const auto top{-half_size.y - padding.y};
    const auto bottom{half_size.y + padding.y};
    const std::array vertices{vertex(left, top), vertex(right, top), vertex(left, bottom), vertex(left, bottom), vertex(right, top), vertex(right, bottom)};
    const auto uploaded{buffers_->create_frame_upload_buffer(buffer_usage::vertex, std::as_bytes(std::span{vertices}))};
    if (!uploaded)
    {
        return false;
    }
    const line_constants constants{active_pass_->target_info.size, half_size, desc.color};
    const auto constant_bytes{std::as_bytes(std::span{&constants, 1})};
    const draw_binding binding{constants_command{.size = constant_bytes.size()}};
    command_draw_state state{.pipeline = pipeline, .vertex_buffer = vertex_buffer_command{uploaded->buffer, sizeof(line_vertex), uploaded->offset, uploaded->size}, .bindings = {{.command = &binding, .dirty = true}}, .constants = constant_bytes};
    return render_draw(state, {static_cast<std::uint32_t>(vertices.size()), 1, 0, 0});
}

bool renderer::render_rectangles(std::span<const rect_draw_desc> rectangles, core::vec2 offset, core::vec4 tint)
{
    assert(active_pass_);

    const std::array elements{
        input_element{"POSITION", 0, input_element_format::float2, offsetof(rect_vertex, position)},
        input_element{"COLOR", 0, input_element_format::float4, offsetof(rect_vertex, color)},
    };
    const auto pipeline{shape_pipeline(rect_resources_, {sizeof(rect_vertex), elements}, std::as_bytes(std::span{rect_vs}), std::as_bytes(std::span{rect_ps}))};
    if (!pipeline)
    {
        return false;
    }

    std::vector<rect_vertex> vertices;
    vertices.reserve(rectangles.size() * 6);
    for (const auto& rect : rectangles)
    {
        const auto left{rect.position.x};
        const auto top{rect.position.y};
        const auto right{left + rect.size.x};
        const auto bottom{top + rect.size.y};
        vertices.insert(vertices.end(), {{{left, top}, rect.color}, {{right, top}, rect.color}, {{left, bottom}, rect.color}, {{left, bottom}, rect.color}, {{right, top}, rect.color}, {{right, bottom}, rect.color}});
    }
    const auto uploaded{buffers_->create_frame_upload_buffer(buffer_usage::vertex, std::as_bytes(std::span{vertices}))};
    if (!uploaded)
    {
        return false;
    }

    const rect_constants constants{active_pass_->target_info.size, offset, tint};
    const auto constant_bytes{std::as_bytes(std::span{&constants, 1})};
    const draw_binding binding{constants_command{.size = constant_bytes.size()}};
    command_draw_state state{.pipeline = pipeline, .vertex_buffer = vertex_buffer_command{uploaded->buffer, sizeof(rect_vertex), uploaded->offset, uploaded->size}, .bindings = {{.command = &binding, .dirty = true}}, .constants = constant_bytes};
    return render_draw(state, {static_cast<std::uint32_t>(vertices.size()), 1, 0, 0});
}

pipeline_handle renderer::shape_pipeline(shape_resources& resources, input_layout layout, std::span<const std::byte> vertex_shader, std::span<const std::byte> pixel_shader)
{
    assert(active_pass_);

    const resource::render_target_signature& signature{active_pass_->target_info.signature};
    const auto it{std::ranges::find_if(resources.pipelines, [&signature](const shape_resources::pipeline_entry& entry)
    {
        return signature == entry.signature;
    })};
    if (it != resources.pipelines.end())
    {
        return it->pipeline;
    }

    if (!resources.vertex_shader)
    {
        resources.vertex_shader = pipelines_->create_shader(vertex_shader);
        if (!resources.vertex_shader)
        {
            return {};
        }
    }
    if (!resources.pixel_shader)
    {
        resources.pixel_shader = pipelines_->create_shader(pixel_shader);
        if (!resources.pixel_shader)
        {
            return {};
        }
    }
    const std::array bindings{shader_binding{
        shader_binding_type::constant_buffer,
        0,
        shader_stage::vertex | shader_stage::pixel
    }};
    const std::vector<render_target_blend_desc> blends(
        signature.render_target_formats.size(),
        render_target_blend_desc{
            true,
            blend_factor::one,
            blend_factor::inverse_source_alpha,
            blend_op::add,
            blend_factor::one,
            blend_factor::inverse_source_alpha,
            blend_op::add,
            color_write_mask::all
        }
    );
    const auto pipeline{pipelines_->create_pipeline(graphics_pipeline_desc{
        .vertex_shader = resources.vertex_shader,
        .pixel_shader = resources.pixel_shader,
        .bindings = bindings,
        .render_target_formats = signature.render_target_formats,
        .depth_stencil_format = signature.depth_stencil_format,
        .layout = layout,
        .topology = primitive_topology::triangle_list,
        .render_target_blends = blends,
        .sample_count = signature.sample_count,
    })};
    if (!pipeline)
    {
        return {};
    }

    resources.pipelines.push_back({signature, pipeline});
    return pipeline;
}
} // namespace nori::graphics::rendering
