#pragma once
#include <optional>
#include <string>
#include <unordered_set>

#include <nori/core/vector.hpp>
#include <nori/graphics/texture.hpp>
#include <nori/resource/value.hpp>

#include "command.hpp"

namespace nori::editor
{
struct hierarchy_state
{
    std::string filter;
    std::optional<nori::resource::property_type> type_filter;
    std::unordered_set<nori::resource::handle> open_nodes;
};

struct inspector_state
{
    bool focus_name;
};

struct preview_state
{
    nori::resource::handle source;
    nori::graphics::unique_texture_handle texture;
    nori::core::vec2 image_size;
    float zoom;
    bool reset_scroll;
    bool refresh;
};

class document_workspace
{
public:
    explicit document_workspace(nori::resource::handle root);

    auto& hierarchy(this auto& self)
    {
        return self.hierarchy_;
    }

    auto& inspector(this auto& self)
    {
        return self.inspector_;
    }

    auto& preview(this auto& self)
    {
        return self.preview_;
    }

    void apply(const command_effect& effect);

private:
    hierarchy_state hierarchy_;
    inspector_state inspector_;
    preview_state preview_;
};
} // namespace nori::editor
