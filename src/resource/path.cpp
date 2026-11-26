#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

#include <nori/core/encoding.hpp>
#include <nori/resource/path.hpp>

namespace
{
std::string normalize_file_path(std::string_view value)
{
    std::string result{value};
    std::ranges::replace(result, '\\', '/');
    return result;
}
} // namespace

namespace nori::resource
{
bool is_valid_property_name(std::string_view name) noexcept
{
    return !name.empty() &&
        name != "." &&
        name != ".." &&
        !name.contains('\0') &&
        !name.contains(path::separator) &&
        !name.contains(path::subpath_delimiter) &&
        !name.ends_with(path::extension);
}

path::path(const char* value) :
    value_{value ? value : ""}
{
}

path::path(std::string_view value) :
    value_{value}
{
}

path::path(std::string value) :
    value_{std::move(value)}
{
}

path::path(const std::filesystem::path& value) :
    value_{core::path_to_utf8(value)}
{
}

path path::operator/(const path& rhs) const
{
    if (empty())
    {
        return rhs;
    }
    if (rhs.empty() || rhs.has_file_path())
    {
        return rhs.empty() ? *this : rhs;
    }

    std::string result{value_};
    if (has_file_path() && value_.find(subpath_delimiter) == std::string_view::npos)
    {
        result.append(subpath_delimiter);
    }
    else if (!result.ends_with(separator) && !result.ends_with(subpath_delimiter))
    {
        result.push_back(separator);
    }
    result.append(rhs.value_);
    return path{std::move(result)};
}

bool path::empty() const noexcept
{
    return value_.empty();
}

bool path::has_file_path() const noexcept
{
    const std::string_view value{value_};
    return value.find(subpath_delimiter) != std::string_view::npos || value.ends_with(extension);
}

bool path::has_subpath() const noexcept
{
    return !subpath().empty();
}

std::string path::string() const
{
    return value_;
}

std::string_view path::string_view() const noexcept
{
    return value_;
}

std::filesystem::path path::file_path() const
{
    if (!has_file_path())
    {
        return {};
    }

    const std::string_view value{value_};
    const std::size_t delimiter_pos{value.find(subpath_delimiter)};
    const std::string_view file{value.substr(0, delimiter_pos)};
    return core::utf8_to_path(normalize_file_path(file));
}

std::string_view path::subpath() const noexcept
{
    const std::string_view value{value_};
    if (const std::size_t delimiter_pos{value.find(subpath_delimiter)}; delimiter_pos != std::string_view::npos)
    {
        return value.substr(delimiter_pos + subpath_delimiter.size());
    }
    return value.ends_with(extension) ? std::string_view{} : value;
}

subpath_iterator::subpath_iterator() noexcept :
    offset_{std::string_view::npos}
{
}

subpath_iterator::subpath_iterator(const path& value) :
    value_{value.subpath()},
    offset_{value_.empty() ? std::string_view::npos : 0}
{
}

subpath_iterator::value_type subpath_iterator::operator*() const noexcept
{
    const std::string_view text{value_};
    const std::size_t next{text.find(path::separator, offset_)};
    return text.substr(offset_, next == std::string_view::npos ? next : next - offset_);
}

subpath_iterator& subpath_iterator::operator++() noexcept
{
    const std::size_t next{value_.find(path::separator, offset_)};
    offset_ = next == std::string_view::npos ? next : next + 1;
    return *this;
}

subpath_iterator subpath_iterator::operator++(int)
{
    subpath_iterator previous{*this};
    ++*this;
    return previous;
}

bool subpath_iterator::operator==(const subpath_iterator& rhs) const noexcept
{
    return offset_ == rhs.offset_ && (offset_ == std::string_view::npos || value_ == rhs.value_);
}

subpath_iterator begin(subpath_iterator it) noexcept
{
    return it;
}

subpath_iterator end(const subpath_iterator&) noexcept
{
    return {};
}
} // namespace nori::resource
