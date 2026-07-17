#pragma once
#include <cstdint>
#include <directx/d3dx12.h>

namespace nori::graphics
{
class viewport
{
public:
    viewport(std::uint32_t width, std::uint32_t height);

    void resize(std::uint32_t width, std::uint32_t height);
    void apply(ID3D12GraphicsCommandList* command_list) const;

    [[nodiscard]]
    std::uint32_t width() const noexcept;
    [[nodiscard]]
    std::uint32_t height() const noexcept;

private:
    D3D12_VIEWPORT viewport_;
    D3D12_RECT scissor_rect_;
    std::uint32_t width_;
    std::uint32_t height_;
};
} // namespace nori::graphics
