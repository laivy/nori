#pragma once
#include <nori/core/handle.hpp>
#include <nori/io/context.hpp>

namespace nori::io::detail
{
struct strand_tag;

class strand_deleter
{
public:
    void operator()(core::basic_handle<strand_tag> handle) const noexcept;
};
} // namespace nori::io::detail

namespace nori::io
{
using strand_handle = core::basic_handle<detail::strand_tag>;
using unique_strand_handle = core::unique_handle<strand_handle, detail::strand_deleter>;

[[nodiscard]] unique_strand_handle create_strand(context_handle ctx);
void destroy(strand_handle st);
} // namespace nori::io
