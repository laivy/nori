#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <span>
#include <vector>

#include <nori/resource/error_code.hpp>
#include <nori/resource/value.hpp>

namespace nori::resource::codec
{
class property_table
{
public:
    class encoder;

    struct entry
    {
        std::uint32_t name_id;
        std::uint32_t subtree_end;
        property_type type;
        std::array<std::byte, 3> reserved;
        std::array<std::byte, 16> value;
    };

    static property_table decode(std::span<const std::byte> bytes);

    [[nodiscard]] const entry& operator[](std::uint32_t index) const;
    [[nodiscard]] std::uint32_t size() const noexcept;

    static constexpr std::size_t entry_size{28};

private:
    std::vector<entry> entries_;
};

class property_table::encoder
{
public:
    [[nodiscard]] entry& operator[](std::uint32_t index) noexcept;

    result<void> append(entry item, std::uint32_t depth);
    result<void> encode(std::ostream& output);

    [[nodiscard]] std::uint32_t size() const noexcept;
    [[nodiscard]] std::uint64_t size_bytes() const noexcept;

private:
    std::vector<entry> entries_;
    std::vector<std::uint32_t> ancestors_;
};
} // namespace nori::resource::codec
