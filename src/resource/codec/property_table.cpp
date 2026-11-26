#include <algorithm>
#include <cassert>
#include <limits>
#include <ostream>
#include <utility>

#include "format.hpp"
#include "property_table.hpp"

namespace nori::resource::codec
{
property_table property_table::decode(std::span<const std::byte> bytes)
{
    property_table table;
    const auto count{static_cast<std::uint32_t>(bytes.size() / entry_size)};
    table.entries_.reserve(count);
    for (std::uint32_t index{0}; index < count; ++index)
    {
        entry e{
            .name_id = decode_scalar<std::uint32_t>(bytes),
            .subtree_end = decode_scalar<std::uint32_t>(bytes),
            .type = property_type{decode_scalar<std::uint8_t>(bytes)},
        };
        std::ranges::copy(bytes.first(e.reserved.size()), e.reserved.begin());
        bytes = bytes.subspan(e.reserved.size());
        std::ranges::copy(bytes.first(e.value.size()), e.value.begin());
        bytes = bytes.subspan(e.value.size());
        table.entries_.push_back(e);
    }
    return table;
}

const property_table::entry& property_table::operator[](std::uint32_t index) const
{
    return entries_[index];
}

std::uint32_t property_table::size() const noexcept
{
    return static_cast<std::uint32_t>(entries_.size());
}

property_table::entry& property_table::encoder::operator[](std::uint32_t index) noexcept
{
    return entries_[index];
}

result<void> property_table::encoder::append(entry item, std::uint32_t depth)
{
    if (entries_.size() >= std::numeric_limits<std::uint32_t>::max())
    {
        return std::unexpected{error_code::invalid_format};
    }
    assert(depth <= ancestors_.size());
    const std::uint32_t index{static_cast<std::uint32_t>(entries_.size())};
    while (ancestors_.size() > depth)
    {
        entries_[ancestors_.back()].subtree_end = index - 1;
        ancestors_.pop_back();
    }
    item.subtree_end = index;
    entries_.push_back(item);
    ancestors_.push_back(index);
    return {};
}

result<void> property_table::encoder::encode(std::ostream& output)
{
    for (const auto ancestor : ancestors_)
    {
        entries_[ancestor].subtree_end = static_cast<std::uint32_t>(entries_.size() - 1);
    }
    ancestors_.clear();
    for (const auto& item : entries_)
    {
        std::array<std::byte, entry_size> bytes{};
        std::span<std::byte> remaining{bytes};
        encode_scalar(remaining, item.name_id);
        encode_scalar(remaining, item.subtree_end);
        encode_scalar(remaining, std::to_underlying(item.type));
        std::ranges::copy(item.reserved, remaining.begin());
        remaining = remaining.subspan(item.reserved.size());
        std::ranges::copy(item.value, remaining.begin());
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    if (!output)
    {
        return std::unexpected{error_code::io_failure};
    }
    return {};
}

std::uint32_t property_table::encoder::size() const noexcept
{
    return static_cast<std::uint32_t>(entries_.size());
}

std::uint64_t property_table::encoder::size_bytes() const noexcept
{
    return static_cast<std::uint64_t>(entries_.size()) * entry_size;
}
} // namespace nori::resource::codec
