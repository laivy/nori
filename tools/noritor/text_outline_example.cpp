#include "pch.hpp"

#include <nori/graphics/imgui.hpp>
#include <nori/graphics/operations.hpp>
#include <nori/resource/operations.hpp>

#include "text_outline_example.hpp"

namespace nori::editor
{
text_outline_example::text_outline_example() :
    open_{false},
    dirty_{true},
    font_size_{32.0f},
    thickness_{2.0f},
    outline_color_{0.02f, 0.02f, 0.04f, 1.0f},
    background_color_{0.35f, 0.45f, 0.55f, 1.0f}
{
}

bool text_outline_example::render()
{
    if (!open_ || !error_.empty())
    {
        return true;
    }
    if (!prepare())
    {
        return true;
    }
    const nori::graphics::render_pass_desc desc{
        .render_targets = {{texture_.get(), {.type = nori::graphics::begin_op::clear, .clear = {.color = {background_color_[0], background_color_[1], background_color_[2], 1.0f}}}, {.type = nori::graphics::end_op::store}}}
    };
    const auto pass{nori::graphics::begin_render_pass(desc)};
    if (!pass)
    {
        return false;
    }
    const auto commands{nori::graphics::begin_encode(pass)};
    if (!commands)
    {
        nori::graphics::end_render_pass(pass);
        return false;
    }
    for (std::size_t i{}; i < layouts_.size(); ++i)
    {
        commands->draw_text({layouts_[i].get(), {24.0f, 18.0f + static_cast<float>(i) * 80.0f}, {1.0f, 1.0f, 1.0f, 1.0f}});
    }
    nori::graphics::end_encode(commands);
    return nori::graphics::end_render_pass(pass);
}

void text_outline_example::draw()
{
    if (!open_)
    {
        return;
    }
    ImGui::SetNextWindowSize({1010.0f, 600.0f}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("텍스트 외곽선 예제", &open_))
    {
        ImGui::TextUnformatted("글꼴: Galmuri11");
        dirty_ |= ImGui::SliderFloat("글자 크기", &font_size_, 16.0f, 48.0f, "%.1f px");
        dirty_ |= ImGui::SliderFloat("외곽선 두께", &thickness_, 0.0f, 6.0f, "%.2f px");
        dirty_ |= ImGui::ColorEdit4("외곽선 색상", outline_color_.data());
        ImGui::ColorEdit3("배경 색상", background_color_.data());
        ImGui::TextUnformatted("위에서부터: 외곽선 없음 / 기본 외곽선 / span별 상속·재정의·끄기 / 작은 자막 (18 px)");
        if (!error_.empty())
        {
            ImGui::TextWrapped("%s", error_.c_str());
            if (ImGui::Button("다시 시도"))
            {
                error_.clear();
                dirty_ = true;
            }
        }
        else if (texture_)
        {
            if (ImGui::BeginChild("outline_preview", {}, ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
            {
                ImGui::Image(nori::graphics::imgui::texture_id(texture_.get()), {960.0f, 336.0f});
            }
            ImGui::EndChild();
        }
    }
    ImGui::End();
}

void text_outline_example::release()
{
    for (auto& layout : layouts_)
    {
        layout.reset();
    }
    texture_.reset();
    font_.reset();
    dirty_ = true;
}

bool text_outline_example::is_open() const
{
    return open_;
}

void text_outline_example::set_open(bool open)
{
    open_ = open;
}

bool text_outline_example::prepare()
{
    if (!font_)
    {
        const auto source{nori::resource::try_get("text_outline.nori::galmuri11")};
        if (!source)
        {
            error_ = std::format("예제 폰트 로드 실패: {}", nori::resource::to_string(source.error()));
            return false;
        }
        auto res{nori::graphics::text::create_font(*source)};
        if (!res)
        {
            error_ = std::format("폰트 생성 실패: {}", nori::graphics::to_string(res.error()));
            return false;
        }
        font_ = std::move(*res);
    }
    if (!texture_)
    {
        auto res{nori::graphics::create_texture({.format = nori::graphics::texture_format::rgba8_unorm, .width = 960, .height = 336, .mip_levels = 1, .sample_count = 1, .usage = nori::graphics::texture_usage::render_target | nori::graphics::texture_usage::shader_resource})};
        if (!res)
        {
            error_ = std::format("미리보기 생성 실패: {}", nori::graphics::to_string(res.error()));
            return false;
        }
        texture_ = std::move(*res);
    }
    if (!dirty_)
    {
        return true;
    }
    nori::graphics::text::layout_desc desc{.font = font_.get(), .font_size = font_size_, .color = {1.0f, 1.0f, 1.0f, 1.0f}};
    const nori::graphics::text::outline_desc outline{thickness_, {outline_color_[0], outline_color_[1], outline_color_[2], outline_color_[3]}};
    const std::array<nori::graphics::text::item, 3> spans{
        nori::graphics::text::span{"상속 Inherit   ", {}},
        nori::graphics::text::span{"재정의 Override   ", {.outline = nori::graphics::text::outline_desc{3.5f, {0.9f, 0.2f, 0.1f, 1.0f}}}},
        nori::graphics::text::span{"끄기 Off", {.outline = nori::graphics::text::outline_desc{}}},
    };
    std::array<nori::graphics::text::unique_layout_handle, 4> prepared;
    for (std::size_t i{}; i < prepared.size(); ++i)
    {
        desc.outline = i == 0 ? std::nullopt : std::optional{outline};
        desc.font_size = i == 3 ? 18.0f : font_size_;
        auto res{i == 2 ? nori::graphics::text::create_layout(desc, spans) : nori::graphics::text::create_layout(desc, i == 3 ? "작은 자막도 또렷하게 — Small subtitles 0123456789" : "텍스트 외곽선 — Nori 0123456789")};
        if (!res)
        {
            error_ = std::format("텍스트 생성 실패: {}", nori::graphics::to_string(res.error()));
            return false;
        }
        prepared[i] = std::move(*res);
    }
    layouts_ = std::move(prepared);
    dirty_ = false;
    return true;
}
}
