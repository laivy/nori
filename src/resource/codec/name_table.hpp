#pragma once
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <nori/resource/error_code.hpp>

namespace nori::resource::codec
{
class name_table
{
public:
    class encoder;

    name_table() = default;

    name_table(const name_table&) = delete;
    name_table(name_table&& other) = default;
    name_table& operator=(const name_table&) = delete;
    name_table& operator=(name_table&& other) = default;

    static name_table decode(std::span<const std::byte> bytes);

    [[nodiscard]] std::string_view operator[](std::uint32_t id) const;
    [[nodiscard]] std::uint32_t size() const noexcept;

    static constexpr std::size_t length_size{sizeof(std::uint16_t)}; // 이름 길이 필드 크기

private:
    std::vector<std::string> names_;
};

class name_table::encoder
{
public:
    encoder() = default;

    encoder(const encoder&) = delete;
    encoder(encoder&& other) = default;
    encoder& operator=(const encoder&) = delete;
    encoder& operator=(encoder&& other) = default;

    result<std::uint32_t> intern(std::string_view name);
    result<void> encode(std::ostream& output) const;

    [[nodiscard]] std::uint32_t size() const noexcept;
    [[nodiscard]] std::uint64_t size_bytes() const noexcept;

private:
    std::unordered_map<std::string, std::uint32_t> names_;
    std::vector<const std::string*> names_by_id_;
};
} // namespace nori::resource::codec
