#pragma once
#include <algorithm>
#include <filesystem>
#include <span>
#include <vector>

#include <nori/resource/operations.hpp>

#include "document_editor.hpp"

namespace nori::editor
{
enum class save_as_policy
{
    require_confirmation,
    overwrite
};

enum class save_as_status
{
    saved,
    overwrite_required
};

class document_manager
{
public:
    ~document_manager();

    nori::resource::result<document_editor*> create();
    nori::resource::result<document_editor*> open(const std::filesystem::path& path);

    nori::resource::result<void> save(document_editor& target);
    nori::resource::result<save_as_status> save_as(document_editor& target, const std::filesystem::path& path, save_as_policy policy = save_as_policy::require_confirmation);
    nori::resource::result<void> save_all();

    void begin_close(document_editor& target);
    void close(document_editor& target);
    void close_all();

    auto documents(this auto& self)
    {
        return std::span{self.documents_};
    }

    auto* find_root(this auto& self, nori::resource::handle root)
    {
        const auto it{std::ranges::find(self.documents_, root, &document_editor::root)};
        return it == self.documents_.end() ? nullptr : &*it;
    }

    auto find(this auto& self, nori::resource::handle handle) -> decltype(self.find_root(handle))
    {
        if (const auto result{self.find_root(handle)})
        {
            return result;
        }
        for (nori::resource::handle parent{nori::resource::parent_of(handle)}; parent; parent = nori::resource::parent_of(parent))
        {
            if (const auto result{self.find_root(parent)})
            {
                return result;
            }
        }
        return nullptr;
    }

    auto* active(this auto& self)
    {
        return self.active_root_ ? self.find_root(self.active_root_) : nullptr;
    }

    void set_active(nori::resource::handle handle);

private:
    std::vector<document_editor> documents_;
    nori::resource::handle active_root_{};
};
} // namespace nori::editor
