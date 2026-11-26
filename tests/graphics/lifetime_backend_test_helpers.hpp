#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <utility>

#include <wrl/client.h>

#include <gtest/gtest.h>

#include <nori/graphics.hpp>

#include "command_queue.hpp"
#include "context.hpp"

namespace
{
inline nori::graphics::result<void> wait_gpu(std::uint64_t fence_value = 0)
{
    const auto graphics_context{nori::graphics::ctx()};
    if (!(fence_value ? graphics_context->queue()->wait(fence_value) : graphics_context->queue()->wait_idle()))
    {
        return std::unexpected{nori::graphics::error_code::synchronization_failed};
    }
    if (!fence_value)
    {
        graphics_context->queue()->collect();
    }
    return {};
}

std::uint64_t completed_fence()
{
    const auto active{nori::graphics::ctx()};
    return active->queue()->completed_fence_value();
}

class destruction_probe
{
public:
    explicit destruction_probe(std::shared_ptr<std::uint32_t> count) :
        count_{std::move(count)}
    {
    }

    ~destruction_probe()
    {
        if (count_)
        {
            ++*count_;
        }
    }

    destruction_probe(const destruction_probe&) = delete;
    destruction_probe(destruction_probe&&) noexcept = default;
    destruction_probe& operator=(const destruction_probe&) = delete;
    destruction_probe& operator=(destruction_probe&&) = delete;

private:
    std::shared_ptr<std::uint32_t> count_;
};

// ASSERT로 테스트가 조기 종료되어도 fallback deadline에 gate 해제
class queue_gate
{
public:
    explicit queue_gate(Microsoft::WRL::ComPtr<ID3D12Fence> fence) :
        release_{false},
        worker_{[this, fence = std::move(fence)]()
    {
        const auto deadline{std::chrono::steady_clock::now() + std::chrono::seconds{2}};
        while (!release_.load() && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::yield();
        }
        EXPECT_TRUE(SUCCEEDED(fence->Signal(1)));
    }}
    {
    }

    ~queue_gate()
    {
        open();
    }

    void open()
    {
        release_.store(true);
    }

private:
    std::atomic<bool> release_;
    std::jthread worker_;
};
} // namespace
