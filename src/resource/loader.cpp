#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <fstream>
#include <utility>

#include "codec/data_pool.hpp"
#include "codec/format.hpp"
#include "loader.hpp"

namespace
{
nori::resource::result<void> read_bytes(std::ifstream& file, std::span<std::byte> destination)
{
    while (!destination.empty())
    {
        const std::size_t count{std::min(destination.size(), 1024uz * 1024)};
        if (!file.read(reinterpret_cast<char*>(destination.data()), static_cast<std::streamsize>(count)))
        {
            return std::unexpected{nori::resource::error_code::io_failure};
        }
        destination = destination.subspan(count);
    }
    if (!file)
    {
        return std::unexpected{nori::resource::error_code::io_failure};
    }
    return {};
}

nori::resource::result<void> read_bytes_at(std::ifstream& file, std::uint64_t offset, std::span<std::byte> destination)
{
    file.seekg(static_cast<std::streamoff>(offset));
    return read_bytes(file, destination);
}

nori::resource::codec::file_header decode_header(std::span<const std::byte> bytes)
{
    return {
        .magic = nori::resource::codec::decode_scalar<std::uint32_t>(bytes),
        .version = nori::resource::codec::decode_scalar<std::uint32_t>(bytes),
        .property_count = nori::resource::codec::decode_scalar<std::uint32_t>(bytes),
        .name_count = nori::resource::codec::decode_scalar<std::uint32_t>(bytes),
        .property_table_offset = nori::resource::codec::decode_scalar<std::uint64_t>(bytes),
        .name_table_offset = nori::resource::codec::decode_scalar<std::uint64_t>(bytes),
        .name_table_size = nori::resource::codec::decode_scalar<std::uint64_t>(bytes),
        .data_pool_offset = nori::resource::codec::decode_scalar<std::uint64_t>(bytes),
        .data_pool_size = nori::resource::codec::decode_scalar<std::uint64_t>(bytes),
    };
}

nori::resource::codec::image_header decode_image_header(std::span<const std::byte> bytes)
{
    return {
        .format = nori::resource::image_format{nori::resource::codec::decode_scalar<std::uint8_t>(bytes)},
        .width = nori::resource::codec::decode_scalar<std::uint32_t>(bytes),
        .height = nori::resource::codec::decode_scalar<std::uint32_t>(bytes),
        .mip_levels = nori::resource::codec::decode_scalar<std::uint32_t>(bytes),
    };
}

nori::resource::codec::data_pool::location decode_data_pool_location(std::span<const std::byte> bytes)
{
    return {
        .offset = nori::resource::codec::decode_scalar<std::uint64_t>(bytes),
        .size = nori::resource::codec::decode_scalar<std::uint64_t>(bytes),
    };
}

nori::resource::property::value_type decode_inline_value(nori::resource::property_type tag, std::span<const std::byte> bytes)
{
    switch (tag)
    {
    case nori::resource::property_type::empty:
    {
        return nori::resource::empty_t{};
    }
    case nori::resource::property_type::boolean:
    {
        const auto value{nori::resource::codec::decode_scalar<std::uint8_t>(bytes)};
        return value != 0;
    }
    case nori::resource::property_type::int32:
    {
        const auto bits{nori::resource::codec::decode_scalar<std::uint32_t>(bytes)};
        return std::bit_cast<std::int32_t>(bits);
    }
    case nori::resource::property_type::int64:
    {
        const auto bits{nori::resource::codec::decode_scalar<std::uint64_t>(bytes)};
        return std::bit_cast<std::int64_t>(bits);
    }
    case nori::resource::property_type::float32:
    {
        const auto bits{nori::resource::codec::decode_scalar<std::uint32_t>(bytes)};
        return std::bit_cast<float>(bits);
    }
    default:
        std::unreachable();
    }
}

nori::resource::result<nori::resource::property::value_type> read_pool_value(std::ifstream& file, nori::resource::property_type tag, std::uint64_t offset, std::uint64_t size)
{
    switch (tag)
    {
    case nori::resource::property_type::image:
    {
        constexpr auto header_size{nori::resource::codec::image_header_size};
        if (size < header_size || !std::in_range<std::size_t>(size - header_size))
        {
            return std::unexpected{nori::resource::error_code::invalid_format};
        }

        std::array<std::byte, header_size> header_bytes{};
        if (const auto res{read_bytes_at(file, offset, header_bytes)}; !res)
        {
            return std::unexpected{res.error()};
        }
        const auto header{decode_image_header(header_bytes)};
        std::vector<std::byte> pixels(static_cast<std::size_t>(size - header_size));
        if (const auto res{read_bytes(file, pixels)}; !res)
        {
            return std::unexpected{res.error()};
        }
        return nori::resource::image{
            .format = header.format,
            .width = header.width,
            .height = header.height,
            .mip_levels = header.mip_levels,
            .data = std::move(pixels),
        };
    }
    case nori::resource::property_type::string:
    case nori::resource::property_type::binary:
    {
        std::vector<std::byte> bytes(size);
        if (const auto res{read_bytes_at(file, offset, bytes)}; !res)
        {
            return std::unexpected{res.error()};
        }
        if (tag == nori::resource::property_type::string)
        {
            return std::string{reinterpret_cast<const char*>(bytes.data()), bytes.size()};
        }
        return nori::resource::binary{std::move(bytes)};
    }
    default:
        std::unreachable();
    }
}
} // namespace

namespace nori::resource
{
loader::loader(std::filesystem::path file_path) :
    file_path_{std::move(file_path)}
{
}

result<loader::reader> loader::open()
{
    std::lock_guard lock{mutex_};
    if (cache_)
    {
        return reader{this};
    }

    auto res{read_metadata(file_path_)};
    if (!res)
    {
        return std::unexpected{res.error()};
    }

    cache loaded{
        .info = std::move(*res),
    };
    loaded.relations.reserve(loaded.info.properties.size());
    std::vector<index_type> parents;
    for (index_type index{0}; index < loaded.info.properties.size(); ++index)
    {
        while (!parents.empty() && index > loaded.info.properties[parents.back()].subtree_end)
        {
            parents.pop_back();
        }
        loaded.relations.push_back({.parent = parents.empty() ? std::nullopt : std::optional{parents.back()}, .child = index});
        if (loaded.info.properties[index].subtree_end > index)
        {
            parents.push_back(index);
        }
    }
    const child_less less{&loaded.info.properties, &loaded.info.names};
    std::ranges::sort(loaded.relations, less);
    cache_.emplace(std::move(loaded));
    return reader{this};
}

loader::child_less::child_less(const codec::property_table* properties, const codec::name_table* names) :
    properties_{properties},
    names_{names}
{
}

bool loader::child_less::operator()(const relation& left, const relation& right) const
{
    return (*this)(left, child_key{right.parent, (*names_)[(*properties_)[right.child].name_id]});
}

bool loader::child_less::operator()(const relation& left, const child_key& right) const
{
    return child_key{left.parent, (*names_)[(*properties_)[left.child].name_id]} < right;
}

bool loader::child_less::operator()(const child_key& left, const relation& right) const
{
    return left < child_key{right.parent, (*names_)[(*properties_)[right.child].name_id]};
}

result<loader::metadata> loader::read_metadata(const std::filesystem::path& file_path)
{
    std::error_code ec;
    if (!std::filesystem::exists(file_path, ec))
    {
        return std::unexpected{ec ? error_code::io_failure : error_code::not_found};
    }
    if (std::filesystem::is_directory(file_path, ec) || ec)
    {
        return std::unexpected{error_code::io_failure};
    }

    std::ifstream file{file_path, std::ios::binary};
    if (!file)
    {
        return std::unexpected{error_code::io_failure};
    }

    std::array<std::byte, codec::file_header_size> header_bytes{};
    if (const auto res{read_bytes_at(file, 0, header_bytes)}; !res)
    {
        return std::unexpected{res.error()};
    }

    const codec::file_header header{decode_header(header_bytes)};
    std::vector<std::byte> metadata_bytes(static_cast<std::size_t>(header.data_pool_offset - header.property_table_offset));
    if (const auto res{read_bytes_at(file, header.property_table_offset, metadata_bytes)}; !res)
    {
        return std::unexpected{res.error()};
    }
    const std::span<const std::byte> metadata_view{metadata_bytes};
    const auto property_table_size{static_cast<std::size_t>(header.name_table_offset - header.property_table_offset)};
    return metadata{
        .data_pool_offset = header.data_pool_offset,
        .names = codec::name_table::decode(metadata_view.subspan(property_table_size)),
        .properties = codec::property_table::decode(metadata_view.first(property_table_size)),
    };
}

loader::reader::cursor loader::reader::root()
{
    return cursor{this, std::nullopt};
}

loader::reader::reader(const loader* source) :
    source_{source}
{
}

result<property::value_type> loader::reader::value(index_type index)
{
    const codec::property_table::entry& entry{source_->cache_->info.properties[index]};
    if (!codec::is_data_pool_type(entry.type))
    {
        return decode_inline_value(entry.type, entry.value);
    }

    // 파일 안열려있으면 열기
    if (!file_.is_open())
    {
        file_.open(source_->file_path_, std::ios::binary);
        if (!file_)
        {
            return std::unexpected{error_code::io_failure};
        }
    }

    const codec::data_pool::location location{decode_data_pool_location(entry.value)};
    const std::uint64_t offset{source_->cache_->info.data_pool_offset + location.offset};
    auto res{read_pool_value(file_, entry.type, offset, location.size)};
    if (!res)
    {
        file_.close();
    }
    return res;
}

std::optional<loader::index_type> loader::reader::find_child(std::optional<index_type> parent, std::string_view child_name) const
{
    const child_key key{parent, child_name};
    const child_less less{&source_->cache_->info.properties, &source_->cache_->info.names};
    const auto it{std::lower_bound(source_->cache_->relations.begin(), source_->cache_->relations.end(), key, less)};
    if (it == source_->cache_->relations.end() || less(key, *it))
    {
        return std::nullopt;
    }
    return it->child;
}

loader::index_type loader::reader::subtree_end(index_type index) const
{
    return source_->cache_->info.properties[index].subtree_end;
}

std::string_view loader::reader::cursor::name() const
{
    const metadata& info{reader_->source_->cache_->info};
    return index_ ? info.names[info.properties[*index_].name_id] : std::string_view{};
}

property_type loader::reader::cursor::type() const
{
    assert(index_);
    return reader_->source_->cache_->info.properties[*index_].type;
}

result<property::value_type> loader::reader::cursor::value() const
{
    assert(index_);
    return reader_->value(*index_);
}

bool loader::reader::cursor::is_root() const
{
    return !index_;
}

std::optional<loader::reader::cursor> loader::reader::cursor::find_child(std::string_view name) const
{
    if (const auto child{reader_->find_child(index_, name)})
    {
        return cursor{reader_, *child};
    }
    return std::nullopt;
}

std::vector<loader::reader::cursor> loader::reader::cursor::children() const
{
    const index_type end{index_ ? reader_->subtree_end(*index_) + 1 : static_cast<index_type>(reader_->source_->cache_->info.properties.size())};
    std::vector<cursor> cursors;
    for (index_type index{index_ ? *index_ + 1 : 0}; index < end; index = reader_->subtree_end(index) + 1)
    {
        cursors.push_back(cursor{reader_, index});
    }
    return cursors;
}

loader::reader::cursor::cursor(reader* source, std::optional<index_type> index) :
    reader_{source},
    index_{index}
{
}
} // namespace nori::resource
