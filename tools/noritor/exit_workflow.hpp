#pragma once
#include <span>
#include <vector>

#include <nori/resource/handle.hpp>

namespace nori::editor
{
class document_editor;

class exit_workflow
{
public:
    exit_workflow();

    bool requested() const;
    bool closing() const;

    void request();
    void cancel();
    void approve(nori::resource::handle handle);
    nori::resource::handle next_confirmation(std::span<const document_editor> documents) const;
    void begin_closing();

private:
    std::vector<nori::resource::handle> approved_documents_;
    bool requested_;
    bool closing_;
};
} // namespace nori::editor
