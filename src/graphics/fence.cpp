#include <cassert>
#include "fence.hpp"

namespace nori::graphics
{
fence::fence(ID3D12Device* device) :
    value_{1},
    event_{nullptr}
{
    assert(device);
    if (const HRESULT result{device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_))}; FAILED(result))
    {
        assert(false && "failed to create fence");
    }

    event_ = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
    assert(event_);
}

fence::~fence()
{
    if (event_)
    {
        ::CloseHandle(event_);
        event_ = nullptr;
    }
}

bool fence::signal(ID3D12CommandQueue* command_queue, std::uint64_t& value)
{
    assert(fence_);
    assert(command_queue);
    value = value_;
    if (FAILED(command_queue->Signal(fence_.Get(), value)))
    {
        return false;
    }
    ++value_;
    return true;
}

bool fence::wait(std::uint64_t value)
{
    assert(fence_);
    assert(event_);
    // 이미 완료됨
    if (value <= fence_->GetCompletedValue())
    {
        return true;
    }
    // 완료될 때까지 대기
    if (FAILED(fence_->SetEventOnCompletion(value, event_)))
    {
        return false;
    }
    if (::WaitForSingleObject(event_, INFINITE) == WAIT_FAILED)
    {
        return false;
    }
    return true;
}

std::uint64_t fence::completed_value() const
{
    assert(fence_);
    return fence_->GetCompletedValue();
}
} // namespace nori::graphics
