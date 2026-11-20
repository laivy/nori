#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <thread>
#include <utility>

#include <gtest/gtest.h>

#include <nori/graphics.hpp>

#include "graphics_test_helpers.hpp"
#include "test_shader/quad_ps.hpp"
#include "test_shader/quad_vs.hpp"

using namespace nori::graphics;

namespace
{
class frame_upload_buffer_test : public shared_graphics_device_test
{
protected:
    frame_upload_buffer_test() :
        target_{},
        atlas_{},
        pipeline_{},
        sampler_{}
    {
    }

    void SetUp() override
    {
        ASSERT_TRUE(initialize());
        const auto target{create_texture({texture_format::rgba8_unorm, 4, 4, 1, 1, texture_usage::render_target | texture_usage::copy_source}).transform([](auto&& owner)
        {
            return owner.release();
        })};
        ASSERT_TRUE(target);
        target_ = *target;
        const auto atlas{create_texture({texture_format::r8_unorm, 1, 1, 1, 1, texture_usage::shader_resource | texture_usage::copy_destination}).transform([](auto&& owner)
        {
            return owner.release();
        })};
        ASSERT_TRUE(atlas);
        atlas_ = *atlas;
        constexpr std::array<std::byte, 1> white{std::byte{255}};
        ASSERT_TRUE(update(atlas_, {0, 0, 1, 1}, {white, 1}));
        const auto vs{create_shader(std::as_bytes(std::span{quad_vs})).release()};
        const auto ps{create_shader(std::as_bytes(std::span{quad_ps})).release()};
        constexpr std::array bindings{
            shader_binding{shader_binding_type::constant_buffer, 0, shader_stage::vertex | shader_stage::pixel},
            shader_binding{shader_binding_type::texture_srv, 0, shader_stage::pixel},
            shader_binding{shader_binding_type::sampler, 0, shader_stage::pixel}
        };
        constexpr std::array elements{
            input_element{"POSITION", 0, input_element_format::float2, 0},
            input_element{"TEXCOORD", 0, input_element_format::float2, 8}
        };
        constexpr std::array formats{texture_format::rgba8_unorm};
        constexpr std::array blends{render_target_blend_desc{false, blend_factor::one, blend_factor::zero, blend_op::add, blend_factor::one, blend_factor::zero, blend_op::add, color_write_mask::all}};
        pipeline_ = create_graphics_pipeline({vs, ps, bindings, formats, std::nullopt, {16, elements}, primitive_topology::triangle_list, {fill_mode::solid, cull_mode::none, front_face::clockwise}, std::nullopt, std::nullopt, blends, 1}).release();
        ASSERT_TRUE(pipeline_);
        sampler_ = create_sampler({sampler_filter::point, sampler_filter::point, sampler_filter::point, sampler_address_mode::clamp, sampler_address_mode::clamp, sampler_address_mode::clamp, 1, 0, 0}).release();
        ASSERT_TRUE(sampler_);
    }

    void TearDown() override
    {
        shutdown();
    }

    render_pass_desc pass(begin_op load = begin_op::clear) const
    {
        return {{{target_, {load, {}}, {end_op::store}}}, std::nullopt};
    }

    static constexpr std::array<float, 16> quad(float left, float right)
    {
        return {left, 0, 0, 0, right, 0, 0, 0, left, 4, 0, 0, right, 4, 0, 0};
    }

    void draw(command_encoder& encoder, frame_upload_buffer_view vertices, frame_upload_buffer_view indices, bool green, index_format format = index_format::uint16) const
    {
        const std::array<float, 8> constants{4, 4, 0, 0, green ? 0.0f : 1.0f, green ? 1.0f : 0.0f, 0, 1};
        encoder.set_constant_buffer(0, std::as_bytes(std::span{constants}));
        bind_and_draw(encoder, vertices, indices, format);
    }

    void draw(command_encoder& encoder, frame_upload_buffer_view vertices, frame_upload_buffer_view indices, frame_upload_buffer_view constant_buffer) const
    {
        encoder.set_constant_buffer(0, constant_buffer);
        bind_and_draw(encoder, vertices, indices, index_format::uint16);
    }

    void bind_and_draw(command_encoder& encoder, frame_upload_buffer_view vertices, frame_upload_buffer_view indices, index_format format) const
    {
        encoder.set_pipeline(pipeline_);
        encoder.set_texture(1, atlas_);
        encoder.set_sampler(2, sampler_);
        encoder.set_vertex_buffer(vertices, 16);
        encoder.set_index_buffer(indices, format);
        encoder.draw_indexed(6);
    }

    void expect_readback(readback_handle rb, std::uint32_t left, std::uint32_t right)
    {
        ASSERT_TRUE(submit_and_wait());
        const auto result{poll(rb)};
        ASSERT_EQ(result.status, readback_status::ready);
        for (std::size_t i{0}; i < 16; ++i)
        {
            std::uint32_t pixel{};
            std::memcpy(&pixel, result.value.data.data() + i * 4, 4);
            EXPECT_EQ(pixel, i % 4 < 2 ? left : right) << i;
        }
        destroy(rb);
    }

    void expect_columns()
    {
        const auto rb{readback(target_, {0, 0, 4, 4}).transform([](auto&& owner)
        {
            return owner.release();
        })};
        ASSERT_TRUE(rb);
        expect_readback(*rb, 0xFF0000FFu, 0xFF00FF00u);
    }

    texture_handle target_;
    texture_handle atlas_;
    pipeline_handle pipeline_;
    sampler_handle sampler_;
};

class frame_upload_buffer_lifetime_test : public frame_upload_buffer_test
{
protected:
    static void SetUpTestSuite()
    {
    }

    static void TearDownTestSuite()
    {
    }
};
} // namespace
