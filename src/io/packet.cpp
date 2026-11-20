#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <nori/io/packet.hpp>

namespace nori::io
{
bool opacket::write(std::string_view value)
{
    return write(std::as_bytes(std::span{value}));
}

bool opacket::write(std::span<const std::byte> value)
{
    if (!write_size(value.size()))
    {
        return false;
    }
    bytes_.append_range(value);
    return true;
}

std::span<const std::byte> opacket::bytes() const noexcept
{
    return bytes_;
}

bool opacket::write_size(std::size_t size)
{
    if (size > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }
    write(static_cast<std::uint32_t>(size));
    return true;
}

ipacket::ipacket(std::vector<std::byte> bytes) :
    bytes_{std::move(bytes)},
    offset_{0}
{
}

std::optional<std::vector<std::byte>> ipacket::read_bytes()
{
    return read_size().transform([this](std::size_t size)
    {
        const std::span<const std::byte> data{bytes_.data() + offset_, size};
        std::vector<std::byte> value{std::from_range, data};
        offset_ += size;
        return value;
    });
}

std::size_t ipacket::remaining() const noexcept
{
    return bytes_.size() - offset_;
}

std::optional<std::size_t> ipacket::read_size()
{
    return read<std::uint32_t>().and_then([this](std::uint32_t size) -> std::optional<std::size_t>
    {
        if (size > remaining())
        {
            return std::nullopt;
        }
        return size;
    });
}
} // namespace nori::io
