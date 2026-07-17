#pragma once
#include <string>
#include <Windows.h>
#include <nori/core/application.hpp>

namespace nori::core
{
class windows_application : public application
{
public:
    struct specification
    {
        std::wstring title;
        int width;
        int height;
    };

public:
    windows_application(specification spec);
    virtual ~windows_application() override = default;

    virtual void run() override;
    virtual void quit(application::exit_code exit_code) override;

    HWND hwnd() const;

protected:
    virtual LRESULT on_window_message(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);

private:
    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);

    void initialize();
    void tick(float delta_seconds);

private:
    specification spec_;
    HWND hwnd_;
};
} // namespace nori::core
