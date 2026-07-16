#pragma once
#include <cstddef>
#include <vector>
#include <directx/d3dx12.h>
#include <wrl/client.h>
#include <nori/core/slot_map.hpp>
#include <nori/graphics/error_code.hpp>
#include <nori/graphics/pipeline.hpp>

namespace nori::graphics
{
class gpu_timeline;

struct shader_resource
{
    shader_stage stage;
    std::vector<std::byte> bytecode;
};

struct pipeline_resource
{
    shader_handle vertex_shader;
    shader_handle pixel_shader;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_state;
    std::vector<render_texture_format> color_formats;
};

class pipeline_manager
{
public:
    pipeline_manager(ID3D12Device* device, gpu_timeline* timeline);

    result<pipeline_handle> create_pipeline(const graphics_pipeline_specification& spec);
    result<void> destroy(pipeline_handle handle);
    result<shader_handle> create_shader(shader_specification spec);
    result<void> destroy(shader_handle handle);
    result<void> bind_fullscreen(pipeline_handle handle, std::span<const render_texture_format> output_formats, D3D12_GPU_DESCRIPTOR_HANDLE source, ID3D12GraphicsCommandList* command_list);

private:
    ID3D12Device* const device_;
    gpu_timeline* const timeline_;
    core::slot_map<shader_handle, shader_resource> shaders_;
    core::slot_map<pipeline_handle, pipeline_resource> pipelines_;
};
} // namespace nori::graphics
