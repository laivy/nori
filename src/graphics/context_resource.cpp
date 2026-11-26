#include "context.hpp"
#include "resource/buffer_manager.hpp"
#include "resource/pipeline_manager.hpp"
#include "resource/sampler_manager.hpp"
#include "resource/swap_chain_manager.hpp"
#include "resource/texture_manager.hpp"
#include "text/manager.hpp"
#include "transfer/manager.hpp"

namespace nori::graphics
{
result<swap_chain_handle> context::create_swap_chain(swap_chain_desc desc)
{
    return swap_chains_->create(desc);
}

texture_handle context::back_buffer(swap_chain_handle handle) const
{
    return swap_chains_->back_buffer(handle);
}

result<void> context::resize_swap_chain(swap_chain_handle handle, std::uint32_t width, std::uint32_t height)
{
    return swap_chains_->resize(handle, width, height);
}

void context::destroy(swap_chain_handle handle)
{
    return swap_chains_->destroy(handle);
}

result<texture_handle> context::create_texture(texture_desc desc)
{
    return textures_->create(desc);
}

result<texture_handle> context::create_texture(nori::resource::handle source)
{
    return textures_->create(source);
}

result<void> context::update_texture(texture_handle handle, texture_region region, texture_data data)
{
    return textures_->update(handle, region, data);
}

void context::destroy(texture_handle handle)
{
    return textures_->destroy(handle);
}

result<buffer_handle> context::create_buffer(buffer_desc desc, std::span<const std::byte> initial_data)
{
    return buffers_->create(desc, initial_data);
}

result<frame_upload_buffer_view> context::create_frame_upload_buffer(buffer_usage usage, std::span<const std::byte> data)
{
    return buffers_->create_frame_upload_buffer(usage, data);
}

result<void> context::update_buffer(buffer_handle handle, std::uint64_t offset, std::span<const std::byte> data)
{
    return buffers_->update(handle, offset, data);
}

void context::destroy(buffer_handle handle)
{
    return buffers_->destroy(handle);
}

shader_handle context::create_shader(std::span<const std::byte> bytecode)
{
    return pipelines_->create_shader(bytecode);
}

void context::destroy(shader_handle handle)
{
    texts_->invalidate(handle);
    return pipelines_->destroy(handle);
}

pipeline_handle context::create_graphics_pipeline(const graphics_pipeline_desc& desc)
{
    return pipelines_->create_pipeline(desc);
}

void context::destroy(pipeline_handle handle)
{
    return pipelines_->destroy(handle);
}

sampler_handle context::create_sampler(sampler_desc desc)
{
    return samplers_->create(desc);
}

void context::destroy(sampler_handle handle)
{
    return samplers_->destroy(handle);
}

result<void> context::copy_buffer(buffer_handle source, std::uint64_t source_offset, buffer_handle destination, std::uint64_t destination_offset, std::uint64_t size)
{
    return transfers_->copy_buffer(source, source_offset, destination, destination_offset, size);
}

result<void> context::copy_texture(texture_handle source, texture_region source_region, texture_handle destination, texture_region destination_region)
{
    return transfers_->copy_texture(source, source_region, destination, destination_region);
}

result<void> context::blit_texture(texture_handle source, texture_region source_region, texture_handle destination, texture_region destination_region, blit_filter filter)
{
    return transfers_->blit_texture(source, source_region, destination, destination_region, filter);
}

result<void> context::generate_mips(texture_handle texture)
{
    return transfers_->generate_mips(texture);
}

result<readback_handle> context::read_texture(texture_handle texture, texture_region region)
{
    return transfers_->read_texture(texture, region);
}

readback_result context::poll(readback_handle handle)
{
    return transfers_->poll(handle);
}

void context::destroy(readback_handle handle)
{
    return transfers_->destroy(handle);
}
} // namespace nori::graphics
