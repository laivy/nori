#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "resource_test_helpers.hpp"

namespace
{
using fixture_value = std::variant<nori::resource::empty_t, bool, std::int32_t, std::int64_t, float, std::string, nori::resource::image, nori::resource::binary>;

struct fixture_property
{
    std::uint32_t depth;
    std::string name;
    fixture_value value;
};

inline nori::resource::result<void> save_fixture(const std::filesystem::path& file, std::span<const fixture_property> properties)
{
    if (const auto existing{nori::resource::try_get(nori::resource::path{file})})
    {
        nori::resource::remove(*existing);
    }
    const nori::resource::handle root{nori::resource::create_root()};
    std::vector<nori::resource::handle> parents{root};
    for (const auto& property : properties)
    {
        parents.resize(property.depth + 1);
        const nori::resource::handle child{std::visit([&property, &parents](const auto& value)
        {
            return nori::resource::create(parents.back(), property.name, value);
        }, property.value)};
        if (!child)
        {
            nori::resource::remove(root);
            return std::unexpected{nori::resource::error_code::name_conflict};
        }
        parents.push_back(child);
    }
    const auto res{nori::resource::save_as(root, file)};
    nori::resource::remove(root);
    return res;
}

template <nori::resource::gettable T>
nori::resource::result<fixture_value> read_fixture_as(nori::resource::handle target)
{
    if (auto res{nori::resource::try_get<T>(target)})
    {
        return fixture_value{std::move(*res)};
    }
    else
    {
        return std::unexpected{res.error()};
    }
}

inline nori::resource::result<fixture_value> read_fixture_value(nori::resource::handle target)
{
    switch (nori::resource::type_of(target))
    {
    case nori::resource::property_type::empty:
    {
        if (const auto res{nori::resource::load(target)}; !res)
        {
            return std::unexpected{res.error()};
        }
        return nori::resource::empty_t{};
    }
    case nori::resource::property_type::boolean:
    {
        return read_fixture_as<bool>(target);
    }
    case nori::resource::property_type::int32:
    {
        return read_fixture_as<std::int32_t>(target);
    }
    case nori::resource::property_type::int64:
    {
        return read_fixture_as<std::int64_t>(target);
    }
    case nori::resource::property_type::float32:
    {
        return read_fixture_as<float>(target);
    }
    case nori::resource::property_type::string:
    {
        return read_fixture_as<std::string>(target);
    }
    case nori::resource::property_type::image:
    {
        return read_fixture_as<nori::resource::image>(target);
    }
    case nori::resource::property_type::binary:
    {
        return read_fixture_as<nori::resource::binary>(target);
    }
    default:
        return std::unexpected{nori::resource::error_code::invalid_format};
    }
}

template <typename T>
void write_scalar(std::ostream& output, T value)
{
    using unsigned_type = std::make_unsigned_t<T>;
    const auto bits{static_cast<unsigned_type>(value)};
    for (std::size_t index{0}; index < sizeof(T); ++index)
    {
        output.put(static_cast<char>((bits >> (index * 8)) & 0xFFu));
    }
}

struct fixture_record
{
    std::string_view name;
    std::uint32_t subtree_end;
    std::int32_t value;
};

// production codec과 독립적인 v1 fixture
inline void write_fixture_file(const std::filesystem::path& file, std::span<const fixture_record> records, bool corrupt_last_value = false)
{
    std::uint64_t name_table_size{0};
    std::vector<std::string_view> names;
    std::vector<std::uint32_t> name_ids;
    for (const fixture_record& record : records)
    {
        const auto it{std::ranges::find(names, record.name)};
        name_ids.push_back(static_cast<std::uint32_t>(it - names.begin()));
        if (it == names.end())
        {
            names.push_back(record.name);
            name_table_size += 2 + record.name.size();
        }
    }
    std::ofstream output{file, std::ios::binary};
    output.write("nori", 4);
    write_scalar(output, std::uint32_t{1});
    write_scalar(output, static_cast<std::uint32_t>(records.size()));
    write_scalar(output, static_cast<std::uint32_t>(names.size()));
    write_scalar(output, std::uint64_t{56});
    write_scalar(output, std::uint64_t{56 + 28 * records.size()});
    write_scalar(output, name_table_size);
    write_scalar(output, std::uint64_t{56 + 28 * records.size() + name_table_size});
    write_scalar(output, std::uint64_t{0});
    for (std::size_t index{0}; index < records.size(); ++index)
    {
        write_scalar(output, name_ids[index]);
        write_scalar(output, records[index].subtree_end);
        write_scalar(output, std::uint32_t{2});
        write_scalar(output, records[index].value);
        write_scalar(output, std::uint32_t{0});
        write_scalar(output, std::uint64_t{corrupt_last_value && index + 1 == records.size() ? 1u : 0u});
    }
    for (const auto name : names)
    {
        write_scalar(output, static_cast<std::uint16_t>(name.size()));
        output.write(name.data(), static_cast<std::streamsize>(name.size()));
    }
}

inline std::vector<char> file_bytes(const std::filesystem::path& file)
{
    std::ifstream input{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}
} // namespace
