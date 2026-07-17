#pragma once
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace nori::network
{
using packet_size_type = std::uint32_t;
using packet_type = std::uint16_t;

class out_packet
{
public:
    template <typename T>
    requires(std::is_enum_v<T> && std::is_same_v<std::underlying_type_t<T>, packet_type>)
    explicit out_packet(T type)
    {
        write(static_cast<packet_type>(type));
    }

    template <typename T>
    void write(T value)
    {
        if constexpr (std::is_convertible_v<T, std::string_view>)
        {
            const std::string_view view{value};
            write_size(view.size());
            const auto data{reinterpret_cast<const std::byte*>(view.data())};
            bytes_.insert(bytes_.end(), data, data + view.size());
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            bytes_.push_back(value ? std::byte{1} : std::byte{0});
        }
        else if constexpr (std::is_enum_v<T>)
        {
            write(static_cast<std::underlying_type_t<T>>(value));
        }
        else if constexpr (std::is_same_v<T, float>)
        {
            write(std::bit_cast<std::uint32_t>(value));
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            write(std::bit_cast<std::uint64_t>(value));
        }
        else if constexpr (std::is_integral_v<T>)
        {
            write_integral(value);
        }
        else
        {
            static_assert(always_false_v<T>, "unsupported packet writer type");
        }
    }

    [[nodiscard]]
    std::span<const std::byte> bytes() const;

private:
    void write_size(std::size_t size)
    {
        if (size > std::numeric_limits<packet_size_type>::max())
        {
            throw std::length_error{"packet field is too large"};
        }
        write(static_cast<packet_size_type>(size));
    }

    template <typename T>
    requires(std::is_integral_v<T> && !std::is_same_v<T, bool> && sizeof(T) <= sizeof(std::uint64_t))
    void write_integral(T value)
    {
        auto encoded{static_cast<std::make_unsigned_t<T>>(value)};
        if constexpr (std::endian::native == std::endian::little)
        {
            encoded = std::byteswap(encoded);
        }

        const auto data{reinterpret_cast<const std::byte*>(&encoded)};
        bytes_.insert(bytes_.end(), data, data + sizeof(encoded));
    }

private:
    template <typename>
    static constexpr bool always_false_v{false};

    std::vector<std::byte> bytes_;
};

class in_packet
{
public:
    in_packet(packet_type type, std::vector<std::byte> bytes);

    template <typename T>
    [[nodiscard]]
    T read()
    {
        if constexpr (std::is_same_v<T, std::string>)
        {
            const std::span<const std::byte> data{read_sized_field()};
            return {reinterpret_cast<const char*>(data.data()), data.size()};
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            const std::uint8_t value{read<std::uint8_t>()};
            if (value > 1)
            {
                throw std::runtime_error{"invalid packet bool value"};
            }
            return value != 0;
        }
        else if constexpr (std::is_enum_v<T>)
        {
            return static_cast<T>(read<std::underlying_type_t<T>>());
        }
        else if constexpr (std::is_same_v<T, float>)
        {
            return std::bit_cast<float>(read<std::uint32_t>());
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            return std::bit_cast<double>(read<std::uint64_t>());
        }
        else if constexpr (std::is_integral_v<T>)
        {
            return read_integral<T>();
        }
        else
        {
            static_assert(always_false_v<T>, "unsupported packet reader type");
        }
    }

    [[nodiscard]]
    packet_type type() const;

private:
    [[nodiscard]]
    std::size_t remaining() const;

    [[nodiscard]]
    std::span<const std::byte> read_sized_field()
    {
        const packet_size_type size{read<packet_size_type>()};
        if (size > remaining())
        {
            throw std::runtime_error{"truncated packet field"};
        }

        std::span<const std::byte> data{bytes_.data() + offset_, size};
        offset_ += size;
        return data;
    }

    template <typename T>
    requires(std::is_integral_v<T> && !std::is_same_v<T, bool> && sizeof(T) <= sizeof(std::uint64_t))
    [[nodiscard]]
    T read_integral()
    {
        if (remaining() < sizeof(T))
        {
            throw std::runtime_error{"truncated packet scalar"};
        }

        std::make_unsigned_t<T> decoded{0};
        std::memcpy(&decoded, bytes_.data() + offset_, sizeof(decoded));
        offset_ += sizeof(decoded);

        if constexpr (std::endian::native == std::endian::little)
        {
            decoded = std::byteswap(decoded);
        }

        if constexpr (std::is_signed_v<T>)
        {
            return std::bit_cast<T>(decoded);
        }
        else
        {
            return decoded;
        }
    }

private:
    template <typename>
    static constexpr bool always_false_v{false};

    packet_type type_;
    std::vector<std::byte> bytes_;
    std::size_t offset_;
};
} // namespace nori::network
