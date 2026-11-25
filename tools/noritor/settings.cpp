#include "pch.hpp"

#include "settings.hpp"

namespace
{
constexpr ImVec4 to_rgba(std::uint32_t argb)
{
    return ImVec4{
        static_cast<float>((argb >> 16) & 0xFF) / 255.0f,
        static_cast<float>((argb >> 8) & 0xFF) / 255.0f,
        static_cast<float>(argb & 0xFF) / 255.0f,
        static_cast<float>((argb >> 24) & 0xFF) / 255.0f
    };
}

constexpr ImVec4 lerp(ImVec4 a, ImVec4 b, float t)
{
    return ImVec4{std::lerp(a.x, b.x, t), std::lerp(a.y, b.y, t), std::lerp(a.z, b.z, t), std::lerp(a.w, b.w, t)};
}
} // namespace

namespace nori::editor
{
settings::settings() :
    font_size_{18.0f}
{
}

void settings::initialize()
{
    apply_theme();
}

void settings::render_menu()
{
    if (!ImGui::BeginMenu("설정"))
    {
        return;
    }

    const float item_width{ImGui::GetFontSize() * 12.0f};
    ImGui::SetNextItemWidth(item_width);
    ImGui::DragFloat("폰트 크기", &font_size_, 1.0f, 12.0f, 64.0f, "%.0f");
    ImGui::EndMenu();
}

void settings::push()
{
    ImGui::PushFont(nullptr, font_size_);
}

void settings::pop() const
{
    ImGui::PopFont();
}

void settings::apply_theme()
{
    auto& style{ImGui::GetStyle()};
    ImGui::StyleColorsDark();

    style.WindowBorderSize = 3.0f;
    style.FrameRounding = 3.0f;
    style.PopupRounding = 3.0f;
    style.ScrollbarRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.DockingSeparatorSize = 3.0f;

    auto& colors{style.Colors};
    colors[ImGuiCol_Text] = to_rgba(0xFFABB2BF);
    colors[ImGuiCol_TextDisabled] = to_rgba(0xFF565656);
    colors[ImGuiCol_WindowBg] = to_rgba(0xFF282C34);
    colors[ImGuiCol_ChildBg] = to_rgba(0xFF21252B);
    colors[ImGuiCol_PopupBg] = to_rgba(0xFF2E323A);
    colors[ImGuiCol_Border] = to_rgba(0xFF2E323A);
    colors[ImGuiCol_BorderShadow] = to_rgba(0x00000000);
    colors[ImGuiCol_FrameBg] = colors[ImGuiCol_ChildBg];
    colors[ImGuiCol_FrameBgHovered] = to_rgba(0xFF484C52);
    colors[ImGuiCol_FrameBgActive] = to_rgba(0xFF54575D);
    colors[ImGuiCol_TitleBg] = colors[ImGuiCol_WindowBg];
    colors[ImGuiCol_TitleBgActive] = colors[ImGuiCol_FrameBgActive];
    colors[ImGuiCol_TitleBgCollapsed] = to_rgba(0x8221252B);
    colors[ImGuiCol_MenuBarBg] = colors[ImGuiCol_ChildBg];
    colors[ImGuiCol_ScrollbarBg] = colors[ImGuiCol_PopupBg];
    colors[ImGuiCol_ScrollbarGrab] = to_rgba(0xFF3E4249);
    colors[ImGuiCol_ScrollbarGrabHovered] = to_rgba(0xFF484C52);
    colors[ImGuiCol_ScrollbarGrabActive] = to_rgba(0xFF54575D);
    colors[ImGuiCol_CheckMark] = colors[ImGuiCol_Text];
    colors[ImGuiCol_SliderGrab] = to_rgba(0xFF353941);
    colors[ImGuiCol_SliderGrabActive] = to_rgba(0xFF7A7A7A);
    colors[ImGuiCol_Button] = colors[ImGuiCol_SliderGrab];
    colors[ImGuiCol_ButtonHovered] = colors[ImGuiCol_FrameBgActive];
    colors[ImGuiCol_ButtonActive] = colors[ImGuiCol_ScrollbarGrabActive];
    colors[ImGuiCol_Header] = colors[ImGuiCol_ChildBg];
    colors[ImGuiCol_HeaderHovered] = to_rgba(0xFF353941);
    colors[ImGuiCol_HeaderActive] = colors[ImGuiCol_FrameBgActive];
    colors[ImGuiCol_Separator] = colors[ImGuiCol_FrameBgActive];
    colors[ImGuiCol_SeparatorHovered] = to_rgba(0xFF3E4452);
    colors[ImGuiCol_SeparatorActive] = colors[ImGuiCol_SeparatorHovered];
    colors[ImGuiCol_ResizeGrip] = colors[ImGuiCol_Separator];
    colors[ImGuiCol_ResizeGripHovered] = colors[ImGuiCol_SeparatorHovered];
    colors[ImGuiCol_ResizeGripActive] = colors[ImGuiCol_SeparatorActive];
    colors[ImGuiCol_InputTextCursor] = to_rgba(0xFF528BFF);
    colors[ImGuiCol_TabHovered] = colors[ImGuiCol_HeaderHovered];
    colors[ImGuiCol_Tab] = colors[ImGuiCol_FrameBgActive];
    colors[ImGuiCol_TabSelected] = colors[ImGuiCol_HeaderHovered];
    colors[ImGuiCol_TabSelectedOverline] = colors[ImGuiCol_HeaderActive];
    colors[ImGuiCol_TabDimmed] = lerp(colors[ImGuiCol_Tab], colors[ImGuiCol_TitleBg], 0.80f);
    colors[ImGuiCol_TabDimmedSelected] = lerp(colors[ImGuiCol_TabSelected], colors[ImGuiCol_TitleBg], 0.40f);
    colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4{0.50f, 0.50f, 0.50f, 0.00f};
    colors[ImGuiCol_DockingPreview] = colors[ImGuiCol_ChildBg];
    colors[ImGuiCol_DockingEmptyBg] = colors[ImGuiCol_WindowBg];
    colors[ImGuiCol_PlotLines] = ImVec4{0.61f, 0.61f, 0.61f, 1.00f};
    colors[ImGuiCol_PlotLinesHovered] = ImVec4{1.00f, 0.43f, 0.35f, 1.00f};
    colors[ImGuiCol_PlotHistogram] = ImVec4{0.90f, 0.70f, 0.00f, 1.00f};
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4{1.00f, 0.60f, 0.00f, 1.00f};
    colors[ImGuiCol_TableHeaderBg] = colors[ImGuiCol_ChildBg];
    colors[ImGuiCol_TableBorderStrong] = colors[ImGuiCol_SliderGrab];
    colors[ImGuiCol_TableBorderLight] = colors[ImGuiCol_FrameBgActive];
    colors[ImGuiCol_TableRowBg] = ImVec4{0.00f, 0.00f, 0.00f, 0.00f};
    colors[ImGuiCol_TableRowBgAlt] = ImVec4{1.00f, 1.00f, 1.00f, 0.06f};
    colors[ImGuiCol_TextLink] = to_rgba(0xFF3F94CE);
    colors[ImGuiCol_TextSelectedBg] = to_rgba(0xFF243140);
    colors[ImGuiCol_TreeLines] = colors[ImGuiCol_Text];
    colors[ImGuiCol_DragDropTarget] = colors[ImGuiCol_Text];
    colors[ImGuiCol_NavCursor] = colors[ImGuiCol_TextLink];
    colors[ImGuiCol_NavWindowingHighlight] = colors[ImGuiCol_Text];
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4{0.80f, 0.80f, 0.80f, 0.20f};
    colors[ImGuiCol_ModalWindowDimBg] = to_rgba(0xC821252B);
}
} // namespace nori::editor
