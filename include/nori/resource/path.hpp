#pragma once
#include <cstddef>
#include <filesystem>
#include <functional>
#include <iterator>
#include <string>
#include <string_view>

namespace nori::resource
{
[[nodiscard]] bool is_valid_property_name(std::string_view name) noexcept;

class path
{
public:
    path() = default;
    path(const char* value);
    path(std::string_view value);
    path(std::string value);
    explicit path(const std::filesystem::path& value);

    [[nodiscard]] path operator/(const path& rhs) const;
    [[nodiscard]] bool operator==(const path&) const = default;
    [[nodiscard]] auto operator<=>(const path&) const = default;

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] bool has_file_path() const noexcept;
    [[nodiscard]] bool has_subpath() const noexcept;

    [[nodiscard]] std::string string() const;
    [[nodiscard]] std::string_view string_view() const noexcept;
    [[nodiscard]] std::filesystem::path file_path() const;
    [[nodiscard]] std::string_view subpath() const noexcept;

    static constexpr std::string_view extension{".nori"};
    static constexpr char separator{'/'};
    static constexpr std::string_view subpath_delimiter{"::"};

private:
    std::string value_;
};

class subpath_iterator
{
public:
    using value_type = std::string_view;
    using difference_type = std::ptrdiff_t;
    using reference = value_type;
    using pointer = void;
    using iterator_category = std::input_iterator_tag;
    using iterator_concept = std::input_iterator_tag;

    subpath_iterator() noexcept;
    explicit subpath_iterator(const path& value);

    [[nodiscard]] value_type operator*() const noexcept;
    subpath_iterator& operator++() noexcept;
    subpath_iterator operator++(int);
    [[nodiscard]] bool operator==(const subpath_iterator& rhs) const noexcept;

private:
    std::string value_;
    std::size_t offset_;
};

[[nodiscard]] subpath_iterator begin(subpath_iterator it) noexcept;
[[nodiscard]] subpath_iterator end(const subpath_iterator& it) noexcept;
} // namespace nori::resource

template <>
struct std::hash<nori::resource::path>
{
    [[nodiscard]] std::size_t operator()(const nori::resource::path& value) const noexcept
    {
        return std::hash<std::string_view>{}(value.string_view());
    }
};
