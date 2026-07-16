#include <algorithm>
#include <cassert>
#include "viewport.hpp"

namespace nori::graphics
{
viewport::viewport(std::uint32_t width, std::uint32_t height) :
    viewport_{},
    scissor_rect_{},
    width_{},
    height_{}
{
    resize(width, height);
}

void viewport::resize(std::uint32_t width, std::uint32_t height)
{
    width_ = std::max<std::uint32_t>(1, width);
    height_ = std::max<std::uint32_t>(1, height);
    viewport_ = CD3DX12_VIEWPORT{0.0f, 0.0f, static_cast<float>(width_), static_cast<float>(height_)};
    scissor_rect_ = CD3DX12_RECT{0, 0, static_cast<LONG>(width_), static_cast<LONG>(height_)};
}

void viewport::apply(ID3D12GraphicsCommandList* command_list) const
{
    assert(command_list);
    command_list->RSSetViewports(1, &viewport_);
    command_list->RSSetScissorRects(1, &scissor_rect_);
}

std::uint32_t viewport::width() const noexcept
{
    return width_;
}

std::uint32_t viewport::height() const noexcept
{
    return height_;
}
} // namespace nori::graphics
