#include "pch.hpp"

#include <expected>
#include <memory>
#include <utility>
#include <vector>

#include <nori/resource/operations.hpp>

#include "clipboard.hpp"
#include "command.hpp"
#include "document_editor.hpp"
#include "temporary_storage.hpp"

namespace nori::editor
{
clipboard::clipboard(temporary_storage* temporary) :
    temporary_{temporary}
{
}

clipboard::~clipboard()
{
    clear();
}

nori::resource::result<void> clipboard::copy(const document_editor& source_document)
{
    if (source_document.selection().selection().empty())
    {
        clear();
        return {};
    }
    auto staged{stage(source_document)};
    if (!staged)
    {
        return std::unexpected{staged.error()};
    }
    clear();
    handles_ = std::move(*staged);
    return {};
}

nori::resource::result<command_effect> clipboard::cut(document_editor& source_document)
{
    if (source_document.closing())
    {
        return std::unexpected{nori::resource::error_code::invalid_relationship};
    }
    if (source_document.selection().selection().empty())
    {
        clear();
        return command_effect{};
    }
    const std::vector<nori::resource::handle> selected{
        source_document.selection().top_level()
    };
    auto staged{stage(source_document)};
    if (!staged)
    {
        return std::unexpected{staged.error()};
    }

    auto command{std::make_unique<remove_command>(temporary_, source_document.root(), selected)};
    auto effect{source_document.execute(std::move(command))};
    if (!effect)
    {
        for (const nori::resource::handle staged_handle : *staged)
        {
            temporary_->remove(staged_handle);
        }
        return effect;
    }
    clear();
    handles_ = std::move(*staged);
    return effect;
}

nori::resource::result<command_effect> clipboard::paste(document_editor& destination_document, nori::resource::handle parent)
{
    if (destination_document.closing())
    {
        return std::unexpected{nori::resource::error_code::invalid_relationship};
    }
    if (handles_.empty())
    {
        return command_effect{};
    }
    std::vector<copy_source> sources;
    sources.reserve(handles_.size());
    for (const nori::resource::handle handle : handles_)
    {
        sources.push_back({handle, parent, nori::resource::name_of(handle)});
    }
    return destination_document.copy(*temporary_, std::move(sources));
}

bool clipboard::empty() const
{
    return handles_.empty();
}

void clipboard::clear()
{
    for (const nori::resource::handle handle : handles_)
    {
        temporary_->remove(handle);
    }
    handles_.clear();
}

nori::resource::result<std::vector<nori::resource::handle>> clipboard::stage(const document_editor& source_document)
{
    const std::vector<nori::resource::handle> selected{
        source_document.selection().top_level()
    };
    std::vector<nori::resource::handle> staged;
    staged.reserve(selected.size());
    for (const nori::resource::handle handle : selected)
    {
        const auto stored{temporary_->store(handle)};
        if (!stored)
        {
            for (const nori::resource::handle staged_handle : staged)
            {
                temporary_->remove(staged_handle);
            }
            return std::unexpected{stored.error()};
        }
        staged.push_back(*stored);
    }
    return staged;
}
} // namespace nori::editor
