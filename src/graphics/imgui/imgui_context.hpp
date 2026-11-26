#pragma once
#include <memory>

#include <nori/core/vector.hpp>
#include <nori/graphics/imgui.hpp>

struct ID3D12GraphicsCommandList;
struct ID3D12Device;
struct ID3D12CommandQueue;

namespace nori::graphics
{
class descriptor_allocator;
}

namespace nori::graphics::imgui
{
class context
{
public:
    struct params
    {
        HWND hwnd;
        ID3D12Device* device;
        ID3D12CommandQueue* queue;
        descriptor_allocator* descriptors;
    };

    ~context() noexcept;

    context(const context&) = delete;
    context(context&&) = delete;
    context& operator=(const context&) = delete;
    context& operator=(context&&) = delete;

    static std::unique_ptr<context> create(params args);

    bool process_window_message(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);

    void begin_frame(core::vec2 render_target_size);
    void end_frame(ID3D12GraphicsCommandList* command_list);
    bool render_platform_windows(); // 렌더링 수행했으면 true

private:
    context();

    bool is_rendered_;
    core::vec2 render_target_size_;
};

bool render_platform_windows();
} // namespace nori::graphics::imgui
