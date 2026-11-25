#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <directx/d3d12.h>
#include <wrl/client.h>

#include <nori/core/slot_map.hpp>
#include <nori/graphics/pipeline.hpp>

namespace nori::graphics
{
class command_queue;
} // namespace nori::graphics

namespace nori::graphics::resource
{
struct pipeline
{
    std::vector<shader_binding> bindings;
    std::uint32_t vertex_stride;
    primitive_topology topology;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_state;
};

struct shader
{
    std::vector<std::byte> bytecode;
};

class pipeline_manager
{
public:
    struct params
    {
        ID3D12Device* device;
        command_queue* queue;
    };

    explicit pipeline_manager(params args);

    shader_handle create_shader(std::span<const std::byte> bytecode);
    void destroy(shader_handle handle);

    pipeline_handle create_pipeline(const graphics_pipeline_desc& desc);
    void bind(pipeline_handle handle, ID3D12GraphicsCommandList* command_list);
    void destroy(pipeline_handle handle);

    const pipeline& at(pipeline_handle handle) const;

private:
    std::optional<pipeline> create_pipeline_resource(const graphics_pipeline_desc& desc) const;

    ID3D12Device* const device_;
    command_queue* const queue_;
    core::slot_map<shader_handle, shader> shaders_;
    core::slot_map<pipeline_handle, pipeline> pipelines_;
};
} // namespace nori::graphics::resource
