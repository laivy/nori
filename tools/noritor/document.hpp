#pragma once
#include <filesystem>

#include <nori/resource/handle.hpp>

#include "command.hpp"

namespace nori::editor
{
class document
{
    friend class document_manager;
    friend class document_editor;

public:
    document(std::filesystem::path path, nori::resource::handle root);

    const std::filesystem::path& path() const;
    nori::resource::handle root() const;

    bool dirty() const;
    bool closing() const;
    bool can_undo() const;
    bool can_redo() const;

private:
    void mark_saved();
    void set_closing(bool closing);

    std::filesystem::path path_;
    nori::resource::handle root_;

    command_manager command_manager_;
    bool closing_;
};
} // namespace nori::editor
