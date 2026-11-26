#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <nori/resource/error_code.hpp>

#include "codec/name_table.hpp"
#include "codec/property_table.hpp"
#include "property.hpp"

namespace nori::resource
{
class loader
{
public:
    class reader;

    explicit loader(std::filesystem::path file_path);

    [[nodiscard]] result<reader> open();

private:
    using index_type = std::uint32_t;

    struct metadata
    {
        std::uint64_t data_pool_offset;
        codec::name_table names;
        codec::property_table properties;
    };

    struct relation
    {
        std::optional<index_type> parent;
        index_type child;
    };

    struct cache
    {
        metadata info;
        std::vector<relation> relations;
    };

    using child_key = std::pair<std::optional<index_type>, std::string_view>;

    class child_less
    {
    public:
        child_less(const codec::property_table* properties, const codec::name_table* names);

        bool operator()(const relation& left, const relation& right) const;
        bool operator()(const relation& left, const child_key& right) const;
        bool operator()(const child_key& left, const relation& right) const;

    private:
        const codec::property_table* properties_;
        const codec::name_table* names_;
    };

    [[nodiscard]] static result<metadata> read_metadata(const std::filesystem::path& file_path);

    std::filesystem::path file_path_;
    std::mutex mutex_;
    std::optional<cache> cache_;
};

class loader::reader
{
    friend class loader;

public:
    class cursor;

    reader(const reader&) = delete;
    reader(reader&& other) = default;
    reader& operator=(const reader&) = delete;
    reader& operator=(reader&& other) = default;

    [[nodiscard]] cursor root();

private:
    explicit reader(const loader* source);

    [[nodiscard]] result<property::value_type> value(index_type index);
    [[nodiscard]] std::optional<index_type> find_child(std::optional<index_type> parent, std::string_view child_name) const;
    [[nodiscard]] index_type subtree_end(index_type index) const;

    std::ifstream file_;
    const loader* source_;
};

class loader::reader::cursor
{
    friend class reader;

public:
    [[nodiscard]] std::string_view name() const;
    [[nodiscard]] property_type type() const;
    [[nodiscard]] result<property::value_type> value() const;

    [[nodiscard]] bool is_root() const;
    [[nodiscard]] std::optional<cursor> find_child(std::string_view name) const;
    [[nodiscard]] std::vector<cursor> children() const;

private:
    cursor(reader* source, std::optional<index_type> index);

    reader* reader_;
    std::optional<index_type> index_;
};
} // namespace nori::resource
