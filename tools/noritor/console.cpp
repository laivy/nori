#include "pch.hpp"

#include "asset_manager.hpp"
#include "console.hpp"
#include "log.hpp"

namespace
{
using nori::editor::log;

const char* level_name(log::level level)
{
    switch (level)
    {
    case log::level::info:
    {
        return "정보";
    }
    case log::level::error:
    {
        return "오류";
    }
    default:
        return "알 수 없음";
    }
}

std::string local_time(std::chrono::system_clock::time_point timestamp)
{
    const auto seconds{std::chrono::floor<std::chrono::seconds>(timestamp)};
    const auto value{std::chrono::current_zone()->to_local(seconds)};
    return std::format("{:%H:%M:%S}", value);
}
} // namespace

namespace nori::editor
{
console::console() :
    filters_{true, true},
    open_{true},
    was_at_bottom_{true},
    scroll_to_bottom_{false},
    displayed_entry_count_{0}
{
}

void console::draw(asset_manager& assets)
{
    if (!ImGui::Begin("콘솔", &open_))
    {
        ImGui::End();
        return;
    }

    for (auto&& [index, filter] : std::views::enumerate(filters_))
    {
        if (index != 0)
        {
            ImGui::SameLine();
        }
        ImGui::Checkbox(level_name(log::level{static_cast<int>(index)}), &filter);
    }
    ImGui::SameLine();

    const ImGuiStyle& style{ImGui::GetStyle()};
    const float clear_button_width{ImGui::GetFrameHeight()};
    const float search_width{std::max(0.0f, std::min(240.0f, ImGui::GetWindowWidth() - style.WindowPadding.x * 2.0f - clear_button_width - style.ItemSpacing.x))};
    const float search_row_width{search_width + style.ItemSpacing.x + clear_button_width};
    ImGui::SetCursorPosX(std::max(style.WindowPadding.x, ImGui::GetWindowWidth() - style.WindowPadding.x - search_row_width));
    ImGui::SetNextItemWidth(search_width);
    ImGui::InputTextWithHint("##console_filter", "필터", &text_filter_);
    ImGui::SameLine();
    if (assets.image_button("##console_clear", "16/delete"))
    {
        log::clear();
        displayed_entry_count_ = 0;
        was_at_bottom_ = true;
        scroll_to_bottom_ = false;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("로그 지우기");
    }
    ImGui::Separator();

    const std::span<const log::entry> entries{log::entries()};
    if (entries.size() != displayed_entry_count_)
    {
        scroll_to_bottom_ = true;
        displayed_entry_count_ = entries.size();
    }
    display_text_.clear();
    for (const log::entry& entry : entries)
    {
        if (!filters_[static_cast<std::size_t>(entry.severity)] || (!text_filter_.empty() && entry.text.find(text_filter_) == std::string::npos))
        {
            continue;
        }
        if (!display_text_.empty())
        {
            display_text_.push_back('\n');
        }
        display_text_.append(std::format("[{}] [{}] {}", local_time(entry.timestamp), level_name(entry.severity), entry.text));
    }

    ImGuiWindow* const window{ImGui::GetCurrentWindow()};
    const ImGuiID input_id{ImGui::GetID("##console_entries")};
    const std::string input_window_name{std::format("{}/##console_entries_{:08x}", window->Name, input_id)};
    ImGuiWindow* const input_window{ImGui::FindWindowByName(input_window_name.c_str())};
    if (input_window)
    {
        was_at_bottom_ = input_window->Scroll.y >= input_window->ScrollMax.y - 1.0f;
    }
    const bool should_scroll{scroll_to_bottom_ && was_at_bottom_};
    if (should_scroll)
    {
        ImGui::SetNextWindowScroll(ImVec2{-1.0f, FLT_MAX});
    }
    ImGui::InputTextMultiline(
        "##console_entries",
        &display_text_,
        ImVec2{-FLT_MIN, -FLT_MIN},
        ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_WordWrap
    );
    scroll_to_bottom_ = false;
    if (input_window)
    {
        was_at_bottom_ = should_scroll || input_window->Scroll.y >= input_window->ScrollMax.y - 1.0f;
    }
    ImGui::End();
}

bool console::is_open() const
{
    return open_;
}

void console::set_open(bool open)
{
    open_ = open;
}
} // namespace nori::editor
