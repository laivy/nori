#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stack>
#include <utility>
#include <vector>

#include <directx/d3d12.h>
#include <wrl/client.h>

#include <nori/graphics/error_code.hpp>

#include "config.hpp"

namespace nori::graphics
{
struct command_submission
{
    std::uint64_t fence_value;
    bool failed;
};

class command_queue
{
public:
    ~command_queue() noexcept;

    command_queue(const command_queue&) = delete;
    command_queue(command_queue&&) = delete;
    command_queue& operator=(const command_queue&) = delete;
    command_queue& operator=(command_queue&&) = delete;

    static std::unique_ptr<command_queue> create(ID3D12Device* device);

    bool begin_frame();
    void end_frame();

    ID3D12GraphicsCommandList* record();
    void retain(IUnknown* resource);
    void on_completed(std::move_only_function<void()> callback);
    result<std::uint64_t> submit();
    void discard();

    template <typename T>
    void release(T resource)
    {
        enqueue(std::make_unique<resource_holder<T>>(std::move(resource)));
    }

    void collect();

    [[nodiscard]] std::uint64_t signal();
    bool poll(std::uint64_t value);
    bool wait(std::uint64_t value);
    bool wait_idle();

    ID3D12GraphicsCommandList* recording_list() const;
    std::shared_ptr<command_submission> submission() const;
    std::uint32_t frame_index() const;
    std::uint64_t completed_fence_value() const;
    ID3D12CommandQueue* native() const;

private:
    class resource_holder_base
    {
    public:
        virtual ~resource_holder_base();
    };

    template <typename T>
    class resource_holder final : public resource_holder_base
    {
    public:
        explicit resource_holder(T resource) :
            resource_{std::move(resource)}
        {
        }

    private:
        T resource_;
    };

    struct command_context
    {
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list;
        std::vector<Microsoft::WRL::ComPtr<IUnknown>> resources;
        std::vector<std::move_only_function<void()>> callbacks;
        std::shared_ptr<command_submission> completion;
    };

    struct deferred_release
    {
        std::uint64_t fence_value;
        std::unique_ptr<resource_holder_base> resource;
    };

    command_queue(ID3D12Device* device, Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue, Microsoft::WRL::ComPtr<ID3D12Fence> fence, void* event);

    void enqueue(std::unique_ptr<resource_holder_base> resource);

    ID3D12Device* const device_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    void* const event_;

    std::uint64_t next_fence_value_;
    std::uint64_t last_signaled_fence_value_;
    bool has_pending_work_;

    bool is_frame_active_;
    std::uint32_t frame_index_;
    std::array<std::uint64_t, frames_in_flight> frame_fences_;

    std::stack<command_context, std::vector<command_context>> pool_;
    std::vector<command_context> pending_;
    std::optional<command_context> recording_;

    std::vector<deferred_release> deferred_releases_;
    std::vector<std::unique_ptr<resource_holder_base>> frame_releases_;
};
} // namespace nori::graphics
