#pragma once
#include <cstddef>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <nori/resource/handle.hpp>

namespace nori::resource
{
class iterator
{
public:
    using iterator_concept = std::contiguous_iterator_tag;
    using iterator_category = std::random_access_iterator_tag;
    using value_type = std::pair<std::string, handle>;
    using difference_type = std::ptrdiff_t;

public:
    iterator();
    iterator(handle target);
    iterator(std::string_view path);

    iterator& operator++();
    iterator operator++(int);
    value_type operator*() const;
    bool operator==(const iterator& other) const;

    iterator begin() const;
    iterator end() const;

private:
    std::vector<handle> children_;
    std::size_t index_;
};
} // namespace nori::resource
