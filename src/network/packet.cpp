#include <utility>
#include "packet.hpp"

namespace nori::network
{
std::span<const std::byte> out_packet::bytes() const
{
    return bytes_;
}

in_packet::in_packet(packet_type type, std::vector<std::byte> bytes) :
    type_{type},
    bytes_{std::move(bytes)},
    offset_{0}
{
}

std::size_t in_packet::remaining() const
{
    return bytes_.size() - offset_;
}

packet_type in_packet::type() const
{
    return type_;
}
} // namespace nori::network
