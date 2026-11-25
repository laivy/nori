#pragma once

#include <array>
#include <barrier>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

#include <gtest/gtest.h>

#include <nori/graphics.hpp>

#include "graphics_test_helpers.hpp"
#include "test_shader/quad_ps.hpp"
#include "test_shader/quad_vs.hpp"
#include "test_shader/sample_ps.hpp"
#include "test_shader/sample_vs.hpp"

using namespace nori::graphics;

namespace
{
class transfer_order_test : public shared_graphics_device_test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(initialize());
    }

    void TearDown() override
    {
        shutdown();
    }

    result<texture_handle> create_color(std::uint32_t size = 4, std::uint16_t mips = 1)
    {
        return create_texture({texture_format::rgba8_unorm, size, size, mips, 1, texture_usage::shader_resource | texture_usage::render_target | texture_usage::copy_source | texture_usage::copy_destination}).transform([](auto&& owner)
        {
            return owner.release();
        });
    }

    void expect_pixels(readback_handle handle, std::uint32_t expected)
    {
        ASSERT_TRUE(submit_and_wait());
        const auto status{poll(handle)};
        ASSERT_EQ(status.status, readback_status::ready);
        ASSERT_FALSE(status.value.data.empty());
        for (std::size_t offset{0}; offset < status.value.data.size(); offset += 4)
        {
            std::uint32_t pixel{};
            std::memcpy(&pixel, status.value.data.data() + offset, 4);
            EXPECT_EQ(pixel, expected);
        }
    }
};

class command_encoder_test : public transfer_order_test
{
protected:
    command_encoder_test() :
        source_{},
        targets_{},
        vertex_{},
        pixel_{},
        pipeline_{},
        sampler_{}
    {
    }

    void SetUp() override
    {
        ASSERT_NO_FATAL_FAILURE(transfer_order_test::SetUp());
        const auto source{create_texture({texture_format::rgba8_unorm, 2, 1, 1, 1, texture_usage::shader_resource | texture_usage::copy_destination}).transform([](auto&& owner)
        {
            return owner.release();
        })};
        ASSERT_TRUE(source);
        source_ = *source;
        const std::array<std::uint32_t, 2> pixels{0xFF0000FF, 0xFF00FF00};
        ASSERT_TRUE(update(source_, {0, 0, 2, 1}, {std::as_bytes(std::span{pixels}), 8}));
        for (auto& target : targets_)
        {
            const auto color{create_texture({texture_format::rgba8_unorm, 4, 4, 1, 1, texture_usage::render_target | texture_usage::copy_source}).transform([](auto&& owner)
            {
                return owner.release();
            })};
            ASSERT_TRUE(color);
            target = *color;
        }
        vertex_ = create_shader(std::as_bytes(std::span{sample_vs})).release();
        pixel_ = create_shader(std::as_bytes(std::span{sample_ps})).release();
        const std::array bindings{
            shader_binding{shader_binding_type::constant_buffer, 0, shader_stage::pixel},
            shader_binding{shader_binding_type::texture_srv, 0, shader_stage::pixel},
            shader_binding{shader_binding_type::sampler, 0, shader_stage::pixel},
        };
        const std::array formats{texture_format::rgba8_unorm};
        const std::array blends{render_target_blend_desc{false, blend_factor::one, blend_factor::zero, blend_op::add, blend_factor::one, blend_factor::zero, blend_op::add, color_write_mask::all}};
        const auto pipeline{create_graphics_pipeline({
                                                         .vertex_shader = vertex_,
                                                         .pixel_shader = pixel_,
                                                         .bindings = bindings,
                                                         .render_target_formats = formats,
                                                         .topology = primitive_topology::triangle_list,
                                                         .render_target_blends = blends,
                                                         .sample_count = 1,
                                                     })
                .release()};
        ASSERT_TRUE(pipeline);
        pipeline_ = pipeline;
        const auto sampler{create_sampler({sampler_filter::point, sampler_filter::point, sampler_filter::point, sampler_address_mode::clamp, sampler_address_mode::clamp, sampler_address_mode::clamp, 1, 0, 0}).release()};
        ASSERT_TRUE(sampler);
        sampler_ = sampler;
    }

    void write(command_encoder& list, bool green) const
    {
        const std::array<float, 4> constants{green ? 1.0f : 0.0f, 0, 0, 0};
        list.set_pipeline(pipeline_);
        list.set_constant_buffer(0, std::as_bytes(std::span{constants}));
        list.set_texture(1, source_);
        list.set_sampler(2, sampler_);
        list.draw(3);
    }

    render_pass_desc pass(texture_handle color) const
    {
        return {{{color, {begin_op::clear}, {end_op::store}}}, std::nullopt};
    }

    void expect_columns(texture_handle color, std::uint32_t left, std::uint32_t right)
    {
        const auto request{readback(color, {0, 0, 4, 4}).transform([](auto&& owner)
        {
            return owner.release();
        })};
        ASSERT_TRUE(request);
        ASSERT_TRUE(submit_and_wait());
        const auto status{poll(*request)};
        ASSERT_EQ(status.status, readback_status::ready);
        ASSERT_EQ(status.value.data.size(), 64u);
        for (std::uint32_t row{0}; row < 4; ++row)
        {
            for (std::uint32_t column{0}; column < 4; ++column)
            {
                std::uint32_t pixel{};
                std::memcpy(&pixel, status.value.data.data() + (row * 4 + column) * 4, 4);
                EXPECT_EQ(pixel, column < 2 ? left : right) << "row=" << row << ", column=" << column;
            }
        }
        destroy(*request);
    }

    texture_handle source_;
    std::array<texture_handle, 2> targets_;
    shader_handle vertex_;
    shader_handle pixel_;
    pipeline_handle pipeline_;
    sampler_handle sampler_;
};
} // namespace
