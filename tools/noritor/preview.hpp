#pragma once
#include <nori/graphics/error_code.hpp>

namespace nori::editor
{
class document_editor;
class document_workspace;
struct preview_state;

class preview
{
public:
    nori::graphics::result<void> update(document_editor& doc, document_workspace& workspace);
    void release(document_workspace& workspace) const;
    void draw(document_editor& doc, document_workspace& workspace);

    bool has_content(document_editor& doc, document_workspace& workspace) const;
};
} // namespace nori::editor
