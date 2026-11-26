#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include <nori/graphics/buffer.hpp>
#include <nori/graphics/error_code.hpp>
#include <nori/graphics/rect.hpp>
#include <nori/graphics/text.hpp>
#include <nori/graphics/texture.hpp>
#include <nori/resource/handle.hpp>

#include "text/layout.hpp"
#include "text/pipeline_cache.hpp"

namespace nori::graphics::resource
{
class buffer_manager;
class pipeline_manager;
class sampler_manager;
class texture_manager;
struct render_target_signature;
} // namespace nori::graphics::resource

namespace nori::graphics::text
{
class manager
{
public:
    struct params
    {
        resource::buffer_manager* buffers;
        resource::texture_manager* textures;
        resource::sampler_manager* samplers;
        resource::pipeline_manager* pipelines;
    };

    ~manager() noexcept;

    manager(const manager&) = delete;
    manager(manager&&) = delete;
    manager& operator=(const manager&) = delete;
    manager& operator=(manager&&) = delete;

    static std::unique_ptr<manager> create(params args);

    result<font_handle> create_font(font_desc desc);
    result<font_handle> create_font(nori::resource::handle source, std::uint32_t face_index);
    void destroy(font_handle handle);

    result<layout_handle> create_layout(layout_desc desc, std::string_view utf8);
    result<layout_handle> create_layout(layout_desc desc, std::span<const item> items);
    result<void> update(layout_handle handle, std::string_view utf8);
    result<void> update(layout_handle handle, layout_desc desc, std::string_view utf8);
    result<void> update(layout_handle handle, std::span<const item> items);
    result<void> update(layout_handle handle, layout_desc desc, std::span<const item> items);
    void destroy(layout_handle handle);

    std::optional<render_state> prepare_render_state(const resource::render_target_signature& target, shader_handle vertex_shader = {}, shader_handle pixel_shader = {});
    void invalidate(shader_handle shader);

    [[nodiscard]] const layout& at(layout_handle handle) const noexcept;
    layout_metrics metrics(layout_handle handle) const;

private:
    class impl;

    explicit manager(std::unique_ptr<impl> implementation) noexcept;

    std::unique_ptr<impl> impl_;
};
} // namespace nori::graphics::text
