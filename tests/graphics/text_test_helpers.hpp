#pragma once

#include <functional>

#include "graphics_test_helpers.hpp"
#include "test_shader/text_effect_ps.hpp"
#include "test_shader/text_identity_ps.hpp"
#include "test_shader/text_identity_vs.hpp"

namespace
{
class text_test : public initialized_graphics_test
{
};
} // namespace

namespace nori::graphics::test_support
{
std::vector<std::byte> text_font_fixture(std::string_view name);
std::vector<std::byte> draw_layout(nori::graphics::text::layout_handle layout, nori::core::vec4 tint = {1.0f, 1.0f, 1.0f, 1.0f}, const std::function<void()>& before_replay = {}, const std::function<void()>& after_replay = {}, std::optional<nori::graphics::text::effect> effect = std::nullopt, nori::core::vec2 position = {8.0f, 8.0f}, const std::function<void(nori::graphics::render_pass_handle)>& record = {});
} // namespace nori::graphics::test_support
