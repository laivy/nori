#include <array>
#include <cassert>

#include "command_queue.hpp"
#include "descriptor_allocator.hpp"
#include "rendering/renderer.hpp"
#include "resource/pipeline_manager.hpp"

namespace
{
void set_render_area(ID3D12GraphicsCommandList* command_list, nori::core::vec2 size)
{
    const D3D12_VIEWPORT native_viewport{0.0f, 0.0f, size.x, size.y, 0.0f, 1.0f};
    const D3D12_RECT scissor{0, 0, static_cast<LONG>(size.x), static_cast<LONG>(size.y)};
    command_list->RSSetViewports(1, &native_viewport);
    command_list->RSSetScissorRects(1, &scissor);
}
} // namespace

namespace nori::graphics::rendering
{
renderer::renderer(params args) :
    descriptors_{args.descriptors},
    queue_{args.queue},
    textures_{args.textures},
    pipelines_{args.pipelines},
    buffers_{args.buffers},
    samplers_{args.samplers},
    texts_{args.texts},
    line_resources_{},
    rect_resources_{}
{
}

renderer::~renderer()
{
    for (const auto resources : {&line_resources_, &rect_resources_})
    {
        for (const auto& entry : resources->pipelines)
        {
            pipelines_->destroy(entry.pipeline);
        }
        if (resources->vertex_shader)
        {
            pipelines_->destroy(resources->vertex_shader);
        }
        if (resources->pixel_shader)
        {
            pipelines_->destroy(resources->pixel_shader);
        }
    }
}

void renderer::begin_rendering(const render_pass_desc& desc, const resource::render_target_info& info)
{
    assert(!active_pass_);

    const std::array<ID3D12DescriptorHeap*, 2> descriptor_heaps{
        descriptors_->heap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
        descriptors_->heap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
    };
    command_list()->SetDescriptorHeaps(static_cast<UINT>(descriptor_heaps.size()), descriptor_heaps.data());

    begin_render_targets(desc);
    set_render_area(command_list(), info.size);
    active_pass_.emplace(render_pass_state{desc, info});
}

void renderer::end_rendering()
{
    assert(active_pass_);

    end_render_targets(active_pass_->desc);
    active_pass_.reset();
}

bool renderer::render(const render_pass_desc& desc, std::span<const command_buffer* const> buffers)
{
    const resource::render_target_info info{make_render_target_info(desc)};
    begin_rendering(desc, info);
    for (const auto buffer : buffers)
    {
        if (!execute(*buffer))
        {
            end_rendering();
            return false;
        }
    }
    end_rendering();
    return true;
}

ID3D12GraphicsCommandList* renderer::command_list() const noexcept
{
    return queue_->recording_list();
}
} // namespace nori::graphics::rendering
