#include "pch.hpp"

#include <array>
#include <tuple>

#include <nori/core/abort.hpp>
#include <nori/graphics/imgui.hpp>
#include <nori/resource/path.hpp>

#include "asset_manager.hpp"

namespace
{
const nori::resource::path icon_root{"icon"};
constexpr std::array<std::string_view, 12> image_paths{
    "16/binary",
    "16/search",
    "16/folder",
    "16/bool",
    "16/digit",
    "16/real",
    "16/string",
    "16/image",
    "16/nori",
    "16/delete",
    "64/folder",
    "64/file",
};
} // namespace

namespace nori::editor
{
asset_manager::asset_manager()
{
}

void asset_manager::initialize()
{
    shutdown();
    const auto root{nori::resource::try_get("ui.nori")};
    if (root)
    {
        load_font(*root);
    }
    for (const std::string_view path : image_paths)
    {
        images_.emplace(path, root ? load_image(*root, path) : image_resource{});
    }
}

void asset_manager::shutdown()
{
    images_.clear();
    if (font_loaded_)
    {
        ImGuiIO& io{ImGui::GetIO()};
        io.FontDefault = nullptr;
        io.Fonts->Clear();
        font_loaded_ = false;
    }
}

void asset_manager::image(std::string_view path, std::optional<ImVec2> area) const
{
    const image_resource& resource{find_image(path)};
    const ImVec2 image_size{resource.size.x, resource.size.y};
    if (!resource.texture || image_size.x <= 0.0f || image_size.y <= 0.0f)
    {
        if (area)
        {
            ImGui::Dummy(*area);
        }
        return;
    }

    if (!area)
    {
        ImGui::Image(nori::graphics::imgui::texture_id(resource.texture.get()), image_size);
        return;
    }

    const ImVec2 area_min{ImGui::GetCursorScreenPos()};
    ImGui::SetCursorScreenPos(area_min + (*area - image_size) * 0.5f);
    ImGui::Image(nori::graphics::imgui::texture_id(resource.texture.get()), image_size);
    ImGui::SetCursorScreenPos(area_min);
    ImGui::Dummy(*area);
}

bool asset_manager::image_button(std::string_view id, std::string_view path, std::optional<ImVec2> size) const
{
    const image_resource& resource{find_image(path)};
    const ImVec2 display_size{size.value_or(ImVec2{resource.size.x, resource.size.y})};
    if (!resource.texture)
    {
        const std::string label{id};
        return ImGui::Button(label.c_str(), display_size);
    }
    const std::string label{id};
    return ImGui::ImageButton(label.c_str(), nori::graphics::imgui::texture_id(resource.texture.get()), display_size);
}

void asset_manager::load_font(nori::resource::handle root)
{
    const auto source{nori::resource::try_get(root, "font/d2coding")};
    if (!source)
    {
        return;
    }
    const auto binary{nori::resource::try_get<nori::resource::binary_view>(*source)};
    if (!binary || binary->data.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        return;
    }

    auto font_data{std::make_unique<std::byte[]>(binary->data.size())};
    if (!font_data)
    {
        return;
    }
    std::memcpy(font_data.get(), binary->data.data(), binary->data.size());

    ImGuiIO& io{ImGui::GetIO()};
    io.Fonts->Clear();
    font_loaded_ = true;

    ImFont* const font{io.Fonts->AddFontFromMemoryTTF(font_data.get(), static_cast<int>(binary->data.size()))};
    if (!font)
    {
        io.FontDefault = io.Fonts->AddFontDefault();
        return;
    }

    io.FontDefault = font;
    font_data.release();
}

asset_manager::image_resource asset_manager::load_image(nori::resource::handle root, std::string_view path)
{
    const nori::resource::path resource_path{icon_root / path};
    const auto source{nori::resource::try_get(root, resource_path)};
    if (!source)
    {
        return {};
    }

    const auto image{nori::resource::try_get<nori::resource::image_view>(*source)};
    if (!image)
    {
        return {};
    }

    auto texture{nori::graphics::create_texture(*source)};
    if (!texture)
    {
        return {};
    }

    return image_resource{
        .texture = std::move(*texture),
        .size = {
            static_cast<float>(image->width),
            static_cast<float>(image->height),
        },
    };
}

const asset_manager::image_resource& asset_manager::find_image(std::string_view path) const
{
    const auto it{images_.find(std::string{path})};
    if (it == images_.end())
    {
        nori::core::abort("requested UI image was not loaded");
    }
    return it->second;
}
} // namespace nori::editor
