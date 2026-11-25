#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "rendering/renderer.hpp"
#include "resource/buffer_manager.hpp"
#include "resource/pipeline_manager.hpp"
#include "resource/sampler_manager.hpp"
#include "resource/texture_manager.hpp"
#include "resource_states.hpp"
#include "text/manager.hpp"

namespace
{
struct text_constants
{
    nori::core::vec2 target_size;
    nori::core::vec2 offset;
    nori::core::vec4 color;
    nori::core::vec2 layout_size;
    std::uint32_t outline_pass;
    float padding;
};

constexpr D3D12_PRIMITIVE_TOPOLOGY to_d3d12_topology(nori::graphics::primitive_topology topology)
{
    switch (topology)
    {
    case nori::graphics::primitive_topology::point_list:
    {
        return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
    }
    case nori::graphics::primitive_topology::line_list:
    {
        return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
    }
    case nori::graphics::primitive_topology::line_strip:
    {
        return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
    }
    case nori::graphics::primitive_topology::triangle_list:
    {
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }
    case nori::graphics::primitive_topology::triangle_strip:
    {
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    }
    default:
        std::unreachable();
    }
}

constexpr DXGI_FORMAT to_index_format(nori::graphics::index_format format)
{
    switch (format)
    {
    case nori::graphics::index_format::uint16:
    {
        return DXGI_FORMAT_R16_UINT;
    }
    case nori::graphics::index_format::uint32:
    {
        return DXGI_FORMAT_R32_UINT;
    }
    default:
        std::unreachable();
    }
}

constexpr bool is_glyph(const nori::graphics::text::textured_batch& batch)
{
    return batch.content == nori::graphics::text::textured_batch::content_type::glyph;
}

constexpr bool is_glyph(const nori::graphics::text::batch& entry)
{
    const auto batch{std::get_if<nori::graphics::text::textured_batch>(&entry)};
    return batch && is_glyph(*batch);
}

constexpr bool has_outline_batch(const nori::graphics::text::batch& entry)
{
    const auto batch{std::get_if<nori::graphics::text::textured_batch>(&entry)};
    return batch && batch->has_outline;
}
} // namespace

namespace nori::graphics::rendering
{
bool renderer::execute(const command_buffer& buffer)
{
    assert(active_pass_);

    command_draw_state command_state{};
    std::optional<std::size_t> state_index;
    for (const auto& command : buffer.draws)
    {
        const draw_state& state{buffer.states[command.state_index]};
        if (state_index != command.state_index)
        {
            command_state.pipeline = state.pipeline;
            command_state.vertex_buffer = state.vertex_buffer;
            command_state.index_buffer = state.index_buffer;
            command_state.constants = buffer.constants;
            command_state.bindings.clear();
            for (std::size_t i{0}; i < state.binding_count; ++i)
            {
                command_state.bindings.push_back({
                    .command = &buffer.bindings[state.binding_offset + i],
                    .dirty = true,
                });
            }
            state_index = command.state_index;
        }

        D3D12_VIEWPORT vp{0.0f, 0.0f, active_pass_->target_info.size.x, active_pass_->target_info.size.y, 0.0f, 1.0f};
        if (state.view)
        {
            vp = D3D12_VIEWPORT{
                state.view->position.x,
                state.view->position.y,
                state.view->size.x,
                state.view->size.y,
                state.view->depth_range.x,
                state.view->depth_range.y
            };
        }
        command_list()->RSSetViewports(1, &vp);

        const scissor_rect scissor{state.scissor.value_or(scissor_rect{
            .width = static_cast<std::uint32_t>(active_pass_->target_info.size.x),
            .height = static_cast<std::uint32_t>(active_pass_->target_info.size.y)
        })};
        const D3D12_RECT native_scissor{static_cast<LONG>(scissor.x), static_cast<LONG>(scissor.y), static_cast<LONG>(scissor.x + scissor.width), static_cast<LONG>(scissor.y + scissor.height)};
        command_list()->RSSetScissorRects(1, &native_scissor);

        const bool res{std::visit([this, &command_state, &buffer, &vp]<typename T>(const T& value) -> bool
        {
            if constexpr (std::same_as<T, draw_args>)
            {
                return render_draw(command_state, value);
            }
            else if constexpr (std::same_as<T, draw_indexed_args>)
            {
                return render_draw_indexed(command_state, value);
            }
            else if constexpr (std::same_as<T, line_draw_desc>)
            {
                return render_line(value, core::vec2{vp.Width, vp.Height});
            }
            else if constexpr (std::same_as<T, rect_draw_desc>)
            {
                return render_rectangles({&value, 1}, {}, {1.0f, 1.0f, 1.0f, 1.0f});
            }
            else
            {
                return render_text(value, buffer.constants);
            }
        }, command.operation)};
        if (!res)
        {
            return false;
        }
    }
    return true;
}

bool renderer::bind_draw(command_draw_state& state)
{
    pipelines_->bind(state.pipeline, command_list());

    const resource::pipeline& pipeline{pipelines_->at(state.pipeline)};
    for (std::uint32_t slot{0}; slot < pipeline.bindings.size(); ++slot)
    {
        auto& binding{state.bindings[slot]};
        if (binding.dirty)
        {
            const auto res{std::visit([&]<typename T>(const T& value) -> bool
            {
                if constexpr (std::same_as<T, constants_command>)
                {
                    const auto view{buffers_->create_frame_upload_buffer(buffer_usage::constant, state.constants.subspan(value.offset, value.size))};
                    if (!view)
                    {
                        return false;
                    }
                    binding.address = buffers_->at(view->buffer).resource->GetGPUVirtualAddress() + view->offset;
                    return true;
                }
                else if constexpr (std::same_as<T, constant_buffer_command>)
                {
                    auto& buffer{buffers_->at(value.buffer)};
                    if (buffer.access == buffer_access::gpu_only)
                    {
                        buffer.states.transition(command_list(), buffer.resource.Get(), D3D12_RESOURCE_STATE_GENERIC_READ);
                    }
                    binding.address = buffer.resource->GetGPUVirtualAddress() + value.offset;
                    return true;
                }
                else if constexpr (std::same_as<T, texture_handle>)
                {
                    auto& texture{textures_->at(value)};
                    texture.states.transition(command_list(), texture.resource.Get(), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
                    binding.descriptor = textures_->srv(value);
                    return true;
                }
                else if constexpr (std::same_as<T, sampler_handle>)
                {
                    binding.descriptor = samplers_->descriptor(value);
                    return true;
                }
                else
                {
                    std::unreachable();
                }
            }, *binding.command)};
            if (!res)
            {
                return false;
            }
            binding.dirty = false;
        }
        if (pipeline.bindings[slot].type == shader_binding_type::constant_buffer)
        {
            command_list()->SetGraphicsRootConstantBufferView(slot, binding.address);
        }
        else
        {
            command_list()->SetGraphicsRootDescriptorTable(slot, binding.descriptor);
        }
    }
    return true;
}

void renderer::bind_vertex_buffer(const command_draw_state& state)
{
    resource::buffer& buffer{buffers_->at(state.vertex_buffer.buffer)};
    if (buffer.access == buffer_access::gpu_only)
    {
        buffer.states.transition(command_list(), buffer.resource.Get(), D3D12_RESOURCE_STATE_GENERIC_READ);
    }

    const D3D12_VERTEX_BUFFER_VIEW view{buffer.resource->GetGPUVirtualAddress() + state.vertex_buffer.offset, static_cast<UINT>(state.vertex_buffer.size.value_or(buffer.size - state.vertex_buffer.offset)), state.vertex_buffer.stride};
    command_list()->IASetVertexBuffers(0, 1, &view);
}

bool renderer::render_draw(command_draw_state& state, const draw_args& draw)
{
    const auto& pipeline{pipelines_->at(state.pipeline)};

    if (pipeline.vertex_stride != 0)
    {
        bind_vertex_buffer(state);
    }

    if (const auto res{bind_draw(state)}; !res)
    {
        return false;
    }
    command_list()->IASetPrimitiveTopology(to_d3d12_topology(pipeline.topology));
    command_list()->DrawInstanced(draw.vertex_count, draw.instance_count, draw.first_vertex, draw.first_instance);
    return true;
}

bool renderer::render_draw_indexed(command_draw_state& state, const draw_indexed_args& draw)
{
    const auto& pipeline{pipelines_->at(state.pipeline)};

    resource::buffer& indices{buffers_->at(state.index_buffer.buffer)};
    if (indices.access == buffer_access::gpu_only)
    {
        indices.states.transition(command_list(), indices.resource.Get(), D3D12_RESOURCE_STATE_GENERIC_READ);
    }

    if (pipeline.vertex_stride != 0)
    {
        bind_vertex_buffer(state);
    }
    if (const auto res{bind_draw(state)}; !res)
    {
        return false;
    }
    const D3D12_INDEX_BUFFER_VIEW index_view{indices.resource->GetGPUVirtualAddress() + state.index_buffer.offset, static_cast<UINT>(state.index_buffer.size.value_or(indices.size - state.index_buffer.offset)), to_index_format(state.index_buffer.format)};
    command_list()->IASetIndexBuffer(&index_view);
    command_list()->IASetPrimitiveTopology(to_d3d12_topology(pipeline.topology));
    command_list()->DrawIndexedInstanced(draw.index_count, draw.instance_count, draw.first_index, draw.vertex_offset, draw.first_instance);
    return true;
}

bool renderer::render_text(const text_draw_command& desc, std::span<const std::byte> recorded_constants)
{
    assert(active_pass_);

    const text::layout& layout{texts_->at(desc.layout)};

    if (layout.batches.empty())
    {
        return true;
    }

    text_constants constants{
        .target_size = active_pass_->target_info.size,
        .offset = desc.position,
        .color = desc.color,
        .layout_size = {layout.measured.width, layout.measured.height},
    };
    const auto constant_bytes{std::as_bytes(std::span{&constants, 1})};

    std::array<draw_binding, 4> bindings{
        constants_command{.size = constant_bytes.size()},
        texture_handle{},
        sampler_handle{},
        std::monostate{},
    };
    command_draw_state state{.constants = constant_bytes};
    for (const auto& binding : bindings)
    {
        state.bindings.push_back({.command = &binding, .dirty = true});
    }

    pipeline_handle effect_pipeline{};
    std::optional<text::render_state> prepared_state;

    for (std::size_t first{0}; first < layout.batches.size();)
    {
        std::size_t last{first + 1};
        if (is_glyph(layout.batches[first]))
        {
            while (last < layout.batches.size() && is_glyph(layout.batches[last]))
            {
                ++last;
            }
        }

        const auto group{std::span{layout.batches}.subspan(first, last - first)};
        const bool has_outline{std::ranges::any_of(group, has_outline_batch)};

        for (int layer{has_outline ? 1 : 0}; layer >= 0; --layer)
        {
            constants.outline_pass = static_cast<std::uint32_t>(layer);
            state.bindings[0].dirty = true;

            for (const auto& entry : group)
            {
                if (const auto batch{std::get_if<text::rect_batch>(&entry)})
                {
                    if (!render_rectangles(std::span{layout.rectangles}.subspan(batch->first_rect, batch->rect_count), desc.position, desc.color))
                    {
                        return false;
                    }
                    continue;
                }

                const auto& batch{std::get<text::textured_batch>(entry)};
                if (!prepared_state)
                {
                    prepared_state = texts_->prepare_render_state(active_pass_->target_info.signature);
                    if (!prepared_state)
                    {
                        return false;
                    }
                    bindings[2] = prepared_state->sampler;
                }
                state.pipeline = prepared_state->pipeline;

                if (is_glyph(batch) && (desc.vertex_shader || desc.pixel_shader))
                {
                    if (!effect_pipeline)
                    {
                        const auto effect_state{texts_->prepare_render_state(active_pass_->target_info.signature, desc.vertex_shader, desc.pixel_shader)};
                        if (!effect_state)
                        {
                            return false;
                        }
                        effect_pipeline = effect_state->pipeline;
                        constexpr std::array<std::byte, 16> empty_constants{};
                        const auto effect_bytes{desc.constants.size == 0 ? std::span<const std::byte>{empty_constants} : recorded_constants.subspan(desc.constants.offset, desc.constants.size)};
                        const auto uploaded{buffers_->create_frame_upload_buffer(buffer_usage::constant, effect_bytes)};
                        if (!uploaded)
                        {
                            return false;
                        }
                        bindings[3] = constant_buffer_command{uploaded->buffer, uploaded->offset};
                    }
                    state.pipeline = effect_pipeline;
                }

                state.vertex_buffer = vertex_buffer_command{layout.vertices, sizeof(text::vertex), 0};
                bindings[1] = batch.texture;
                state.bindings[1].dirty = true;
                if (!render_draw(state, {batch.vertex_count, 1, batch.first_vertex, 0}))
                {
                    return false;
                }
            }
        }
        first = last;
    }

    return true;
}
} // namespace nori::graphics::rendering
