#pragma once
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>
#include "fence.hpp"

namespace nori::graphics
{
class gpu_timeline
{
private:
    struct deferred_release
    {
        std::optional<std::uint64_t> fence_value;
        std::move_only_function<void()> release;
    };

public:
    gpu_timeline(ID3D12Device* device, ID3D12CommandQueue* command_queue);

    bool signal(std::uint64_t& value);
    bool wait(std::uint64_t value);
    bool wait_idle();

    std::uint64_t completed_value() const;
    void defer(std::move_only_function<void()> release);
    void collect();

private:
    ID3D12CommandQueue* const command_queue_;
    fence fence_;
    std::vector<deferred_release> deferred_releases_;
};
} // namespace nori::graphics
