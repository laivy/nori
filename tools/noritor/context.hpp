#pragma once
#include <filesystem>
#include <optional>
#include <vector>
#include <imgui.h>
#include <nori/resource.hpp>

struct context
{
    struct root
    {
        std::filesystem::path path;
        nori::resource::handle handle;
        bool is_modified;
    };

    std::vector<root> roots;
    std::optional<nori::resource::handle> selected;
    ImGuiID dock_space_id;
};
