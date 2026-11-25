#include <array>
#include <limits>
#include <ostream>
#include <utility>

#include <nori/resource/path.hpp>

#include "format.hpp"
#include "name_table.hpp"

namespace nori::resource::codec
{
name_table name_table::decode(std::span<const std::byte> bytes)
{
    name_table table;
    while (!bytes.empty())
    {
        const std::uint16_t length{decode_scalar<std::uint16_t>(bytes)};
        const std::string_view name{reinterpret_cast<const char*>(bytes.data()), length};
        table.names_.emplace_back(name);
        bytes = bytes.subspan(length);
    }
    return table;
}

std::string_view name_table::operator[](std::uint32_t id) const
{
    return names_[id];
}

std::uint32_t name_table::size() const noexcept
{
    return static_cast<std::uint32_t>(names_.size());
}

result<std::uint32_t> name_table::encoder::intern(std::string_view name)
{
    if (!is_valid_property_name(name) || name.size() > std::numeric_limits<std::uint16_t>::max())
    {
        return std::unexpected{error_code::invalid_format};
    }

    std::string key{name};
    if (const auto it{names_.find(key)}; it != names_.end())
    {
        return it->second;
    }

    if (names_.size() >= std::numeric_limits<std::uint32_t>::max())
    {
        return std::unexpected{error_code::invalid_format};
    }

    const auto [it, _]{names_.emplace(std::move(key), size())};
    names_by_id_.push_back(&it->first);
    return it->second;
}

result<void> name_table::encoder::encode(std::ostream& output) const
{
    for (const auto name : names_by_id_)
    {
        std::array<std::byte, length_size> length{};
        encode_scalar(length, static_cast<std::uint16_t>(name->size()));
        output.write(reinterpret_cast<const char*>(length.data()), static_cast<std::streamsize>(length.size()));
        output.write(name->data(), static_cast<std::streamsize>(name->size()));
    }
    if (!output)
    {
        return std::unexpected{error_code::io_failure};
    }
    return {};
}

std::uint32_t name_table::encoder::size() const noexcept
{
    return static_cast<std::uint32_t>(names_.size());
}

std::uint64_t name_table::encoder::size_bytes() const noexcept
{
    std::uint64_t total{0};
    for (const auto name : names_by_id_)
    {
        total += length_size + name->size();
    }
    return total;
}
} // namespace nori::resource::codec
