#pragma once
#include <cstddef>
#include <functional>

#include <nori/core/handle.hpp>

namespace nori::resource::detail
{
struct hierarchy_tag;
using hierarchy_handle = core::basic_handle<hierarchy_tag>;

struct node_tag;
using node_handle = core::basic_handle<node_tag>;
} // namespace nori::resource::detail

namespace nori::resource
{
class handle
{
public:
    constexpr bool operator==(const handle&) const = default;

    explicit constexpr operator bool() const noexcept
    {
        return static_cast<bool>(hier) && static_cast<bool>(node);
    }

    detail::hierarchy_handle hier;
    detail::node_handle node;
};
} // namespace nori::resource

namespace std
{
template <>
struct hash<nori::resource::handle>
{
    std::size_t operator()(nori::resource::handle value) const noexcept
    {
        const std::size_t hierarchy_hash{std::hash<nori::resource::detail::hierarchy_handle>{}(value.hier)};
        const std::size_t node_hash{std::hash<nori::resource::detail::node_handle>{}(value.node)};
        return hierarchy_hash ^ (node_hash + 0x9E3779B9 + (hierarchy_hash << 6) + (hierarchy_hash >> 2));
    }
};
} // namespace std
