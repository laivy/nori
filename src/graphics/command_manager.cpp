#include <cassert>

#include "command_encoder_internal.hpp"
#include "command_manager.hpp"

namespace nori::graphics
{
command_manager::command_manager() = default;

command_manager::~command_manager() noexcept = default;

render_pass_handle command_manager::begin_render_pass(const render_pass_desc& desc)
{
    const std::lock_guard lock{mutex_};
    return passes_.emplace(render_pass{desc, {}});
}

command_encoder* command_manager::begin_encode(render_pass_handle handle)
{
    const std::lock_guard lock{mutex_};

    command_encoder* encoder{};
    if (available_encoders_.empty())
    {
        std::unique_ptr<command_encoder> object{new command_encoder};
        encoder = object.get();
        encoder_pool_.push_back(std::move(object));
    }
    else
    {
        encoder = available_encoders_.top();
        available_encoders_.pop();
    }

    command_buffer* buffer{};
    if (available_buffers_.empty())
    {
        auto object{std::make_unique<command_buffer>()};
        buffer = object.get();
        buffer_pool_.push_back(std::move(object));
    }
    else
    {
        buffer = available_buffers_.top();
        available_buffers_.pop();
    }
    buffer->pass = handle;
    encoder->impl_->begin_encode(buffer);
    return encoder;
}

void command_manager::end_encode(command_encoder* encoder)
{
    assert(encoder);
    const std::lock_guard lock{mutex_};

    command_buffer* const buffer{encoder->impl_->end_encode()};
    render_pass& pass{passes_[buffer->pass]};
    pass.buffers.push_back(buffer);
    used_buffers_.push_back(buffer);
    available_encoders_.push(encoder);
}

std::pair<render_pass_desc, std::span<const command_buffer* const>> command_manager::end_render_pass(render_pass_handle handle)
{
    const std::lock_guard lock{mutex_};
    render_pass& pass{passes_[handle]};
    return {pass.desc, pass.buffers};
}

void command_manager::end_frame()
{
    const std::lock_guard lock{mutex_};
    assert(encoder_pool_.size() == available_encoders_.size() && "frame still has active command encoders");

    passes_.clear();
    for (const auto buffer : used_buffers_)
    {
        buffer->pass = {};
        buffer->states.clear();
        buffer->bindings.clear();
        buffer->constants.clear();
        buffer->draws.clear();
        available_buffers_.push(buffer);
    }
    used_buffers_.clear();
}
} // namespace nori::graphics
