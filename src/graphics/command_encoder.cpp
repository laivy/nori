#include <algorithm>
#include <cassert>
#include <utility>

#include <nori/graphics/command_encoder.hpp>

#include "command_encoder_internal.hpp"

namespace nori::graphics
{
command_encoder::impl::impl() :
    buffer_{},
    state_{},
    bindings_{},
    state_index_{},
    dirty_{true}
{
}

void command_encoder::impl::begin_encode(command_buffer* buffer)
{
    buffer_ = buffer;
    state_ = {};
    bindings_.clear();
    state_index_ = {};
    dirty_ = true;
}

command_buffer* command_encoder::impl::end_encode()
{
    assert(buffer_);
    state_ = {};
    bindings_.clear();
    dirty_ = true;
    return std::exchange(buffer_, nullptr);
}

void command_encoder::impl::set_pipeline(pipeline_handle pipeline)
{
    state_.pipeline = pipeline;
    dirty_ = true;
}

void command_encoder::impl::set_vertex_buffer(buffer_handle buffer, std::uint32_t stride, std::uint64_t offset, std::optional<std::uint64_t> size)
{
    state_.vertex_buffer = vertex_buffer_command{buffer, stride, offset, size};
    dirty_ = true;
}

void command_encoder::impl::set_index_buffer(buffer_handle buffer, index_format format, std::uint64_t offset, std::optional<std::uint64_t> size)
{
    state_.index_buffer = index_buffer_command{buffer, format, offset, size};
    dirty_ = true;
}

void command_encoder::impl::set_constant_buffer(std::uint32_t binding_index, buffer_handle buffer, std::uint64_t offset)
{
    set_binding(binding_index, constant_buffer_command{buffer, offset});
}

void command_encoder::impl::set_constant_buffer(std::uint32_t binding_index, std::span<const std::byte> data)
{
    const std::size_t offset{buffer_->constants.size()};
    buffer_->constants.append_range(data);
    set_binding(binding_index, constants_command{offset, data.size()});
}

void command_encoder::impl::set_texture(std::uint32_t binding_index, texture_handle texture)
{
    set_binding(binding_index, texture);
}

void command_encoder::impl::set_sampler(std::uint32_t binding_index, sampler_handle sampler)
{
    set_binding(binding_index, sampler);
}

void command_encoder::impl::set_viewport(viewport view)
{
    state_.view = view;
    dirty_ = true;
}

void command_encoder::impl::set_scissor_rect(scissor_rect scissor)
{
    state_.scissor = scissor;
    dirty_ = true;
}

void command_encoder::impl::draw(draw_operation operation)
{
    if (dirty_)
    {
        state_.binding_offset = buffer_->bindings.size();
        state_.binding_count = bindings_.size();
        buffer_->bindings.append_range(bindings_);
        state_index_ = buffer_->states.size();
        buffer_->states.push_back(state_);
        dirty_ = false;
    }
    buffer_->draws.push_back({state_index_, std::move(operation)});
}

void command_encoder::impl::draw_text(text::draw_desc desc)
{
    text_draw_command command{desc.layout, desc.position, desc.color};
    if (desc.glyph_effect)
    {
        command.vertex_shader = desc.glyph_effect->vertex_shader;
        command.pixel_shader = desc.glyph_effect->pixel_shader;
        command.constants = {buffer_->constants.size(), desc.glyph_effect->constants.size()};
        buffer_->constants.append_range(desc.glyph_effect->constants);
    }
    draw(command);
}

void command_encoder::impl::set_binding(std::uint32_t binding_index, draw_binding binding)
{
    bindings_.resize(std::max(bindings_.size(), static_cast<std::size_t>(binding_index) + 1));
    bindings_[binding_index] = std::move(binding);
    dirty_ = true;
}

command_encoder::~command_encoder() noexcept = default;

void command_encoder::set_pipeline(pipeline_handle pipeline)
{
    impl_->set_pipeline(pipeline);
}

void command_encoder::set_vertex_buffer(buffer_handle buffer, std::uint32_t stride, std::uint64_t offset)
{
    impl_->set_vertex_buffer(buffer, stride, offset);
}

void command_encoder::set_vertex_buffer(frame_upload_buffer_view buffer, std::uint32_t stride)
{
    impl_->set_vertex_buffer(buffer.buffer, stride, buffer.offset, buffer.size);
}

void command_encoder::set_index_buffer(buffer_handle buffer, index_format format, std::uint64_t offset)
{
    impl_->set_index_buffer(buffer, format, offset);
}

void command_encoder::set_index_buffer(frame_upload_buffer_view buffer, index_format format)
{
    impl_->set_index_buffer(buffer.buffer, format, buffer.offset, buffer.size);
}

void command_encoder::set_constant_buffer(std::uint32_t binding_index, buffer_handle buffer, std::uint64_t offset)
{
    impl_->set_constant_buffer(binding_index, buffer, offset);
}

void command_encoder::set_constant_buffer(std::uint32_t binding_index, frame_upload_buffer_view buffer)
{
    impl_->set_constant_buffer(binding_index, buffer.buffer, buffer.offset);
}

void command_encoder::set_constant_buffer(std::uint32_t binding_index, std::span<const std::byte> data)
{
    impl_->set_constant_buffer(binding_index, data);
}

void command_encoder::set_texture(std::uint32_t binding_index, texture_handle texture)
{
    impl_->set_texture(binding_index, texture);
}

void command_encoder::set_sampler(std::uint32_t binding_index, sampler_handle sampler)
{
    impl_->set_sampler(binding_index, sampler);
}

void command_encoder::set_viewport(viewport view)
{
    impl_->set_viewport(view);
}

void command_encoder::set_scissor_rect(scissor_rect scissor)
{
    impl_->set_scissor_rect(scissor);
}

void command_encoder::draw(std::uint32_t vertex_count, std::uint32_t first_vertex)
{
    impl_->draw(draw_args{vertex_count, 1, first_vertex, 0});
}

void command_encoder::draw_instanced(std::uint32_t vertex_count, std::uint32_t instance_count, std::uint32_t first_vertex, std::uint32_t first_instance)
{
    impl_->draw(draw_args{vertex_count, instance_count, first_vertex, first_instance});
}

void command_encoder::draw_indexed(std::uint32_t index_count, std::uint32_t first_index, std::int32_t vertex_offset)
{
    impl_->draw(draw_indexed_args{index_count, 1, first_index, vertex_offset, 0});
}

void command_encoder::draw_indexed_instanced(std::uint32_t index_count, std::uint32_t instance_count, std::uint32_t first_index, std::int32_t vertex_offset, std::uint32_t first_instance)
{
    impl_->draw(draw_indexed_args{index_count, instance_count, first_index, vertex_offset, first_instance});
}

void command_encoder::draw_line(line_draw_desc desc)
{
    impl_->draw(desc);
}

void command_encoder::draw_rect(rect_draw_desc desc)
{
    impl_->draw(desc);
}

void command_encoder::draw_text(text::draw_desc desc)
{
    impl_->draw_text(desc);
}

command_encoder::command_encoder() :
    impl_{std::make_unique<impl>()}
{
}
} // namespace nori::graphics
