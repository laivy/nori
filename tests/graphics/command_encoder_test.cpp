#include "command_encoder_test_helpers.hpp"

namespace
{
TEST_F(command_encoder_test, successive_recording_sessions_preserve_previous_draws)
{
    ASSERT_TRUE(begin_frame());
    const auto target_pass{begin_render_pass(pass(targets_[0]))};
    auto first{begin_encode(target_pass)};
    write(*first, false);
    end_encode(first);
    auto second{begin_encode(target_pass)};
    second->set_scissor_rect({2, 0, 2, 4});
    write(*second, true);
    end_encode(second);
    ASSERT_TRUE(end_render_pass(target_pass));
    destroy(pipeline_);
    destroy(source_);
    destroy(sampler_);
    ASSERT_TRUE(end_frame());
    expect_columns(targets_[0], 0xFF0000FF, 0xFF00FF00);
}

TEST_F(command_encoder_test, snapshots_constants_and_preserves_bindings_after_draw)
{
    ASSERT_TRUE(begin_frame());
    const auto target_pass{begin_render_pass(pass(targets_[0]))};
    auto commands{begin_encode(target_pass)};
    commands->set_pipeline(pipeline_);
    commands->set_texture(1, source_);
    commands->set_sampler(2, sampler_);
    std::array<float, 4> constants{0, 0, 0, 0};
    commands->set_constant_buffer(0, std::as_bytes(std::span{constants}));
    constants[0] = 1.0f;
    commands->set_scissor_rect({0, 0, 2, 4});
    commands->draw(3);
    commands->draw(3);
    // constants arena 재할당 뒤에도 이전 draw의 바이트 범위 유지
    const std::array<std::byte, 65536> padding{};
    commands->set_constant_buffer(0, padding);
    commands->set_constant_buffer(0, std::as_bytes(std::span{constants}));
    constants[0] = 0.0f;
    commands->set_scissor_rect({2, 0, 2, 4});
    commands->draw_instanced(3, 2);
    end_encode(commands);
    ASSERT_TRUE(end_render_pass(target_pass));
    ASSERT_TRUE(end_frame());
    expect_columns(targets_[0], 0xFF0000FF, 0xFF00FF00);
}

TEST_F(command_encoder_test, successive_frames_preserve_each_frames_draws)
{
    constexpr std::uint32_t frame_count{7};
    for (std::uint32_t frame_index{0}; frame_index < frame_count; ++frame_index)
    {
        const bool is_last_frame{frame_index == frame_count - 1};
        ASSERT_TRUE(begin_frame());
        for (const auto target : targets_)
        {
            const auto target_pass{begin_render_pass(pass(target))};
            auto commands{begin_encode(target_pass)};
            write(*commands, is_last_frame);
            commands->draw(3);
            end_encode(commands);
            ASSERT_TRUE(end_render_pass(target_pass));
        }
        ASSERT_TRUE(end_frame());
        const std::uint32_t expected{is_last_frame ? 0xFF00FF00u : 0xFF0000FFu};
        for (const auto target : targets_)
        {
            expect_columns(target, expected, expected);
        }
    }
}

TEST_F(command_encoder_test, new_encoding_resets_viewport_and_scissor)
{
    ASSERT_TRUE(begin_frame());
    for (std::size_t index{0}; index < targets_.size(); ++index)
    {
        const auto target_pass{begin_render_pass(pass(targets_[index]))};
        auto area{begin_encode(target_pass)};
        if (index == 0)
        {
            area->set_viewport({{0, 0}, {2, 4}, {0, 1}});
        }
        else
        {
            area->set_scissor_rect({0, 0, 2, 4});
        }
        write(*area, false);
        end_encode(area);
        auto empty{begin_encode(target_pass)};
        end_encode(empty);
        auto full{begin_encode(target_pass)};
        write(*full, true);
        end_encode(full);
        ASSERT_TRUE(end_render_pass(target_pass));
    }
    ASSERT_TRUE(end_frame());
    for (const auto target : targets_)
    {
        expect_columns(target, 0xFF00FF00, 0xFF00FF00);
    }
}

TEST_F(command_encoder_test, parallel_recording_on_same_and_different_passes)
{
    ASSERT_TRUE(begin_frame());
    const std::array passes{begin_render_pass(pass(targets_[0])), begin_render_pass(pass(targets_[1]))};
    constexpr std::size_t worker_count{8};
    std::barrier ready{static_cast<std::ptrdiff_t>(worker_count)};
    std::array<std::jthread, worker_count> workers;
    for (std::size_t index{0}; index < worker_count; ++index)
    {
        workers[index] = std::jthread{[&, index]()
        {
            auto commands{begin_encode(passes[index / 4])};
            ready.arrive_and_wait();
            commands->set_scissor_rect({static_cast<std::uint32_t>(index % 4), 0, 1, 4});
            write(*commands, index % 4 >= 2);
            end_encode(commands);
        }};
    }
    for (auto& worker : workers)
    {
        worker.join();
    }
    ASSERT_TRUE(end_render_pass(passes[1]));
    ASSERT_TRUE(end_render_pass(passes[0]));
    ASSERT_TRUE(end_frame());
    for (const auto target : targets_)
    {
        expect_columns(target, 0xFF0000FF, 0xFF00FF00);
    }
}

TEST_F(command_encoder_test, native_pass_order_follows_end_calls)
{
    ASSERT_TRUE(begin_frame());
    auto red_desc{pass(targets_[0])};
    red_desc.render_targets[0].begin.clear.color = {1, 0, 0, 1};
    auto green_desc{pass(targets_[0])};
    green_desc.render_targets[0].begin.clear.color = {0, 1, 0, 1};
    const auto red_pass{begin_render_pass(red_desc)};
    const auto green_pass{begin_render_pass(green_desc)};
    ASSERT_TRUE(end_render_pass(green_pass));
    ASSERT_TRUE(end_render_pass(red_pass));
    ASSERT_TRUE(end_frame());
    expect_columns(targets_[0], 0xFF0000FF, 0xFF0000FF);
}

TEST_F(command_encoder_test, indexed_draw_variants)
{
    const std::array<std::uint16_t, 4> indices{0, 0, 1, 2};
    const auto index_buffer{create_buffer({sizeof(indices), buffer_usage::index, buffer_access::cpu_write}, std::as_bytes(std::span{indices})).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(index_buffer);
    ASSERT_TRUE(begin_frame());
    const auto target_pass{begin_render_pass(pass(targets_[0]))};
    auto commands{begin_encode(target_pass)};
    commands->set_scissor_rect({0, 0, 2, 4});
    write(*commands, false);
    commands->set_index_buffer(*index_buffer, index_format::uint16);
    commands->draw_indexed(3, 1);
    commands->set_scissor_rect({2, 0, 2, 4});
    const std::array<float, 4> green{1, 0, 0, 0};
    commands->set_constant_buffer(0, std::as_bytes(std::span{green}));
    commands->draw_indexed_instanced(3, 2, 1);
    end_encode(commands);
    ASSERT_TRUE(end_render_pass(target_pass));
    ASSERT_TRUE(end_frame());
    expect_columns(targets_[0], 0xFF0000FF, 0xFF00FF00);
    destroy(*index_buffer);
}

TEST_F(command_encoder_test, end_pass_records_without_submitting_transfers)
{
    const auto copied{create_texture({texture_format::rgba8_unorm, 2, 1, 1, 1, texture_usage::shader_resource | texture_usage::copy_source | texture_usage::copy_destination}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(copied);
    const auto source{create_texture({texture_format::rgba8_unorm, 2, 1, 1, 1, texture_usage::copy_source | texture_usage::copy_destination}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(source);
    const std::array<std::uint32_t, 2> pixels{0xFF0000FF, 0xFF00FF00};
    ASSERT_TRUE(update(*source, {0, 0, 2, 1}, {std::as_bytes(std::span{pixels}), 8}));
    ASSERT_TRUE(begin_frame());
    const auto target_pass{begin_render_pass(pass(targets_[0]))};
    ASSERT_TRUE(copy(*source, {0, 0, 2, 1}, *copied, {0, 0, 2, 1}));
    const auto pending{readback(*copied, {0, 0, 2, 1}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(pending);
    EXPECT_EQ(poll(*pending).status, readback_status::pending);
    source_ = *copied;
    auto commands{begin_encode(target_pass)};
    write(*commands, false);
    end_encode(commands);
    EXPECT_EQ(poll(*pending).status, readback_status::pending);
    ASSERT_TRUE(end_render_pass(target_pass));
    EXPECT_EQ(poll(*pending).status, readback_status::pending);
    ASSERT_TRUE(end_frame());
    expect_columns(targets_[0], pixels[0], pixels[0]);
    EXPECT_EQ(poll(*pending).status, readback_status::ready);
    destroy(*pending);
    destroy(*source);
}

TEST_F(command_encoder_test, render_then_copy_or_blit_then_render_preserves_order)
{
    const auto rendered{create_texture({texture_format::rgba8_unorm, 4, 4, 1, 1, texture_usage::shader_resource | texture_usage::render_target | texture_usage::copy_source}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto copied{create_texture({texture_format::rgba8_unorm, 4, 4, 1, 1, texture_usage::shader_resource | texture_usage::render_target | texture_usage::copy_destination}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(rendered && copied);
    source_ = *copied;
    for (const bool use_blit : {false, true})
    {
        ASSERT_TRUE(begin_frame());
        auto red{pass(*rendered)};
        red.render_targets[0].begin.clear.color = {1, 0, 0, 1};
        const auto first{begin_render_pass(red)};
        ASSERT_TRUE(end_render_pass(first));
        if (use_blit)
        {
            ASSERT_TRUE(blit(*rendered, {0, 0, 4, 4}, *copied, {0, 0, 4, 4}));
        }
        else
        {
            ASSERT_TRUE(copy(*rendered, {0, 0, 4, 4}, *copied, {0, 0, 4, 4}));
        }
        const auto second{begin_render_pass(pass(targets_[1]))};
        auto commands{begin_encode(second)};
        write(*commands, false);
        end_encode(commands);
        ASSERT_TRUE(end_render_pass(second));
        ASSERT_TRUE(end_frame());
        expect_columns(targets_[1], 0xFF0000FF, 0xFF0000FF);
    }
    destroy(*copied);
    destroy(*rendered);
}

TEST_F(command_encoder_test, frame_readback_captures_render_at_request_position)
{
    ASSERT_TRUE(begin_frame());
    auto red{pass(targets_[0])};
    red.render_targets[0].begin.clear.color = {1, 0, 0, 1};
    const auto first{begin_render_pass(red)};
    ASSERT_TRUE(end_render_pass(first));
    const auto request{readback(targets_[0], {0, 0, 4, 4}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(request);
    EXPECT_EQ(poll(*request).status, readback_status::pending);
    auto green{pass(targets_[0])};
    green.render_targets[0].begin.clear.color = {0, 1, 0, 1};
    const auto second{begin_render_pass(green)};
    ASSERT_TRUE(end_render_pass(second));
    ASSERT_TRUE(end_frame());
    ASSERT_TRUE(submit_and_wait());
    const auto status{poll(*request)};
    ASSERT_EQ(status.status, readback_status::ready);
    for (std::size_t offset{0}; offset < status.value.data.size(); offset += 4)
    {
        std::uint32_t pixel{};
        std::memcpy(&pixel, status.value.data.data() + offset, 4);
        EXPECT_EQ(pixel, 0xFF0000FF);
    }
    expect_columns(targets_[0], 0xFF00FF00, 0xFF00FF00);
    destroy(*request);
}

TEST_F(command_encoder_test, rendering_after_mip_generation_preserves_texture_sampling)
{
    const auto texture{create_texture({texture_format::rgba8_unorm, 4, 4, 3, 1, texture_usage::shader_resource | texture_usage::render_target | texture_usage::copy_source}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(texture);
    source_ = *texture;
    ASSERT_TRUE(begin_frame());
    auto red{pass(*texture)};
    red.render_targets[0].begin.clear.color = {1, 0, 0, 1};
    const auto first{begin_render_pass(red)};
    ASSERT_TRUE(end_render_pass(first));
    ASSERT_TRUE(generate_mips(*texture));
    const auto request{readback(*texture, {0, 0, 1, 1, 2}).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(request);
    const auto second{begin_render_pass(pass(targets_[1]))};
    auto commands{begin_encode(second)};
    write(*commands, false);
    end_encode(commands);
    ASSERT_TRUE(end_render_pass(second));
    ASSERT_TRUE(end_frame());
    ASSERT_TRUE(submit_and_wait());
    const auto status{poll(*request)};
    ASSERT_EQ(status.status, readback_status::ready);
    std::uint32_t pixel{};
    std::memcpy(&pixel, status.value.data.data(), 4);
    EXPECT_EQ(pixel, 0xFF0000FF);
    expect_columns(targets_[1], 0xFF0000FF, 0xFF0000FF);
    destroy(*request);
    destroy(*texture);
}

TEST_F(command_encoder_test, pipeline_switches_preserve_updated_bindings)
{
    const auto vertex{create_shader(std::as_bytes(std::span{sample_vs})).release()};
    const auto pixel_shader{create_shader(std::as_bytes(std::span{sample_ps})).release()};
    ASSERT_TRUE(vertex && pixel_shader);
    std::array bindings{
        shader_binding{shader_binding_type::constant_buffer, 0, shader_stage::pixel},
        shader_binding{shader_binding_type::texture_srv, 0, shader_stage::pixel},
        shader_binding{shader_binding_type::sampler, 0, shader_stage::pixel},
    };
    const std::array formats{texture_format::rgba8_unorm};
    const std::array blends{render_target_blend_desc{false, blend_factor::one, blend_factor::zero, blend_op::add, blend_factor::one, blend_factor::zero, blend_op::add, color_write_mask::all}};
    const graphics_pipeline_desc desc{
        .vertex_shader = vertex,
        .pixel_shader = pixel_shader,
        .bindings = bindings,
        .render_target_formats = formats,
        .topology = primitive_topology::triangle_list,
        .render_target_blends = blends,
        .sample_count = 1,
    };
    const auto first_pipeline{create_graphics_pipeline(desc).release()};
    std::swap(bindings.front(), bindings.back());
    const auto second_pipeline{create_graphics_pipeline(desc).release()};
    ASSERT_TRUE(first_pipeline && second_pipeline);
    const auto source{create_color(2)};
    const auto destination{create_color()};
    ASSERT_TRUE(source && destination);
    const std::array<std::uint32_t, 4> pixels{0xFF0000FF, 0xFF00FF00, 0xFF0000FF, 0xFF00FF00};
    ASSERT_TRUE(update(*source, {0, 0, 2, 2}, {std::as_bytes(std::span{pixels}), 8}));
    const auto sampler{create_sampler({sampler_filter::point, sampler_filter::point, sampler_filter::point, sampler_address_mode::clamp, sampler_address_mode::clamp, sampler_address_mode::clamp, 1, 0, 0}).release()};
    const render_pass_desc pass_desc{{{*destination, {begin_op::clear}, {end_op::store}}}, std::nullopt};
    ASSERT_TRUE(sampler);
    ASSERT_TRUE(begin_frame());
    const std::array<float, 4> red_constants{0, 0, 0, 0};
    const std::array<float, 4> green_constants{1, 0, 0, 0};
    const auto constant_buffer{create_buffer({256, buffer_usage::constant, buffer_access::cpu_write}, std::as_bytes(std::span{green_constants})).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(constant_buffer);
    const auto pass_handle{begin_render_pass(pass_desc)};
    const auto encoder{begin_encode(pass_handle)};
    encoder->set_pipeline(first_pipeline);
    encoder->set_scissor_rect({0, 0, 2, 4});
    encoder->set_constant_buffer(0, std::as_bytes(std::span{red_constants}));
    encoder->set_texture(1, *source);
    encoder->set_sampler(2, sampler);
    encoder->draw(3);
    encoder->draw(3);
    encoder->set_pipeline(second_pipeline);
    encoder->set_scissor_rect({2, 0, 2, 4});
    encoder->set_sampler(0, sampler);
    encoder->set_texture(1, *source);
    encoder->set_constant_buffer(2, *constant_buffer);
    encoder->draw(3);
    end_encode(encoder);
    ASSERT_TRUE(end_render_pass(pass_handle));
    ASSERT_TRUE(end_frame());
    expect_columns(*destination, pixels[0], pixels[1]);
    destroy(*constant_buffer);
    destroy(*destination);
    destroy(*source);
    destroy(sampler);
    destroy(second_pipeline);
    destroy(first_pipeline);
    destroy(pixel_shader);
    destroy(vertex);
}

TEST_F(command_encoder_test, pipeline_accepts_borrowed_semantic_substrings)
{
    const auto vertex{create_shader(std::as_bytes(std::span{quad_vs})).release()};
    const auto pixel{create_shader(std::as_bytes(std::span{quad_ps})).release()};
    const std::array bindings{
        shader_binding{shader_binding_type::constant_buffer, 0, shader_stage::vertex | shader_stage::pixel},
        shader_binding{shader_binding_type::texture_srv, 0, shader_stage::pixel},
        shader_binding{shader_binding_type::sampler, 0, shader_stage::pixel},
    };
    const std::array formats{texture_format::rgba8_unorm};
    const std::array blends{render_target_blend_desc{false, blend_factor::one, blend_factor::zero, blend_op::add, blend_factor::one, blend_factor::zero, blend_op::add, color_write_mask::all}};
    std::optional<pipeline_handle> pipeline;
    {
        const std::string semantics{"POSITION:TEXCOORD:unused"};
        const std::array elements{
            input_element{std::string_view{semantics}.substr(0, 8), 0, input_element_format::float2, 0},
            input_element{std::string_view{semantics}.substr(9, 8), 0, input_element_format::float2, 8},
        };
        const auto created{create_graphics_pipeline({vertex, pixel, bindings, formats, std::nullopt, {16, elements}, primitive_topology::triangle_list, {fill_mode::solid, cull_mode::none, front_face::clockwise}, std::nullopt, std::nullopt, blends, 1}).release()};
        ASSERT_TRUE(created);
        pipeline = created;
    }
    ASSERT_TRUE(pipeline);
    constexpr std::array<float, 16> vertices{0, 0, 0, 0, 4, 0, 1, 0, 0, 4, 0, 1, 4, 4, 1, 1};
    constexpr std::array<std::uint16_t, 6> indices{0, 1, 2, 2, 1, 3};
    const auto vertex_buffer{create_buffer({sizeof(vertices), buffer_usage::vertex, buffer_access::cpu_write}, std::as_bytes(std::span{vertices})).transform([](auto&& owner)
    {
        return owner.release();
    })};
    const auto index_buffer{create_buffer({sizeof(indices), buffer_usage::index, buffer_access::cpu_write}, std::as_bytes(std::span{indices})).transform([](auto&& owner)
    {
        return owner.release();
    })};
    ASSERT_TRUE(vertex_buffer && index_buffer);
    ASSERT_TRUE(begin_frame());
    const auto target_pass{begin_render_pass(pass(targets_[0]))};
    const auto encoder{begin_encode(target_pass)};
    constexpr std::array<float, 8> constants{4, 4, 0, 0, 1, 1, 1, 1};
    encoder->set_pipeline(*pipeline);
    encoder->set_constant_buffer(0, std::as_bytes(std::span{constants}));
    encoder->set_texture(1, source_);
    encoder->set_sampler(2, sampler_);
    encoder->set_vertex_buffer(*vertex_buffer, 16);
    encoder->set_index_buffer(*index_buffer, index_format::uint16);
    encoder->draw_indexed(6);
    end_encode(encoder);
    ASSERT_TRUE(end_render_pass(target_pass));
    ASSERT_TRUE(end_frame());
    expect_columns(targets_[0], 0xFFFFFFFF, 0);
    destroy(*index_buffer);
    destroy(*vertex_buffer);
    destroy(*pipeline);
    destroy(pixel);
    destroy(vertex);
}
} // namespace
