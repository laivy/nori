#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <string>

#include <Windows.h>

#include <nori/core/application.hpp>
#include <nori/core/input_code.hpp>

namespace nori::core
{
class windows_application : public application
{
public:
    struct specification
    {
        std::string title;
        int width;
        int height;
        bool resizable;
        std::uint32_t target_frame_rate;
        std::uint32_t fixed_update_rate;
        std::uint32_t max_fixed_updates_per_frame;
    };

    explicit windows_application(const specification& spec);

    [[nodiscard]] int run() override final;

    [[nodiscard]] HWND hwnd() const noexcept;

protected:
    virtual void on_render();

    virtual LRESULT on_window_message(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);

    virtual void on_window_resize(int width, int height);
    virtual void on_window_focus(bool focused);
    virtual void on_window_close();

    virtual void on_key_down(key_code key, bool repeat);
    virtual void on_key_up(key_code key);
    virtual void on_text_input(char32_t character);

    virtual void on_mouse_move(int x, int y);
    virtual void on_mouse_button_down(mouse_button button, int x, int y);
    virtual void on_mouse_button_up(mouse_button button, int x, int y);
    virtual void on_mouse_wheel(int x, int y, float delta);

private:
    struct mouse_motion
    {
        int x;
        int y;
    };

    struct size_motion
    {
        int width;
        int height;
        bool minimized;
    };

    struct wheel_motion
    {
        int x;
        int y;
        float delta;
    };

    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);
    LRESULT process_window_message(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);

    bool initialize();
    void shutdown();

    void on_platform_start() override final;
    void on_platform_begin_frame() override final;
    void on_platform_end_frame() override final;

    void enqueue_event(std::move_only_function<void()> callback);

    void dispatch_text_input(char32_t character);

    // 모션을 제외한 discrete 이벤트 전용 유한 큐 용량, 초과는 프로그래머 버그로 취급
    static constexpr std::size_t max_events_{2048};

    specification spec_;
    HWND hwnd_;
    std::atomic<bool> finished_;

    // 메시지 스레드
    std::array<bool, 3> mouse_buttons_;
    char16_t pending_high_surrogate_;

    // 프레임 스레드
    bool minimized_;

    // 스레드 공유
    std::mutex mutex_;
    std::queue<std::move_only_function<void()>> events_;
    std::optional<mouse_motion> mouse_;
    std::optional<size_motion> size_;
    std::optional<wheel_motion> wheel_;
};
} // namespace nori::core
