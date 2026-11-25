#include "pch.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>

#include <Windows.h>

#include <nori/core/abort.hpp>
#include <nori/core/encoding.hpp>
#include <nori/resource/operations.hpp>

#include "document_manager.hpp"
#include "log.hpp"

namespace
{
using nori::editor::log;

std::expected<std::filesystem::path, std::error_code> normalize_document_path(const std::filesystem::path& path)
{
    std::error_code ec;
    const std::filesystem::path normalized{std::filesystem::weakly_canonical(path, ec)};
    if (ec)
    {
        return std::unexpected{ec};
    }
    return normalized.lexically_normal();
}

std::expected<bool, std::error_code> same_document_path(const std::filesystem::path& lhs, const std::filesystem::path& rhs)
{
    const auto normalized_lhs{normalize_document_path(lhs)};
    if (!normalized_lhs)
    {
        return std::unexpected{normalized_lhs.error()};
    }
    const auto normalized_rhs{normalize_document_path(rhs)};
    if (!normalized_rhs)
    {
        return std::unexpected{normalized_rhs.error()};
    }
    if (*normalized_lhs == *normalized_rhs)
    {
        return true;
    }

    std::error_code ec;
    const bool lhs_exists{std::filesystem::exists(*normalized_lhs, ec)};
    if (ec)
    {
        return std::unexpected{ec};
    }
    const bool rhs_exists{std::filesystem::exists(*normalized_rhs, ec)};
    if (ec)
    {
        return std::unexpected{ec};
    }
    if (lhs_exists && rhs_exists)
    {
        const bool equivalent{std::filesystem::equivalent(*normalized_lhs, *normalized_rhs, ec)};
        if (ec)
        {
            return std::unexpected{ec};
        }
        return equivalent;
    }

    return CompareStringOrdinal(
               normalized_lhs->native().c_str(), -1, normalized_rhs->native().c_str(), -1, TRUE
           ) == CSTR_EQUAL;
}
} // namespace

namespace nori::editor
{
document_manager::~document_manager()
{
    if (!documents_.empty() || active_root_)
    {
        nori::core::abort("document manager destroyed before close_all");
    }
}

nori::resource::result<document_editor*> document_manager::create()
{
    std::error_code ec;
    const std::filesystem::path current{std::filesystem::current_path(ec)};
    if (ec)
    {
        log::error(std::format("생성 실패: {}", nori::resource::to_string(nori::resource::error_code::io_failure)));
        return std::unexpected{nori::resource::error_code::io_failure};
    }
    for (std::uint64_t index{1};; ++index)
    {
        const auto path{normalize_document_path(current / std::format("untitled{}.nori", index))};
        if (!path)
        {
            log::error(std::format("생성 실패: {}", nori::resource::to_string(nori::resource::error_code::io_failure)));
            return std::unexpected{nori::resource::error_code::io_failure};
        }
        const bool exists{std::filesystem::exists(*path, ec)};
        if (ec)
        {
            log::error(std::format("생성 실패 ('{}'): {}", nori::core::path_to_utf8(*path), nori::resource::to_string(nori::resource::error_code::io_failure)));
            return std::unexpected{nori::resource::error_code::io_failure};
        }
        if (exists)
        {
            continue;
        }
        bool document_conflict{};
        for (const document_editor& existing : documents_)
        {
            const auto same{same_document_path(*path, existing.path())};
            if (!same)
            {
                log::error(std::format("생성 실패 ('{}'): {}", nori::core::path_to_utf8(*path), nori::resource::to_string(nori::resource::error_code::io_failure)));
                return std::unexpected{nori::resource::error_code::io_failure};
            }
            document_conflict |= *same;
        }
        if (document_conflict)
        {
            continue;
        }
        const std::string key{nori::core::path_to_utf8(*path)};
        const auto existing{nori::resource::try_get(key)};
        if (existing)
        {
            continue;
        }
        if (existing.error() != nori::resource::error_code::not_found)
        {
            log::error(std::format("생성 실패 ('{}'): {}", key, nori::resource::to_string(existing.error())));
            return std::unexpected{existing.error()};
        }
        const nori::resource::handle root{nori::resource::create_root()};
        documents_.emplace_back(*path, root);
        active_root_ = root;
        log::info(std::format("생성: '{}'", key));
        return &documents_.back();
    }
}

nori::resource::result<document_editor*> document_manager::open(const std::filesystem::path& path)
{
    const auto normalized{normalize_document_path(path)};
    if (!normalized)
    {
        log::error(std::format("열기 실패 ('{}'): {}", nori::core::path_to_utf8(path), nori::resource::to_string(nori::resource::error_code::io_failure)));
        return std::unexpected{nori::resource::error_code::io_failure};
    }
    for (document_editor& existing : documents_)
    {
        const auto same{same_document_path(*normalized, existing.path())};
        if (!same)
        {
            log::error(std::format("열기 실패 ('{}'): {}", nori::core::path_to_utf8(*normalized), nori::resource::to_string(nori::resource::error_code::io_failure)));
            return std::unexpected{nori::resource::error_code::io_failure};
        }
        if (*same)
        {
            active_root_ = existing.root();
            log::info(std::format("이미 열려 있습니다: '{}'", nori::core::path_to_utf8(*normalized)));
            return &existing;
        }
    }
    const std::string key{nori::core::path_to_utf8(*normalized)};
    const auto root{nori::resource::try_get(key)};
    if (!root)
    {
        log::error(std::format("열기 실패 ('{}'): {}", key, nori::resource::to_string(root.error())));
        return std::unexpected{root.error()};
    }
    if (const auto res{nori::resource::load(*root)}; !res)
    {
        nori::resource::remove(*root);
        log::error(std::format("열기 실패 ('{}'): {}", key, nori::resource::to_string(res.error())));
        return std::unexpected{res.error()};
    }
    documents_.emplace_back(*normalized, *root);
    documents_.back().document_.mark_saved();
    active_root_ = *root;
    log::info(std::format("열기: '{}'", key));
    return &documents_.back();
}

nori::resource::result<void> document_manager::save(document_editor& target)
{
    if (target.closing())
    {
        log::error(std::format("저장 실패 ('{}'): {}", nori::core::path_to_utf8(target.path()), nori::resource::to_string(nori::resource::error_code::invalid_relationship)));
        return std::unexpected{nori::resource::error_code::invalid_relationship};
    }
    const nori::resource::path current_path{nori::resource::path_of(target.root())};
    const auto result{!current_path.empty() ? nori::resource::save(target.root()) : nori::resource::save_as(target.root(), target.path())};
    if (!result)
    {
        log::error(std::format("저장 실패 ('{}'): {}", nori::core::path_to_utf8(target.path()), nori::resource::to_string(result.error())));
        return result;
    }
    target.document_.mark_saved();
    log::info(std::format("저장: '{}'", nori::core::path_to_utf8(target.path())));
    return {};
}

nori::resource::result<save_as_status> document_manager::save_as(document_editor& target, const std::filesystem::path& path, save_as_policy policy)
{
    if (target.closing())
    {
        log::error(std::format("저장 실패 ('{}'): {}", nori::core::path_to_utf8(path), nori::resource::to_string(nori::resource::error_code::invalid_relationship)));
        return std::unexpected{nori::resource::error_code::invalid_relationship};
    }
    const auto normalized{normalize_document_path(path)};
    if (!normalized)
    {
        log::error(std::format("저장 실패 ('{}'): {}", nori::core::path_to_utf8(path), nori::resource::to_string(nori::resource::error_code::io_failure)));
        return std::unexpected{nori::resource::error_code::io_failure};
    }
    const auto same_target{same_document_path(target.path(), *normalized)};
    if (!same_target)
    {
        log::error(std::format("저장 실패 ('{}'): {}", nori::core::path_to_utf8(*normalized), nori::resource::to_string(nori::resource::error_code::io_failure)));
        return std::unexpected{nori::resource::error_code::io_failure};
    }
    if (*same_target)
    {
        const auto saved{save(target)};
        if (!saved)
        {
            return std::unexpected{saved.error()};
        }
        return save_as_status::saved;
    }
    for (const document_editor& existing : documents_)
    {
        if (&existing == &target)
        {
            continue;
        }
        const auto same{same_document_path(existing.path(), *normalized)};
        if (!same)
        {
            log::error(std::format("저장 실패 ('{}'): {}", nori::core::path_to_utf8(*normalized), nori::resource::to_string(nori::resource::error_code::io_failure)));
            return std::unexpected{nori::resource::error_code::io_failure};
        }
        if (*same)
        {
            log::error(std::format("저장 실패 ('{}'): {}", nori::core::path_to_utf8(*normalized), nori::resource::to_string(nori::resource::error_code::name_conflict)));
            return std::unexpected{nori::resource::error_code::name_conflict};
        }
    }

    std::error_code ec;
    const bool exists{std::filesystem::exists(*normalized, ec)};
    if (ec)
    {
        log::error(std::format("저장 실패 ('{}'): {}", nori::core::path_to_utf8(*normalized), nori::resource::to_string(nori::resource::error_code::io_failure)));
        return std::unexpected{nori::resource::error_code::io_failure};
    }
    if (exists && policy == save_as_policy::require_confirmation)
    {
        return save_as_status::overwrite_required;
    }

    const std::string new_key{nori::core::path_to_utf8(*normalized)};
    const auto saved{nori::resource::save_as(target.root(), *normalized)};
    if (!saved)
    {
        log::error(std::format("저장 실패 ('{}'): {}", new_key, nori::resource::to_string(saved.error())));
        return std::unexpected{saved.error()};
    }
    target.document_.path_ = *normalized;
    target.document_.mark_saved();
    log::info(std::format("저장: '{}'", new_key));
    return save_as_status::saved;
}

nori::resource::result<void> document_manager::save_all()
{
    std::optional<nori::resource::error_code> first_error;
    for (document_editor& target : documents_)
    {
        if (target.closing())
        {
            continue;
        }
        if (const auto res{save(target)}; !res && !first_error)
        {
            first_error = res.error();
        }
    }
    if (first_error)
    {
        return std::unexpected{*first_error};
    }
    return {};
}

void document_manager::begin_close(document_editor& target)
{
    target.document_.set_closing(true);
}

void document_manager::close(document_editor& target)
{
    assert(target.closing());
    const auto it{std::ranges::find_if(documents_, [&target](const document_editor& value)
    {
        return &value == &target;
    })};
    assert(it != documents_.end());
    const std::size_t index{static_cast<std::size_t>(std::distance(documents_.begin(), it))};
    nori::resource::handle next{};
    if (active_root_ == target.root() && documents_.size() > 1)
    {
        next = index > 0 ? documents_[index - 1].root() : documents_[index + 1].root();
    }
    else if (active_root_ != target.root())
    {
        next = active_root_;
    }
    const std::string path{nori::core::path_to_utf8(target.path())};
    target.document_.command_manager_.clear();
    nori::resource::remove(target.root());
    documents_.erase(it);
    active_root_ = next;
    log::info(std::format("닫기: '{}'", path));
}

void document_manager::close_all()
{
    for (document_editor& target : documents_)
    {
        target.document_.set_closing(true);
    }
    while (!documents_.empty())
    {
        close(documents_.back());
    }
}

void document_manager::set_active(nori::resource::handle handle)
{
    if (document_editor* const target{find(handle)})
    {
        active_root_ = target->root();
    }
}
} // namespace nori::editor
