#pragma once
#include <cstdint>
#include <functional>
#include <queue>
#include <vector>
#include <Windows.h>
#include <directx/d3dx12.h>
#include <wrl/client.h>

namespace nori::graphics
{
class gpu_timeline;

class upload_manager
{
private:
    struct command_context
    {
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> command_allocator;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> command_list;
    };

    struct upload_info
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
        command_context command_context;
        std::uint64_t fence_value;
        std::function<void()> on_completed;
    };

public:
    upload_manager(ID3D12Device* device, ID3D12CommandQueue* command_queue, gpu_timeline* timeline);
    ~upload_manager();

    upload_manager(const upload_manager&) = delete;
    upload_manager(upload_manager&&) = delete;
    upload_manager& operator=(const upload_manager&) = delete;
    upload_manager& operator=(upload_manager&&) = delete;

    bool upload(
        ID3D12Resource* destination,
        const D3D12_SUBRESOURCE_DATA& subresource,
        D3D12_RESOURCE_STATES before_state,
        D3D12_RESOURCE_STATES after_state,
        const std::function<void()>& on_completed
    );
    bool flush();

    void collect();

private:
    command_context acquire_command_context();

private:
    ID3D12Device* const device_;
    ID3D12CommandQueue* const command_queue_;
    gpu_timeline* const timeline_;
    std::vector<upload_info> pending_uploads_;
    std::queue<command_context> command_contexts_;
};
} // namespace nori::graphics
