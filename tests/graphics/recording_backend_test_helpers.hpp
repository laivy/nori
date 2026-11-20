#pragma once

#include <array>
#include <cstring>
#include <memory>

#include <gtest/gtest.h>

#include <nori/graphics.hpp>

#include "command_queue.hpp"
#include "context.hpp"
#include "graphics_test_helpers.hpp"
#include "resource/buffer_manager.hpp"
#include "resource/texture_manager.hpp"
#include "transfer/manager.hpp"
#include "upload_manager.hpp"

using namespace nori::graphics;

namespace
{
class recording_backend_test : public shared_graphics_device_test
{
protected:
    void SetUp() override
    {
        auto graphics_context{context::create()};
        ASSERT_TRUE(graphics_context);
        graphics_context_ = std::move(graphics_context);
        uploads_ = std::make_unique<upload_manager>(upload_manager::params{graphics_context_->device(), graphics_context_->queue()});
        textures_ = std::make_unique<resource::texture_manager>(resource::texture_manager::params{graphics_context_->device(), graphics_context_->descriptors(), uploads_.get(), graphics_context_->queue()});
        buffers_ = std::make_unique<resource::buffer_manager>(resource::buffer_manager::params{graphics_context_->device(), uploads_.get(), graphics_context_->queue()});
        transfers_ = std::make_unique<transfer::manager>(transfer::manager::params{graphics_context_->device(), graphics_context_->descriptors(), graphics_context_->queue(), buffers_.get(), textures_.get()});
    }

    void TearDown() override
    {
        if (!graphics_context_)
        {
            return;
        }
        graphics_context_->queue()->discard();
        EXPECT_TRUE(graphics_context_->queue()->wait_idle());
        transfers_.reset();
        buffers_.reset();
        textures_.reset();
        uploads_.reset();
        graphics_context_.reset();
    }

    result<texture_handle> create_color(std::uint32_t size = 4, std::uint16_t mips = 1)
    {
        return textures_->create({texture_format::rgba8_unorm, size, size, mips, 1, texture_usage::shader_resource | texture_usage::render_target | texture_usage::copy_source | texture_usage::copy_destination});
    }

    void expect_pixels(readback_handle handle, std::uint32_t expected)
    {
        ASSERT_TRUE(graphics_context_->queue()->submit());
        ASSERT_TRUE(graphics_context_->queue()->wait_idle());
        const auto status{transfers_->poll(handle)};
        ASSERT_EQ(status.status, readback_status::ready);
        for (std::size_t offset{0}; offset < status.value.data.size(); offset += 4)
        {
            std::uint32_t pixel{};
            std::memcpy(&pixel, status.value.data.data() + offset, 4);
            EXPECT_EQ(pixel, expected);
        }
    }

    std::unique_ptr<context> graphics_context_;
    std::unique_ptr<upload_manager> uploads_;
    std::unique_ptr<resource::texture_manager> textures_;
    std::unique_ptr<resource::buffer_manager> buffers_;
    std::unique_ptr<transfer::manager> transfers_;
};
} // namespace
