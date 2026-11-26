#pragma once
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include <directx/d3d12.h>

#include <nori/graphics/render_pass.hpp>

#include "command_buffer.hpp"
#include "resource/render_target_info.hpp"
#include "resource/render_target_signature.hpp"

namespace nori::graphics
{
class command_queue;
class descriptor_allocator;
} // namespace nori::graphics

namespace nori::graphics::resource
{
class buffer_manager;
class pipeline_manager;
class sampler_manager;
class texture_manager;
} // namespace nori::graphics::resource

namespace nori::graphics::text
{
class manager;
} // namespace nori::graphics::text

namespace nori::graphics::rendering
{
class renderer
{
public:
    struct params
    {
        descriptor_allocator* descriptors;
        command_queue* queue;
        resource::texture_manager* textures;
        resource::pipeline_manager* pipelines;
        resource::buffer_manager* buffers;
        resource::sampler_manager* samplers;
        text::manager* texts;
    };

    explicit renderer(params args);
    ~renderer();

    renderer(const renderer&) = delete;
    renderer(renderer&&) = delete;
    renderer& operator=(const renderer&) = delete;
    renderer& operator=(renderer&&) = delete;

    resource::render_target_info make_render_target_info(const render_pass_desc& desc) const;
    void begin_rendering(const render_pass_desc& desc, const resource::render_target_info& info);
    void end_rendering();

    bool render(const render_pass_desc& desc, std::span<const command_buffer* const> buffers);

    ID3D12GraphicsCommandList* command_list() const noexcept;

private:
    struct draw_binding_state
    {
        const draw_binding* command;
        D3D12_GPU_VIRTUAL_ADDRESS address;
        D3D12_GPU_DESCRIPTOR_HANDLE descriptor;
        bool dirty;
    };

    struct command_draw_state
    {
        pipeline_handle pipeline;
        vertex_buffer_command vertex_buffer;
        index_buffer_command index_buffer;
        std::vector<draw_binding_state> bindings;
        std::span<const std::byte> constants;
    };

    struct render_pass_state
    {
        render_pass_desc desc;
        resource::render_target_info target_info;
    };

    struct shape_resources
    {
        struct pipeline_entry
        {
            resource::render_target_signature signature;
            pipeline_handle pipeline;
        };

        std::vector<pipeline_entry> pipelines;
        shader_handle vertex_shader;
        shader_handle pixel_shader;
    };

    void begin_render_targets(const render_pass_desc& desc);
    void end_render_targets(const render_pass_desc& desc);

    bool execute(const command_buffer& buffer);
    bool bind_draw(command_draw_state& state);
    void bind_vertex_buffer(const command_draw_state& state);

    bool render_draw(command_draw_state& state, const draw_args& draw);
    bool render_draw_indexed(command_draw_state& state, const draw_indexed_args& draw);

    bool render_line(const line_draw_desc& desc, core::vec2 viewport_size);
    bool render_rectangles(std::span<const rect_draw_desc> rectangles, core::vec2 offset, core::vec4 tint);
    pipeline_handle shape_pipeline(shape_resources& resources, input_layout layout, std::span<const std::byte> vertex_shader, std::span<const std::byte> pixel_shader);

    bool render_text(const text_draw_command& desc, std::span<const std::byte> recorded_constants);

    descriptor_allocator* const descriptors_;
    command_queue* const queue_;
    resource::texture_manager* const textures_;
    resource::pipeline_manager* const pipelines_;
    resource::buffer_manager* const buffers_;
    resource::sampler_manager* const samplers_;
    text::manager* const texts_;
    std::optional<render_pass_state> active_pass_;

    shape_resources line_resources_;
    shape_resources rect_resources_;
};
} // namespace nori::graphics::rendering
