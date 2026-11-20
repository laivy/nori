#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <ranges>
#include <utility>

#include <nori/core/hash.hpp>

#include "data_pool.hpp"

namespace nori::resource::codec
{
data_pool::encoder::encoder(std::fstream& file, std::uint64_t base_offset) :
    file_{file},
    base_offset_{base_offset},
    size_{0},
    is_cursor_at_end_{false}
{
}

result<data_pool::location> data_pool::encoder::intern(property_type type, std::span<const std::span<const std::byte>> chunks)
{
    if (!file_)
    {
        return std::unexpected{error_code::io_failure};
    }

    std::uint64_t length{0};
    std::size_t hash{std::to_underlying(type)};
    for (const auto chunk : chunks)
    {
        length += chunk.size();
        std::size_t offset{0};
        for (; chunk.size() - offset >= sizeof(std::size_t); offset += sizeof(std::size_t))
        {
            std::size_t word{};
            std::memcpy(&word, chunk.data() + offset, sizeof(word));
            core::hash_combine(hash, word);
        }
        for (const auto byte : chunk.subspan(offset))
        {
            core::hash_combine(hash, std::to_integer<std::size_t>(byte));
        }
    }

    std::vector<entry>& candidates{entries_[hash]};
    for (const auto& candidate : candidates)
    {
        if (candidate.type != type || candidate.loc.size != length)
        {
            continue;
        }
        if (const auto res{seek(candidate.loc.offset)}; !res)
        {
            return std::unexpected{res.error()};
        }
        const auto equal{equals(chunks)};
        if (!equal)
        {
            return std::unexpected{equal.error()};
        }
        if (*equal)
        {
            return candidate.loc;
        }
    }

    constexpr auto limit{static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max())};
    if (length > limit - base_offset_ - size_)
    {
        return std::unexpected{error_code::invalid_format};
    }
    const location position{size_, length};
    if (const auto res{append(chunks)}; !res)
    {
        return std::unexpected{res.error()};
    }

    size_ += length;
    candidates.push_back({type, position});
    return position;
}

std::uint64_t data_pool::encoder::size_bytes() const noexcept
{
    return size_;
}

result<void> data_pool::encoder::seek(std::uint64_t offset)
{
    is_cursor_at_end_ = false;
    if (!file_.seekg(static_cast<std::streamoff>(base_offset_ + offset)))
    {
        return std::unexpected{error_code::io_failure};
    }
    return {};
}

result<bool> data_pool::encoder::equals(std::span<const std::span<const std::byte>> chunks)
{
    std::array<std::byte, io_block_size_> buffer{};
    for (const auto chunk : chunks)
    {
        for (const auto block : chunk | std::views::chunk(buffer.size()))
        {
            if (!file_.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(block.size())))
            {
                return std::unexpected{error_code::io_failure};
            }
            if (!std::ranges::equal(std::span{buffer}.first(block.size()), block))
            {
                return false;
            }
        }
    }
    if (!file_)
    {
        return std::unexpected{error_code::io_failure};
    }
    return true;
}

result<void> data_pool::encoder::append(std::span<const std::span<const std::byte>> chunks)
{
    if (!is_cursor_at_end_)
    {
        if (!file_.seekp(static_cast<std::streamoff>(base_offset_ + size_)))
        {
            return std::unexpected{error_code::io_failure};
        }
        is_cursor_at_end_ = true;
    }
    for (const auto chunk : chunks)
    {
        for (const auto block : chunk | std::views::chunk(io_block_size_))
        {
            if (!file_.write(reinterpret_cast<const char*>(block.data()), static_cast<std::streamsize>(block.size())))
            {
                return std::unexpected{error_code::io_failure};
            }
        }
    }
    return {};
}
} // namespace nori::resource::codec
