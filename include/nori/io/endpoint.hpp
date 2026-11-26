#pragma once
#include <cstdint>
#include <string>

namespace nori::io
{
struct endpoint
{
    std::string address;
    std::uint16_t port;
};
} // namespace nori::io
