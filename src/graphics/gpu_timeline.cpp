#include <algorithm>
#include <cassert>
#include "gpu_timeline.hpp"

namespace nori::graphics
{
gpu_timeline::gpu_timeline(ID3D12Device* device, ID3D12CommandQueue* command_queue) :
    command_queue_{command_queue},
    fence_{device}
{
    assert(command_queue_);
}

bool gpu_timeline::signal(std::uint64_t& value)
{
    if (!fence_.signal(command_queue_, value))
    {
        return false;
    }
    for (deferred_release& item : deferred_releases_)
    {
        if (!item.fence_value)
        {
            item.fence_value = value;
        }
    }
    return true;
}

bool gpu_timeline::wait(std::uint64_t value)
{
    return fence_.wait(value);
}

bool gpu_timeline::wait_idle()
{
    std::uint64_t value{};
    return signal(value) && wait(value);
}

std::uint64_t gpu_timeline::completed_value() const
{
    return fence_.completed_value();
}

void gpu_timeline::defer(std::move_only_function<void()> release)
{
    if (release)
    {
        deferred_releases_.push_back({std::nullopt, std::move(release)});
    }
}

void gpu_timeline::collect()
{
    const std::uint64_t completed{completed_value()};
    std::erase_if(
        deferred_releases_,
        [completed](deferred_release& item)
        {
            if (!item.fence_value || *item.fence_value > completed)
            {
                return false;
            }
            item.release();
            return true;
        }
    );
}
} // namespace nori::graphics
