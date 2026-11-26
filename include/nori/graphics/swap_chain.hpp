#pragma once
#include <Windows.h>

#include <nori/core/handle.hpp>

namespace nori::graphics::detail
{
struct swap_chain_tag;

class swap_chain_deleter
{
public:
    void operator()(core::basic_handle<swap_chain_tag> handle) const noexcept;
};
} // namespace nori::graphics::detail

namespace nori::graphics
{
using swap_chain_handle = core::basic_handle<detail::swap_chain_tag>;
using unique_swap_chain_handle = core::unique_handle<swap_chain_handle, detail::swap_chain_deleter>;

struct swap_chain_desc
{
    HWND hwnd;
    bool vsync;
};
} // namespace nori::graphics
