#include <filesystem>
#include <mutex>
#include <string>
#include <utility>

#include <nori/core/encoding.hpp>

#include "file_registry.hpp"

namespace
{
bool is_same_file(const std::filesystem::path& lhs, const std::filesystem::path& rhs)
{
    if (lhs == rhs)
    {
        return true;
    }

    std::error_code ec;
    const bool equivalent{std::filesystem::equivalent(lhs, rhs, ec)};
    return !ec && equivalent;
}

bool is_valid_file_path(const std::filesystem::path& file_path)
{
    const std::string value{nori::core::path_to_utf8(file_path)};
    return !value.contains('\0') &&
        !value.contains(nori::resource::path::subpath_delimiter) &&
        value.ends_with(nori::resource::path::extension);
}
} // namespace

namespace nori::resource
{
file_registry::file_registry(std::filesystem::path root_path) :
    root_path_{std::move(root_path)}
{
}

handle file_registry::bind(handle root, const std::filesystem::path& file_path)
{
    std::unique_lock lock{mutex_};
    for (const auto& [r, p] : files_)
    {
        if (r != root && is_same_file(p, file_path))
        {
            return r;
        }
    }
    files_.insert_or_assign(root, file_path);
    return root;
}

void file_registry::unbind(handle root)
{
    std::unique_lock lock{mutex_};
    files_.erase(root);
}

std::filesystem::path file_registry::file_path_of(handle root) const
{
    std::shared_lock lock{mutex_};
    const auto it{files_.find(root)};
    if (it == files_.end())
    {
        return {};
    }
    return it->second;
}

handle file_registry::find(const std::filesystem::path& file_path) const
{
    std::shared_lock lock{mutex_};
    for (const auto& [root, registered_file_path] : files_)
    {
        if (is_same_file(registered_file_path, file_path))
        {
            return root;
        }
    }
    return handle{};
}

result<std::filesystem::path> file_registry::absolute(const std::filesystem::path& input) const
{
    std::filesystem::path file_path{input};
    if (!is_valid_file_path(file_path))
    {
        return std::unexpected{error_code::invalid_path};
    }

    if (file_path.is_relative())
    {
        file_path = root_path_ / file_path;
    }

    std::error_code ec;
    file_path = std::filesystem::weakly_canonical(file_path, ec);
    if (ec || !file_path.is_absolute())
    {
        return std::unexpected{error_code::io_failure};
    }
    return file_path;
}

std::filesystem::path normalize_root_path(std::filesystem::path p)
{
    std::error_code ec;
    if (p.empty())
    {
        p = std::filesystem::current_path(ec);
    }
    else if (p.is_relative())
    {
        p = std::filesystem::absolute(p, ec);
    }

    if (ec)
    {
        return {};
    }

    p = std::filesystem::weakly_canonical(p, ec);
    if (ec)
    {
        return {};
    }
    return p;
}
} // namespace nori::resource
