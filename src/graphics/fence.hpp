#pragma once
#include <cstdint>
#include <Windows.h>
#include <directx/d3d12.h>
#include <wrl/client.h>

namespace nori::graphics
{
class fence
{
public:
    explicit fence(ID3D12Device* device);
    ~fence();

    fence(const fence&) = delete;
    fence(fence&&) = delete;
    fence& operator=(const fence&) = delete;
    fence& operator=(fence&&) = delete;

    bool signal(ID3D12CommandQueue* command_queue, std::uint64_t& value);
    bool wait(std::uint64_t value);
    std::uint64_t completed_value() const;

private:
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    std::uint64_t value_;
    HANDLE event_;
};
} // namespace nori::graphics
