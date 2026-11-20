#pragma once
#include "document_editor.hpp"

namespace nori::editor
{
class clipboard
{
public:
    explicit clipboard(temporary_storage* temporary);
    ~clipboard();

    clipboard(const clipboard&) = delete;
    clipboard(clipboard&&) = delete;
    clipboard& operator=(const clipboard&) = delete;
    clipboard& operator=(clipboard&&) = delete;

    nori::resource::result<void> copy(const document_editor& source_document);
    nori::resource::result<command_effect> cut(document_editor& source_document);
    nori::resource::result<command_effect> paste(document_editor& destination_document, nori::resource::handle parent);

    bool empty() const;
    void clear();

private:
    nori::resource::result<std::vector<nori::resource::handle>> stage(const document_editor& source_document);

    temporary_storage* temporary_;
    std::vector<nori::resource::handle> handles_;
};
} // namespace nori::editor
