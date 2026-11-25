#pragma once
#include <memory>
#include <mutex>
#include <span>
#include <stack>
#include <utility>
#include <vector>

#include <nori/core/slot_map.hpp>
#include <nori/graphics/render_pass.hpp>

#include "command_buffer.hpp"

namespace nori::graphics
{
class command_encoder;

class command_manager
{
public:
    command_manager();
    ~command_manager() noexcept;

    render_pass_handle begin_render_pass(const render_pass_desc& desc);
    command_encoder* begin_encode(render_pass_handle handle);
    void end_encode(command_encoder* encoder);
    std::pair<render_pass_desc, std::span<const command_buffer* const>> end_render_pass(render_pass_handle handle);
    void end_frame();

private:
    struct render_pass
    {
        render_pass_desc desc;
        std::vector<const command_buffer*> buffers;
    };

    std::mutex mutex_;
    core::slot_map<render_pass_handle, render_pass> passes_;
    std::vector<std::unique_ptr<command_buffer>> buffer_pool_;
    std::stack<command_buffer*, std::vector<command_buffer*>> available_buffers_;
    std::vector<command_buffer*> used_buffers_; // end_encode 때 회수되서 end_frame 때 available_buffers_로 이동됨
    std::vector<std::unique_ptr<command_encoder>> encoder_pool_;
    std::stack<command_encoder*, std::vector<command_encoder*>> available_encoders_;
};
} // namespace nori::graphics
