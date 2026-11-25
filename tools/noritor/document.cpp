#include "pch.hpp"

#include <utility>

#include "document.hpp"

namespace nori::editor
{
document::document(std::filesystem::path path, nori::resource::handle root) :
    path_{std::move(path)},
    root_{root},
    closing_{false}
{
}

const std::filesystem::path& document::path() const
{
    return path_;
}

nori::resource::handle document::root() const
{
    return root_;
}

bool document::dirty() const
{
    return command_manager_.dirty();
}

bool document::closing() const
{
    return closing_;
}

bool document::can_undo() const
{
    return command_manager_.can_undo();
}

bool document::can_redo() const
{
    return command_manager_.can_redo();
}

void document::mark_saved()
{
    command_manager_.mark_saved();
}

void document::set_closing(bool closing)
{
    closing_ = closing;
}
} // namespace nori::editor
