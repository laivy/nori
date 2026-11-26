#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include <imgui.h>

#include <nori/core/vector.hpp>
#include <nori/graphics/texture.hpp>
#include <nori/resource/handle.hpp>

namespace nori::editor
{
class asset_manager
{
public:
    asset_manager();

    asset_manager(const asset_manager&) = delete;
    asset_manager(asset_manager&&) = delete;
    asset_manager& operator=(const asset_manager&) = delete;
    asset_manager& operator=(asset_manager&&) = delete;

    void initialize();
    void shutdown();

    void image(std::string_view path, std::optional<ImVec2> area = std::nullopt) const;
    bool image_button(std::string_view id, std::string_view path, std::optional<ImVec2> size = std::nullopt) const;

private:
    struct image_resource
    {
        nori::graphics::unique_texture_handle texture;
        nori::core::vec2 size;
    };

    void load_font(nori::resource::handle root);

    static image_resource load_image(nori::resource::handle root, std::string_view path);
    const image_resource& find_image(std::string_view path) const;

    std::unordered_map<std::string, image_resource> images_;

    bool font_loaded_{};
};
} // namespace nori::editor
