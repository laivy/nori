#pragma once
#include <cstdint>
#include <memory>

#include <wrl/client.h>

#include <nori/graphics/buffer.hpp>
#include <nori/graphics/error_code.hpp>
#include <nori/graphics/pipeline.hpp>
#include <nori/graphics/render_pass.hpp>
#include <nori/graphics/sampler.hpp>
#include <nori/graphics/swap_chain.hpp>
#include <nori/graphics/text.hpp>
#include <nori/graphics/texture.hpp>
#include <nori/graphics/transfer.hpp>
#include <nori/resource/handle.hpp>

#include "command_queue.hpp"

struct IDXGIFactory4;
struct ID3D12Device;

namespace nori::graphics::resource
{
class buffer_manager;
class pipeline_manager;
class sampler_manager;
class swap_chain_manager;
class texture_manager;
} // namespace nori::graphics::resource

namespace nori::graphics::rendering
{
class renderer;
} // namespace nori::graphics::rendering

namespace nori::graphics::text
{
class manager;
} // namespace nori::graphics::text

namespace nori::graphics::transfer
{
class manager;
} // namespace nori::graphics::transfer

namespace nori::graphics
{
class command_manager;
class descriptor_allocator;
class upload_manager;

namespace imgui
{
class context;
}

class context
{
public:
    ~context() noexcept;

    static std::unique_ptr<context> create();

    bool begin_frame();
    render_pass_handle begin_render_pass(const render_pass_desc& desc);
    bool end_render_pass(render_pass_handle handle);
    bool begin_imgui_render_pass(imgui::context& ui, const render_pass_desc& desc);
    bool end_imgui_render_pass(imgui::context& ui);
    bool end_frame();
    bool present(swap_chain_handle handle);

    std::uint64_t submit();
    bool poll(std::uint64_t fence_value);
    void wait(std::uint64_t fence_value);

    ID3D12Device* device() const noexcept;
    descriptor_allocator* descriptors() noexcept;
    command_queue* queue() noexcept;
    resource::texture_manager* textures() noexcept;
    command_manager* commands() noexcept;

    result<swap_chain_handle> create_swap_chain(swap_chain_desc desc);
    texture_handle back_buffer(swap_chain_handle handle) const;
    result<void> resize_swap_chain(swap_chain_handle handle, std::uint32_t width, std::uint32_t height);
    void destroy(swap_chain_handle handle);

    result<texture_handle> create_texture(texture_desc desc);
    result<texture_handle> create_texture(nori::resource::handle source);
    result<void> update_texture(texture_handle handle, texture_region region, texture_data data);
    void destroy(texture_handle handle);

    result<buffer_handle> create_buffer(buffer_desc desc, std::span<const std::byte> initial_data);
    result<frame_upload_buffer_view> create_frame_upload_buffer(buffer_usage usage, std::span<const std::byte> data);
    result<void> update_buffer(buffer_handle handle, std::uint64_t offset, std::span<const std::byte> data);
    void destroy(buffer_handle handle);

    shader_handle create_shader(std::span<const std::byte> bytecode);
    void destroy(shader_handle handle);
    pipeline_handle create_graphics_pipeline(const graphics_pipeline_desc& desc);
    void destroy(pipeline_handle handle);

    sampler_handle create_sampler(sampler_desc desc);
    void destroy(sampler_handle handle);

    result<text::font_handle> create_font(text::font_desc desc);
    result<text::font_handle> create_font(nori::resource::handle source, std::uint32_t face_index);
    void destroy(text::font_handle handle);

    result<text::layout_handle> create_text_layout(text::layout_desc desc, std::string_view utf8);
    result<text::layout_handle> create_text_layout(text::layout_desc desc, std::span<const text::item> items);
    result<void> update_text_layout(text::layout_handle layout, std::string_view utf8);
    result<void> update_text_layout(text::layout_handle layout, text::layout_desc desc, std::string_view utf8);
    result<void> update_text_layout(text::layout_handle layout, std::span<const text::item> items);
    result<void> update_text_layout(text::layout_handle layout, text::layout_desc desc, std::span<const text::item> items);
    text::layout_metrics text_layout_metrics(text::layout_handle layout) const;
    void destroy(text::layout_handle handle);

    result<void> copy_buffer(buffer_handle source, std::uint64_t source_offset, buffer_handle destination, std::uint64_t destination_offset, std::uint64_t size);
    result<void> copy_texture(texture_handle source, texture_region source_region, texture_handle destination, texture_region destination_region);
    result<void> blit_texture(texture_handle source, texture_region source_region, texture_handle destination, texture_region destination_region, blit_filter filter);
    result<void> generate_mips(texture_handle texture);

    result<readback_handle> read_texture(texture_handle texture, texture_region region);
    readback_result poll(readback_handle handle);
    void destroy(readback_handle handle);

private:
    struct params;

    explicit context(params args);

    Microsoft::WRL::ComPtr<IDXGIFactory4> dxgi_factory_;
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    std::unique_ptr<descriptor_allocator> descriptors_;
    std::unique_ptr<command_queue> queue_;
    std::unique_ptr<upload_manager> uploads_;

    std::unique_ptr<resource::buffer_manager> buffers_;
    std::unique_ptr<resource::texture_manager> textures_;
    std::unique_ptr<resource::sampler_manager> samplers_;
    std::unique_ptr<resource::pipeline_manager> pipelines_;
    std::unique_ptr<resource::swap_chain_manager> swap_chains_;

    std::unique_ptr<transfer::manager> transfers_;
    std::unique_ptr<text::manager> texts_;
    std::unique_ptr<command_manager> commands_;
    std::unique_ptr<rendering::renderer> renderer_;
};

context* ctx() noexcept;
} // namespace nori::graphics
