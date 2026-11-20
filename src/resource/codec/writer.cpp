#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <concepts>
#include <format>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "writer.hpp"

namespace
{
void encode_header(std::span<std::byte> bytes, const nori::resource::codec::file_header& header)
{
    nori::resource::codec::encode_scalar(bytes, header.magic);
    nori::resource::codec::encode_scalar(bytes, header.version);
    nori::resource::codec::encode_scalar(bytes, header.property_count);
    nori::resource::codec::encode_scalar(bytes, header.name_count);
    nori::resource::codec::encode_scalar(bytes, header.property_table_offset);
    nori::resource::codec::encode_scalar(bytes, header.name_table_offset);
    nori::resource::codec::encode_scalar(bytes, header.name_table_size);
    nori::resource::codec::encode_scalar(bytes, header.data_pool_offset);
    nori::resource::codec::encode_scalar(bytes, header.data_pool_size);
}

void encode_image_header(std::span<std::byte> bytes, const nori::resource::codec::image_header& header)
{
    nori::resource::codec::encode_scalar(bytes, std::to_underlying(header.format));
    nori::resource::codec::encode_scalar(bytes, header.width);
    nori::resource::codec::encode_scalar(bytes, header.height);
    nori::resource::codec::encode_scalar(bytes, header.mip_levels);
}

void encode_data_pool_location(std::span<std::byte> bytes, const nori::resource::codec::data_pool::location& position)
{
    nori::resource::codec::encode_scalar(bytes, position.offset);
    nori::resource::codec::encode_scalar(bytes, position.size);
}

std::filesystem::path make_temporary_path(const std::filesystem::path& destination)
{
    static std::atomic<std::uint64_t> counter{0};
    return destination.parent_path() / std::format("nori-writer-{}-{}.tmp", std::chrono::steady_clock::now().time_since_epoch().count(), counter.fetch_add(1, std::memory_order_relaxed));
}

nori::resource::result<void> replace_file(const std::filesystem::path& source, const std::filesystem::path& destination)
{
    std::error_code ec;
    const bool exists{std::filesystem::exists(destination, ec)};
    if (ec)
    {
        return std::unexpected{nori::resource::error_code::io_failure};
    }
    if (!exists)
    {
        std::filesystem::rename(source, destination, ec);
        if (ec)
        {
            return std::unexpected{nori::resource::error_code::io_failure};
        }
        return {};
    }

    const std::filesystem::file_status status{std::filesystem::status(destination, ec)};
    constexpr std::filesystem::perms writable{std::filesystem::perms::owner_write | std::filesystem::perms::group_write | std::filesystem::perms::others_write};
    if (ec || !std::filesystem::is_regular_file(status) || (status.permissions() & writable) == std::filesystem::perms::none)
    {
        return std::unexpected{nori::resource::error_code::io_failure};
    }

    const std::filesystem::path backup{make_temporary_path(destination)};
    if (std::filesystem::exists(backup, ec) || ec)
    {
        return std::unexpected{nori::resource::error_code::io_failure};
    }

    // 기존 파일 백업
    std::filesystem::rename(destination, backup, ec);
    if (ec)
    {
        return std::unexpected{nori::resource::error_code::io_failure};
    }

    // 새 파일로 교체
    std::filesystem::rename(source, destination, ec);
    if (ec)
    {
        std::error_code restore_ec;
        std::filesystem::rename(backup, destination, restore_ec);
        return std::unexpected{nori::resource::error_code::io_failure};
    }
    std::filesystem::remove(backup, ec);
    return {};
}
} // namespace

namespace nori::resource::codec
{
writer::writer(std::filesystem::path file_path) :
    file_path_{std::move(file_path)},
    temporary_path_{make_temporary_path(file_path_)},
    owns_temporary_{false}
{
}

writer::~writer()
{
    if (owns_temporary_)
    {
        std::error_code ec;
        std::filesystem::remove(temporary_path_, ec);
    }
}

result<void> writer::append(std::string_view name, const property::value_type& value, std::uint32_t depth)
{
    const auto name_id{names_.intern(name)};
    if (!name_id)
    {
        return std::unexpected{name_id.error()};
    }

    property_table::entry entry{
        .name_id = *name_id,
        .type = nori::resource::type_of(value)
    };
    std::visit([this, &entry, &value]<typename T>(const T& data)
    {
        if constexpr (std::is_same_v<T, bool>)
        {
            encode_scalar(entry.value, static_cast<std::uint8_t>(data));
        }
        else if constexpr (std::is_same_v<T, std::int32_t>)
        {
            encode_scalar(entry.value, std::bit_cast<std::uint32_t>(data));
        }
        else if constexpr (std::is_same_v<T, std::int64_t>)
        {
            encode_scalar(entry.value, std::bit_cast<std::uint64_t>(data));
        }
        else if constexpr (std::is_same_v<T, float>)
        {
            encode_scalar(entry.value, canonical_float_bits(std::bit_cast<std::uint32_t>(data)));
        }
        else if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, image> || std::is_same_v<T, binary>)
        {
            pending_payloads_.push_back({properties_.size(), &value});
        }
        else
        {
            static_assert(std::is_same_v<T, empty_t>, "unsupported resource value type");
        }
    }, value);

    return properties_.append(entry, depth);
}

result<void> writer::finish()
{
    const std::uint64_t names_offset{file_header_size + properties_.size_bytes()};
    const std::uint64_t names_size{names_.size_bytes()};
    const std::uint64_t data_pool_offset{names_offset + names_size};
    constexpr auto limit{static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max())};
    if (data_pool_offset > limit)
    {
        return std::unexpected{error_code::invalid_format};
    }

    std::fstream output{temporary_path_, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc | std::ios::noreplace};
    if (!output)
    {
        return std::unexpected{error_code::io_failure};
    }

    owns_temporary_ = true;
    data_pool::encoder pool{output, data_pool_offset};
    for (const auto& pending : pending_payloads_)
    {
        if (const auto res{encode_data_pool_value(pool, pending)}; !res)
        {
            return res;
        }
    }

    const file_header header{
        .magic = file_magic,
        .version = file_version,
        .property_count = properties_.size(),
        .name_count = names_.size(),
        .property_table_offset = file_header_size,
        .name_table_offset = names_offset,
        .name_table_size = names_size,
        .data_pool_offset = data_pool_offset,
        .data_pool_size = pool.size_bytes(),
    };
    output.seekp(0);
    std::array<std::byte, file_header_size> header_bytes{};
    encode_header(header_bytes, header);
    output.write(reinterpret_cast<const char*>(header_bytes.data()), static_cast<std::streamsize>(header_bytes.size()));

    if (const auto res{properties_.encode(output)}; !res)
    {
        return res;
    }

    if (const auto res{names_.encode(output)}; !res)
    {
        return res;
    }

    if (!output.flush())
    {
        return std::unexpected{error_code::io_failure};
    }

    output.close();
    if (output.fail())
    {
        return std::unexpected{error_code::io_failure};
    }

    if (const auto res{replace_file(temporary_path_, file_path_)}; !res)
    {
        return res;
    }
    owns_temporary_ = false;
    return {};
}

result<void> writer::encode_data_pool_value(data_pool::encoder& pool, const payload& pending)
{
    auto& record{properties_[pending.index]};
    std::array<std::byte, image_header_size> header_bytes{};
    std::array<std::span<const std::byte>, 2> chunks{};
    std::size_t count{1};

    if (const auto text{std::get_if<std::string>(pending.value)})
    {
        chunks[0] = std::as_bytes(std::span{*text});
    }
    else if (const auto picture{std::get_if<image>(pending.value)})
    {
        if (!is_valid_image(picture->format, picture->width, picture->height, picture->mip_levels, picture->data.size()))
        {
            return std::unexpected{error_code::invalid_format};
        }
        encode_image_header(header_bytes, image_header{
            .format = picture->format,
            .width = picture->width,
            .height = picture->height,
            .mip_levels = picture->mip_levels,
        });
        chunks[0] = header_bytes;
        chunks[1] = picture->data;
        count = 2;
    }
    else
    {
        chunks[0] = std::get<binary>(*pending.value).data;
    }

    const auto res{pool.intern(record.type, std::span{chunks}.first(count))};
    if (!res)
    {
        return std::unexpected{res.error()};
    }
    encode_data_pool_location(record.value, *res);
    return {};
}
} // namespace nori::resource::codec
