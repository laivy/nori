#pragma once
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <nori/core/endian.hpp>

namespace nori::io
{
class opacket
{
public:
    template <typename T>
    requires(std::integral<T> || std::same_as<T, float> || std::same_as<T, double> || std::is_enum_v<T>)
    void write(T value);
    bool write(std::string_view value);
    bool write(std::span<const std::byte> value);
    [[nodiscard]] std::span<const std::byte> bytes() const noexcept;

private:
    bool write_size(std::size_t size);

    std::vector<std::byte> bytes_;
};

class ipacket
{
public:
    explicit ipacket(std::vector<std::byte> bytes);

    ipacket(const ipacket&) = delete;
    ipacket(ipacket&&) noexcept = default;
    ipacket& operator=(const ipacket&) = delete;
    ipacket& operator=(ipacket&&) noexcept = default;

    template <typename T>
    requires(std::same_as<T, std::string> || std::integral<T> || std::same_as<T, float> || std::same_as<T, double> || std::is_enum_v<T>)
    [[nodiscard]] std::optional<T> read();
    [[nodiscard]] std::optional<std::vector<std::byte>> read_bytes();
    [[nodiscard]] std::size_t remaining() const noexcept;

private:
    [[nodiscard]] std::optional<std::size_t> read_size();

    std::vector<std::byte> bytes_;
    std::size_t offset_;
};

template <typename T>
requires(std::integral<T> || std::same_as<T, float> || std::same_as<T, double> || std::is_enum_v<T>)
void opacket::write(T value)
{
    if constexpr (std::same_as<T, bool>)
    {
        bytes_.push_back(value ? std::byte{1} : std::byte{0});
    }
    else if constexpr (std::is_enum_v<T>)
    {
        write(std::to_underlying(value));
    }
    else
    {
        const T encoded{core::to_endian(value, std::endian::big)};
        bytes_.append_range(std::as_bytes(std::span{&encoded, 1}));
    }
}

template <typename T>
requires(std::same_as<T, std::string> || std::integral<T> || std::same_as<T, float> || std::same_as<T, double> || std::is_enum_v<T>)
std::optional<T> ipacket::read()
{
    if constexpr (std::same_as<T, std::string>)
    {
        return read_size().transform([this](std::size_t size)
        {
            std::string value{reinterpret_cast<const char*>(bytes_.data() + offset_), size};
            offset_ += size;
            return value;
        });
    }
    else if constexpr (std::same_as<T, bool>)
    {
        return read<std::uint8_t>().and_then([](std::uint8_t value) -> std::optional<bool>
        {
            if (value > 1)
            {
                return std::nullopt;
            }
            return value != 0;
        });
    }
    else if constexpr (std::is_enum_v<T>)
    {
        return read<std::underlying_type_t<T>>().transform([](std::underlying_type_t<T> value)
        {
            return static_cast<T>(value);
        });
    }
    else
    {
        if (remaining() < sizeof(T))
        {
            return std::nullopt;
        }
        T decoded{};
        std::memcpy(&decoded, bytes_.data() + offset_, sizeof(T));
        offset_ += sizeof(T);
        return core::to_endian(decoded, std::endian::big);
    }
}
} // namespace nori::io
