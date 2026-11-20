#include "pch.hpp"

#include <algorithm>

#include "document_workspace.hpp"

namespace nori::editor
{
document_workspace::document_workspace(nori::resource::handle root) :
    hierarchy_{.open_nodes = {root}},
    inspector_{},
    preview_{.zoom = 1.0f, .reset_scroll = true}
{
}

void document_workspace::apply(const command_effect& effect)
{
    for (const nori::resource::handle handle : effect.invalidated)
    {
        hierarchy_.open_nodes.erase(handle);
    }
    if (std::ranges::contains(effect.updated, preview_.source) || std::ranges::contains(effect.invalidated, preview_.source))
    {
        preview_.refresh = true;
    }
}
} // namespace nori::editor
