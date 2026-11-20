#pragma once

#include <chrono>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <nori/graphics.hpp>
#include <nori/graphics/render_pass.hpp>
#include <nori/resource.hpp>

#include "diagnostics_test_helpers.hpp"

namespace
{
class submission_queue_gate
{
public:
    explicit submission_queue_gate(Microsoft::WRL::ComPtr<ID3D12Fence> fence) :
        fence_{std::move(fence)}
    {
    }

    ~submission_queue_gate()
    {
        fence_->Signal(1);
    }

private:
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
};

bool submit_and_wait()
{
    const auto submission{nori::graphics::submit()};
    if (!submission)
    {
        return false;
    }
    nori::graphics::wait(submission);
    return nori::graphics::poll(submission);
}

nori::graphics::readback_result poll_until_complete(nori::graphics::readback_handle handle)
{
    const auto deadline{std::chrono::steady_clock::now() + std::chrono::seconds{5}};
    while (true)
    {
        auto result{nori::graphics::poll(handle)};
        if (result.status != nori::graphics::readback_status::pending || std::chrono::steady_clock::now() >= deadline)
        {
            return result;
        }
        std::this_thread::yield();
    }
}

class shared_graphics_device_test : public testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        device_ = nori::graphics::test_support::acquire_device();
        if (!device_)
        {
            GTEST_SKIP() << "graphics device unavailable, skipping GPU tests";
        }
    }

    static void TearDownTestSuite()
    {
        device_.Reset();
    }

private:
    inline static Microsoft::WRL::ComPtr<ID3D12Device> device_;
};

class initialized_graphics_test : public testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        if (!nori::graphics::initialize())
        {
            GTEST_SKIP() << "graphics device unavailable, skipping GPU tests";
        }
    }

    static void TearDownTestSuite()
    {
        nori::graphics::shutdown();
    }
};

class initialized_graphics_resource_test : public initialized_graphics_test
{
protected:
    static void SetUpTestSuite()
    {
        ASSERT_TRUE(nori::resource::initialize(std::filesystem::temp_directory_path()));
        if (!nori::graphics::initialize())
        {
            nori::resource::shutdown();
            GTEST_SKIP() << "graphics device unavailable, skipping GPU tests";
        }
    }

    static void TearDownTestSuite()
    {
        nori::graphics::shutdown();
        nori::resource::shutdown();
    }
};

template <typename T>
std::vector<std::byte> draw_and_read_pixels(T&& record)
{
    namespace graphics = nori::graphics;
    const auto target{graphics::create_texture({graphics::texture_format::rgba8_unorm, 64, 48, 1, 1, graphics::texture_usage::render_target | graphics::texture_usage::copy_source}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    EXPECT_TRUE(target);
    if (!target)
    {
        return {};
    }
    EXPECT_TRUE(graphics::begin_frame());
    const auto pass{graphics::begin_render_pass({.render_targets = {{*target, {graphics::begin_op::clear}, {graphics::end_op::store}}}})};
    const auto encoder{graphics::begin_encode(pass)};
    record(*encoder);
    end_encode(encoder);
    EXPECT_TRUE(graphics::end_render_pass(pass));
    EXPECT_TRUE(graphics::end_frame());
    const auto request{graphics::readback(*target, {0, 0, 64, 48, 0}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    EXPECT_TRUE(request);
    if (!request)
    {
        graphics::destroy(*target);
        return {};
    }
    std::vector<std::byte> pixels;
    EXPECT_TRUE(graphics::submit());
    const auto deadline{std::chrono::steady_clock::now() + std::chrono::seconds{5}};
    while (std::chrono::steady_clock::now() < deadline)
    {
        auto current{graphics::poll(*request)};
        EXPECT_NE(current.status, graphics::readback_status::failed);
        if (current.status == graphics::readback_status::ready)
        {
            pixels = std::move(current.value.data);
            break;
        }
        std::this_thread::yield();
    }
    EXPECT_FALSE(pixels.empty());
    graphics::destroy(*request);
    graphics::destroy(*target);
    return pixels;
}

std::vector<std::byte> load_test_font(const std::filesystem::path& path = std::filesystem::path{__FILE__}.parent_path() / "testdata" / "NanumGothic-Regular.ttf")
{
    std::ifstream stream{path, std::ios::binary | std::ios::ate};
    if (!stream)
    {
        return {};
    }
    const auto size{stream.tellg()};
    if (size <= 0)
    {
        return {};
    }
    std::vector<std::byte> result(static_cast<std::size_t>(size));
    stream.seekg(0);
    stream.read(reinterpret_cast<char*>(result.data()), size);
    return result;
}
} // namespace
