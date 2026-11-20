#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <thread>
#include <utility>

#include <gtest/gtest.h>

#include <imgui.h>

#include <nori/graphics.hpp>
#include <nori/graphics/imgui.hpp>

#include "../graphics/graphics_test_helpers.hpp"

namespace
{
class render_test : public shared_graphics_device_test
{
protected:
    render_test() :
        window_{nullptr},
        color_{},
        depth_{},
        target_{}
    {
    }

    void SetUp() override
    {
        const WNDCLASSW window_class{.lpfnWndProc = ::DefWindowProcW, .hInstance = ::GetModuleHandleW(nullptr), .lpszClassName = L"NoriImGuiTests"};
        ASSERT_NE(::RegisterClassW(&window_class), 0);
        window_ = ::CreateWindowExW(0, window_class.lpszClassName, L"Nori ImGui tests", WS_POPUP, 0, 0, 256, 256, nullptr, nullptr, window_class.hInstance, nullptr);
        ASSERT_NE(window_, nullptr);
        ASSERT_TRUE(nori::graphics::initialize());
        ASSERT_TRUE(nori::graphics::imgui::initialize(window_));
        ImGui::GetIO().IniFilename = nullptr;
        const auto color{nori::graphics::create_texture({nori::graphics::texture_format::rgba8_unorm, 256, 256, 1, 1, nori::graphics::texture_usage::render_target | nori::graphics::texture_usage::copy_source}).transform([](auto&& owner)
        {
            return owner.release();
        })};
        ASSERT_TRUE(color);
        color_ = *color;
        const auto depth{nori::graphics::create_texture({nori::graphics::texture_format::d32_float, 256, 256, 1, 1, nori::graphics::texture_usage::depth_stencil}).transform([](auto&& owner)
        {
            return owner.release();
        })};
        ASSERT_TRUE(depth);
        depth_ = *depth;
        target_ = {
            .render_targets = {{color_, {nori::graphics::begin_op::clear}, {nori::graphics::end_op::store}}},
            .depth_stencil = {{depth_, {nori::graphics::begin_op::clear, {.depth = 1.0f}}, {nori::graphics::begin_op::clear}, {nori::graphics::end_op::store}, {nori::graphics::end_op::store}}}
        };
    }

    void TearDown() override
    {
        nori::graphics::imgui::shutdown();
        nori::graphics::shutdown();
        if (window_)
        {
            EXPECT_TRUE(::DestroyWindow(window_));
        }
        EXPECT_TRUE(::UnregisterClassW(L"NoriImGuiTests", ::GetModuleHandleW(nullptr)));
    }

    bool start_frame()
    {
        return nori::graphics::begin_frame();
    }

    bool begin_ui()
    {
        return nori::graphics::imgui::begin_render_pass(ui_pass());
    }

    nori::graphics::render_pass_desc ui_pass() const
    {
        return target_;
    }

    bool finish_ui()
    {
        return nori::graphics::imgui::end_render_pass();
    }

    bool finish_frame()
    {
        return nori::graphics::end_frame();
    }

    HWND window_;
    nori::graphics::texture_handle color_;
    nori::graphics::texture_handle depth_;
    nori::graphics::render_pass_desc target_;
};

class lifetime_test : public render_test
{
protected:
    static void SetUpTestSuite()
    {
    }

    static void TearDownTestSuite()
    {
    }
};

std::optional<nori::graphics::texture_readback> await_readback(nori::graphics::readback_handle request)
{
    if (!nori::graphics::submit())
    {
        return std::nullopt;
    }
    const auto deadline{std::chrono::steady_clock::now() + std::chrono::seconds{5}};
    do
    {
        auto status{nori::graphics::poll(request)};
        if (status.status == nori::graphics::readback_status::failed)
        {
            return std::nullopt;
        }
        if (status.status == nori::graphics::readback_status::ready)
        {
            return std::move(status.value);
        }
        std::this_thread::yield();
    } while (std::chrono::steady_clock::now() < deadline);
    return std::nullopt;
}
} // namespace
