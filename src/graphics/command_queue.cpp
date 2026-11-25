#include <cassert>
#include <limits>
#include <utility>

#include <Windows.h>

#include <nori/core/abort.hpp>

#include "command_queue.hpp"

namespace nori::graphics
{
command_queue::~command_queue() noexcept
{
    discard();
    if (has_pending_work_ && !wait_idle())
    {
        core::abort("gpu shutdown synchronization failed");
    }
    collect();
    ::CloseHandle(event_);
}

std::unique_ptr<command_queue> command_queue::create(ID3D12Device* device)
{
    constexpr D3D12_COMMAND_QUEUE_DESC desc{};
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    if (FAILED(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&queue))) ||
        FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
    {
        return nullptr;
    }
    void* const event{::CreateEventW(nullptr, FALSE, FALSE, nullptr)};
    if (!event)
    {
        return nullptr;
    }
    return std::unique_ptr<command_queue>{new command_queue{device, std::move(queue), std::move(fence), event}};
}

bool command_queue::begin_frame()
{
    assert(!is_frame_active_);
    if (const auto res{submit()}; !res)
    {
        return false;
    }

    const std::uint64_t fence{frame_fences_.at(frame_index_)};
    if (fence != 0 && !wait(fence))
    {
        return false;
    }
    if (!record())
    {
        return false;
    }
    is_frame_active_ = true;
    return true;
}

void command_queue::end_frame()
{
    frame_fences_.at(frame_index_) = last_signaled_fence_value_;
    frame_index_ = (frame_index_ + 1) % frames_in_flight;
    for (auto& resource : frame_releases_)
    {
        deferred_releases_.push_back({last_signaled_fence_value_, std::move(resource)});
    }
    frame_releases_.clear();
    is_frame_active_ = false;
}

ID3D12GraphicsCommandList* command_queue::record()
{
    if (!recording_)
    {
        collect();
        command_context ctx{};
        if (pool_.empty())
        {
            if (FAILED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&ctx.allocator))) ||
                FAILED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, ctx.allocator.Get(), nullptr, IID_PPV_ARGS(&ctx.list))))
            {
                return nullptr;
            }
        }
        else
        {
            ctx = std::move(pool_.top());
            pool_.pop();
            if (FAILED(ctx.allocator->Reset()) || FAILED(ctx.list->Reset(ctx.allocator.Get(), nullptr)))
            {
                return nullptr;
            }
        }
        ctx.completion = std::make_shared<command_submission>(0, false);
        recording_ = std::move(ctx);
    }
    return recording_->list.Get();
}

void command_queue::retain(IUnknown* resource)
{
    assert(recording_);
    recording_->resources.emplace_back(resource);
}

void command_queue::on_completed(std::move_only_function<void()> callback)
{
    assert(recording_);
    if (callback)
    {
        recording_->callbacks.push_back(std::move(callback));
    }
}

result<std::uint64_t> command_queue::submit()
{
    if (!recording_)
    {
        const std::uint64_t value{signal()};
        if (!value)
        {
            return std::unexpected{error_code::synchronization_failed};
        }
        return value;
    }
    if (FAILED(recording_->list->Close()))
    {
        discard();
        return std::unexpected{error_code::command_failed};
    }

    command_context& ctx{pending_.emplace_back(std::move(*recording_))};
    recording_.reset();

    ID3D12CommandList* const list{ctx.list.Get()};
    queue_->ExecuteCommandLists(1, &list);
    has_pending_work_ = true;

    if (signal() == 0)
    {
        ctx.completion->failed = true;
        return std::unexpected{error_code::synchronization_failed};
    }

    ctx.completion->fence_value = last_signaled_fence_value_;
    return last_signaled_fence_value_;
}

void command_queue::discard()
{
    if (recording_)
    {
        recording_->completion->failed = true;
        recording_.reset();
    }
}

void command_queue::collect()
{
    const std::uint64_t completed{completed_fence_value()};
    if (completed == std::numeric_limits<std::uint64_t>::max())
    {
        return;
    }
    std::erase_if(pending_, [this, completed](command_context& ctx)
    {
        if (!ctx.completion->fence_value || ctx.completion->fence_value > completed)
        {
            return false;
        }
        for (auto& callback : ctx.callbacks)
        {
            callback();
        }
        ctx.resources.clear();
        ctx.callbacks.clear();
        ctx.completion.reset();
        pool_.push(std::move(ctx));
        return true;
    });
    std::erase_if(deferred_releases_, [completed](const deferred_release& item)
    {
        return item.fence_value && item.fence_value <= completed;
    });
}

std::uint64_t command_queue::signal()
{
    const std::uint64_t value{next_fence_value_};
    if (FAILED(queue_->Signal(fence_.Get(), value)) || completed_fence_value() == std::numeric_limits<std::uint64_t>::max())
    {
        return 0;
    }

    ++next_fence_value_;
    last_signaled_fence_value_ = value;
    has_pending_work_ = true;
    for (auto& item : deferred_releases_)
    {
        if (!item.fence_value)
        {
            item.fence_value = value;
        }
    }
    return value;
}

bool command_queue::poll(std::uint64_t value)
{
    const std::uint64_t completed{completed_fence_value()};
    if (completed == std::numeric_limits<std::uint64_t>::max())
    {
        return false;
    }
    collect();
    return value <= completed;
}

bool command_queue::wait(std::uint64_t value)
{
    const std::uint64_t completed{completed_fence_value()};
    if (completed == std::numeric_limits<std::uint64_t>::max())
    {
        return false;
    }
    if (value <= completed)
    {
        return true;
    }
    if (FAILED(fence_->SetEventOnCompletion(value, event_)))
    {
        return false;
    }
    return ::WaitForSingleObject(event_, INFINITE) == WAIT_OBJECT_0 &&
        completed_fence_value() != std::numeric_limits<std::uint64_t>::max();
}

bool command_queue::wait_idle()
{
    const std::uint64_t value{signal()};
    if (value == 0 || !wait(value))
    {
        return false;
    }
    has_pending_work_ = false;
    return true;
}

ID3D12GraphicsCommandList* command_queue::recording_list() const
{
    assert(recording_);
    return recording_->list.Get();
}

std::shared_ptr<command_submission> command_queue::submission() const
{
    assert(recording_);
    return recording_->completion;
}

std::uint32_t command_queue::frame_index() const
{
    return frame_index_;
}

std::uint64_t command_queue::completed_fence_value() const
{
    return fence_->GetCompletedValue();
}

ID3D12CommandQueue* command_queue::native() const
{
    return queue_.Get();
}

command_queue::resource_holder_base::~resource_holder_base() = default;

command_queue::command_queue(ID3D12Device* device, Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue, Microsoft::WRL::ComPtr<ID3D12Fence> fence, void* event) :
    device_{device},
    queue_{std::move(queue)},
    fence_{std::move(fence)},
    event_{event},
    next_fence_value_{1},
    last_signaled_fence_value_{0},
    has_pending_work_{false},
    is_frame_active_{false},
    frame_index_{0},
    frame_fences_{}
{
}

void command_queue::enqueue(std::unique_ptr<resource_holder_base> resource)
{
    if (is_frame_active_)
    {
        frame_releases_.push_back(std::move(resource));
    }
    else
    {
        deferred_releases_.push_back({0, std::move(resource)});
    }
}
} // namespace nori::graphics
