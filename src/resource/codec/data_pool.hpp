#pragma once
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <span>
#include <unordered_map>
#include <vector>

#include <nori/resource/error_code.hpp>
#include <nori/resource/value.hpp>

namespace nori::resource::codec
{
class data_pool
{
public:
    class encoder;

    struct location
    {
        std::uint64_t offset;
        std::uint64_t size;
    };
};

class data_pool::encoder
{
public:
    encoder(std::fstream& file, std::uint64_t base_offset);

    encoder(const encoder&) = delete;
    encoder(encoder&&) = delete;
    encoder& operator=(const encoder&) = delete;
    encoder& operator=(encoder&&) = delete;

    result<location> intern(property_type type, std::span<const std::span<const std::byte>> chunks);

    [[nodiscard]] std::uint64_t size_bytes() const noexcept;

private:
    struct entry
    {
        property_type type;
        location loc;
    };

    result<void> seek(std::uint64_t offset);
    result<bool> equals(std::span<const std::span<const std::byte>> chunks);
    result<void> append(std::span<const std::span<const std::byte>> chunks);

    static constexpr std::size_t io_block_size_{64uz * 1024uz};

    std::fstream& file_;
    std::uint64_t base_offset_;
    std::uint64_t size_;
    bool is_cursor_at_end_;
    std::unordered_map<std::size_t, std::vector<entry>> entries_;
};
} // namespace nori::resource::codec
