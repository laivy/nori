#include "pch.hpp"

#include <algorithm>
#include <cassert>

#include "document_editor.hpp"
#include "exit_workflow.hpp"

namespace nori::editor
{
exit_workflow::exit_workflow() :
    approved_documents_{},
    requested_{false},
    closing_{false}
{
}

bool exit_workflow::requested() const
{
    return requested_;
}

bool exit_workflow::closing() const
{
    return closing_;
}

void exit_workflow::request()
{
    requested_ = true;
    closing_ = false;
    approved_documents_.clear();
}

void exit_workflow::cancel()
{
    requested_ = false;
    closing_ = false;
    approved_documents_.clear();
}

void exit_workflow::approve(nori::resource::handle handle)
{
    if (!std::ranges::contains(approved_documents_, handle))
    {
        approved_documents_.push_back(handle);
    }
}

nori::resource::handle exit_workflow::next_confirmation(std::span<const document_editor> documents) const
{
    if (!requested_ || closing_)
    {
        return {};
    }
    for (const document_editor& target : documents)
    {
        if (target.dirty() && !std::ranges::contains(approved_documents_, target.root()))
        {
            return target.root();
        }
    }
    return {};
}

void exit_workflow::begin_closing()
{
    assert(requested_);
    closing_ = true;
}
} // namespace nori::editor
