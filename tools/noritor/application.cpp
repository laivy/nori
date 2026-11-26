#include "pch.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <dbt.h>
#include <windowsx.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <nori/core/encoding.hpp>
#include <nori/core/vector.hpp>
#include <nori/graphics/imgui.hpp>
#include <nori/graphics/operations.hpp>
#include <nori/resource/error_code.hpp>
#include <nori/resource/operations.hpp>
#include <nori/resource/path.hpp>

#include "application.hpp"
#include "application_workflow.hpp"
#include "context.hpp"
#include "explorer.hpp"
#include "file_dialog.hpp"
#include "hierarchy.hpp"
#include "import_workflow.hpp"
#include "inspector.hpp"
#include "log.hpp"
#include "utils.hpp"
#include "workflow_view.hpp"

ImGuiKey ImGui_ImplWin32_KeyEventToImGuiKey(WPARAM w_param, LPARAM l_param);

namespace
{
constexpr nori::core::vec4 editor_clear_color{0.15625f, 0.171875f, 0.203125f, 1.0f};
constexpr float title_bar_vertical_padding{6.0f};
constexpr float window_button_width_ratio{1.45f};
constexpr std::size_t max_worker_count{4};

std::size_t worker_count()
{
    const std::size_t hardware_count{static_cast<std::size_t>(std::thread::hardware_concurrency())};
    return hardware_count > 1 ? std::min(hardware_count - 1, max_worker_count) : 1;
}
} // namespace

namespace nori::editor
{
application::application(const specification& spec) :
    nori::core::windows_application{spec},
    main_swap_chain_{},
    main_context_{nori::io::create_context()},
    worker_context_{nori::io::create_context()},
    mouse_cursor_{::LoadCursorW(nullptr, IDC_ARROW)},
    title_drag_rect_{},
    minimize_button_rect_{},
    maximize_button_rect_{},
    close_button_rect_{},
    hovered_window_button_{window_button::none},
    pressed_window_button_{window_button::none},
    document_dock_space_{},
    explorer_dock_space_{}
{
}

application::~application()
{
    shutdown_io();
}

void application::on_start()
{
    initialize_custom_title_bar();
    if (!nori::resource::initialize("data"))
    {
        quit(1);
        return;
    }
    if (auto res{nori::graphics::initialize()}; !res)
    {
        quit(1);
        return;
    }
    if (auto res{nori::graphics::imgui::initialize(hwnd())}; !res)
    {
        quit(1);
        return;
    }
    auto swap_chain{nori::graphics::create_swap_chain({
        .hwnd = hwnd(),
        .vsync = true,
    })};
    if (!swap_chain)
    {
        quit(1);
        return;
    }
    main_swap_chain_ = std::move(*swap_chain);

    ImGuiIO& io{ImGui::GetIO()};
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
    io.IniFilename = nullptr;
    context_ = std::make_unique<context>();
    context_->preferences().initialize();
    context_->assets().initialize();
    explorer_ = std::make_unique<explorer>(context_.get(), &context_->assets());
    importer_ = std::make_unique<import_workflow>(context_.get(), main_context_.get(), worker_context_.get());
    inspector_ = std::make_unique<inspector>(context_.get(), &file_dialog_, &context_->assets(), importer_.get());
    hierarchy_ = std::make_unique<hierarchy>(context_.get(), &file_dialog_, &context_->assets(), importer_.get());
    workflow_ = std::make_unique<application_workflow>(context_.get(), &file_dialog_, &preview_);
    workflow_view_ = std::make_unique<workflow_view>(workflow_.get());

    const std::size_t count{worker_count()};
    worker_threads_.reserve(count);
    for (std::size_t i{0}; i < count; ++i)
    {
        worker_threads_.emplace_back([this]()
        {
            nori::io::run(worker_context_.get());
        });
    }
}

void application::on_stop()
{
    shutdown_io();

    if (context_)
    {
        for (document_editor& document : context_->documents().documents())
        {
            preview_.release(context_->workspace(document.root()));
            context_->remove_workspace(document.root());
        }
        context_->documents().close_all();
    }
    workflow_view_.reset();
    workflow_.reset();
    hierarchy_.reset();
    inspector_.reset();
    importer_.reset();
    explorer_.reset();
    if (context_)
    {
        context_->assets().shutdown();
    }
    context_.reset();
    text_outline_example_.release();
    nori::graphics::imgui::shutdown();
    main_swap_chain_.reset();
    nori::graphics::shutdown();
    nori::resource::shutdown();
}

void application::on_update(float)
{
    process_input_messages();

    // 실행 스레드에서 생성된 ImGui platform window의 메시지 처리
    MSG message{};
    for (std::uint32_t count{0}; count < 128 && ::PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE); ++count)
    {
        if (message.message == WM_QUIT)
        {
            quit(0);
            break;
        }
        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }
    workflow_->process_pending_closes();
    workflow_->close_exit_documents();
    workflow_->advance_exit();
    if (workflow_->requested() && context_->documents().documents().empty())
    {
        quit(0);
        return;
    }

    nori::io::poll(main_context_.get());
    update_previews();
}

void application::on_render()
{
    LPCWSTR cursor_id{IDC_ARROW};
    switch (ImGui::GetMouseCursor())
    {
    case ImGuiMouseCursor_None:
    {
        cursor_id = nullptr;
        break;
    }
    case ImGuiMouseCursor_TextInput:
    {
        cursor_id = IDC_IBEAM;
        break;
    }
    case ImGuiMouseCursor_ResizeAll:
    {
        cursor_id = IDC_SIZEALL;
        break;
    }
    case ImGuiMouseCursor_ResizeNS:
    {
        cursor_id = IDC_SIZENS;
        break;
    }
    case ImGuiMouseCursor_ResizeEW:
    {
        cursor_id = IDC_SIZEWE;
        break;
    }
    case ImGuiMouseCursor_ResizeNESW:
    {
        cursor_id = IDC_SIZENESW;
        break;
    }
    case ImGuiMouseCursor_ResizeNWSE:
    {
        cursor_id = IDC_SIZENWSE;
        break;
    }
    case ImGuiMouseCursor_Hand:
    {
        cursor_id = IDC_HAND;
        break;
    }
    case ImGuiMouseCursor_NotAllowed:
    {
        cursor_id = IDC_NO;
        break;
    }
    default:
        break;
    }
    mouse_cursor_.store(cursor_id ? ::LoadCursorW(nullptr, cursor_id) : nullptr);
    if (!render_frame())
    {
        quit(1);
    }
}

void application::on_window_resize(int width, int height)
{
    if (width <= 0 || height <= 0)
    {
        return;
    }
    if (!nori::graphics::resize(main_swap_chain_.get(), static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)))
    {
        quit(1);
    }
}

void application::on_window_close()
{
    workflow_->request_exit();
}

void application::on_text_input(char32_t character)
{
    ImGui::GetIO().AddInputCharacter(static_cast<unsigned int>(character));
}

LRESULT application::on_window_message(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param)
{
    switch (message)
    {
    case WM_SETCURSOR:
    {
        if (LOWORD(l_param) == HTCLIENT)
        {
            ::SetCursor(mouse_cursor_.load());
            return TRUE;
        }
        break;
    }
    case WM_NCCALCSIZE:
    {
        if (w_param == TRUE)
        {
            const auto params{reinterpret_cast<NCCALCSIZE_PARAMS*>(l_param)};
            if (::IsZoomed(hwnd))
            {
                MONITORINFO monitor_info{.cbSize = sizeof(MONITORINFO)};
                const HMONITOR monitor{::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST)};
                if (::GetMonitorInfoW(monitor, &monitor_info))
                {
                    params->rgrc[0] = monitor_info.rcWork;
                }
            }
            return 0;
        }
        break;
    }
    case WM_NCHITTEST:
    {
        return title_bar_hit_test(hwnd, l_param);
    }
    case WM_NCMOUSEMOVE:
    {
        const window_button button = [w_param]()
        {
            switch (w_param)
            {
            case HTMINBUTTON:
            {
                return window_button::minimize;
            }
            case HTMAXBUTTON:
            {
                return window_button::maximize;
            }
            case HTCLOSE:
            {
                return window_button::close;
            }
            default:
                return window_button::none;
            }
        }();
        hovered_window_button_ = button;
        TRACKMOUSEEVENT event{
            .cbSize = sizeof(TRACKMOUSEEVENT),
            .dwFlags = TME_LEAVE | TME_NONCLIENT,
            .hwndTrack = hwnd
        };
        ::TrackMouseEvent(&event);
        return 0;
    }
    case WM_NCMOUSELEAVE:
    {
        hovered_window_button_ = window_button::none;
        pressed_window_button_ = window_button::none;
        return 0;
    }
    case WM_NCLBUTTONDOWN:
    {
        if (w_param == HTMINBUTTON || w_param == HTMAXBUTTON || w_param == HTCLOSE)
        {
            pressed_window_button_ = hovered_window_button_.load();
            return 0;
        }
        break;
    }
    case WM_NCLBUTTONUP:
    {
        const window_button button{pressed_window_button_};
        pressed_window_button_ = window_button::none;
        if ((button == window_button::minimize && w_param == HTMINBUTTON) ||
            (button == window_button::maximize && w_param == HTMAXBUTTON) ||
            (button == window_button::close && w_param == HTCLOSE))
        {
            execute_window_command(button);
            return 0;
        }
        break;
    }
    default:
        break;
    }

    switch (message)
    {
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_INPUTLANGCHANGE:
    case WM_DISPLAYCHANGE:
    case WM_MOUSEMOVE:
    case WM_MOUSELEAVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
    {
        const std::lock_guard lock{input_mutex_};
        input_messages_.push_back({hwnd, message, w_param, l_param});
        break;
    }
    case WM_DEVICECHANGE:
    {
        // 포인터를 포함할 수 있는 장치 알림의 비동기 전달 금지
        if (w_param == DBT_DEVNODES_CHANGED)
        {
            const std::lock_guard lock{input_mutex_};
            input_messages_.push_back({hwnd, message, w_param, 0});
        }
        break;
    }
    default:
        break;
    }

    return nori::core::windows_application::on_window_message(hwnd, message, w_param, l_param);
}

void application::process_input_messages()
{
    std::deque<input_message> messages;
    {
        const std::lock_guard lock{input_mutex_};
        const std::size_t count{std::min(input_messages_.size(), std::size_t{1024})};
        for (std::size_t i{0}; i < count; ++i)
        {
            messages.push_back(input_messages_.front());
            input_messages_.pop_front();
        }
    }
    for (const input_message& message : messages)
    {
        process_input_message(message.hwnd, message.message, message.w_param, message.l_param);
    }
}

void application::process_input_message(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param)
{
    if (message == WM_KILLFOCUS)
    {
        input_keys_.fill(false);
    }
    if (message == WM_KEYDOWN || message == WM_KEYUP || message == WM_SYSKEYDOWN || message == WM_SYSKEYUP)
    {
        ImGuiIO& io{ImGui::GetIO()};
        const bool down{message == WM_KEYDOWN || message == WM_SYSKEYDOWN};
        if (w_param == VK_SHIFT)
        {
            w_param = (HIWORD(l_param) & 0xFF) == 0x36 ? VK_RSHIFT : VK_LSHIFT;
        }
        else if (w_param == VK_CONTROL)
        {
            w_param = (HIWORD(l_param) & KF_EXTENDED) != 0 ? VK_RCONTROL : VK_LCONTROL;
        }
        else if (w_param == VK_MENU)
        {
            w_param = (HIWORD(l_param) & KF_EXTENDED) != 0 ? VK_RMENU : VK_LMENU;
        }
        if (w_param < input_keys_.size())
        {
            input_keys_[w_param] = down;
        }
        const ImGuiKey key{::ImGui_ImplWin32_KeyEventToImGuiKey(w_param, l_param)};
        if (key != ImGuiKey_None)
        {
            io.AddKeyEvent(key, down);
        }
        io.AddKeyEvent(ImGuiMod_Ctrl, input_keys_[VK_LCONTROL] || input_keys_[VK_RCONTROL]);
        io.AddKeyEvent(ImGuiMod_Shift, input_keys_[VK_LSHIFT] || input_keys_[VK_RSHIFT]);
        io.AddKeyEvent(ImGuiMod_Alt, input_keys_[VK_LMENU] || input_keys_[VK_RMENU]);
        return;
    }
    nori::graphics::imgui::process_window_message(hwnd, message, w_param, l_param);
}

void application::initialize_custom_title_bar()
{
    const LONG_PTR style{::GetWindowLongPtrW(hwnd(), GWL_STYLE)};
    ::SetWindowLongPtrW(hwnd(), GWL_STYLE, style & ~static_cast<LONG_PTR>(WS_CAPTION));
    ::SetWindowPos(hwnd(), nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

LRESULT application::title_bar_hit_test(HWND hwnd, LPARAM l_param)
{
    RECT title_drag_rect;
    RECT minimize_button_rect;
    RECT maximize_button_rect;
    RECT close_button_rect;
    {
        const std::lock_guard lock{title_bar_mutex_};
        title_drag_rect = title_drag_rect_;
        minimize_button_rect = minimize_button_rect_;
        maximize_button_rect = maximize_button_rect_;
        close_button_rect = close_button_rect_;
    }
    const POINT point{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
    if (!::IsZoomed(hwnd))
    {
        RECT window_rect{};
        ::GetWindowRect(hwnd, &window_rect);
        const int frame_x{::GetSystemMetrics(SM_CXSIZEFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER)};
        const int frame_y{::GetSystemMetrics(SM_CYSIZEFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER)};
        const bool left{point.x >= window_rect.left && point.x < window_rect.left + frame_x};
        const bool right{point.x < window_rect.right && point.x >= window_rect.right - frame_x};
        const bool top{point.y >= window_rect.top && point.y < window_rect.top + frame_y};
        const bool bottom{point.y < window_rect.bottom && point.y >= window_rect.bottom - frame_y};
        if (top && left)
        {
            return HTTOPLEFT;
        }
        if (top && right)
        {
            return HTTOPRIGHT;
        }
        if (bottom && left)
        {
            return HTBOTTOMLEFT;
        }
        if (bottom && right)
        {
            return HTBOTTOMRIGHT;
        }
        if (left)
        {
            return HTLEFT;
        }
        if (right)
        {
            return HTRIGHT;
        }
        if (top)
        {
            return HTTOP;
        }
        if (bottom)
        {
            return HTBOTTOM;
        }
    }

    const auto contains = [point](RECT rect)
    {
        return point.x >= rect.left && point.x < rect.right && point.y >= rect.top && point.y < rect.bottom;
    };
    if (contains(close_button_rect))
    {
        hovered_window_button_ = window_button::close;
        return HTCLOSE;
    }
    if (contains(maximize_button_rect))
    {
        hovered_window_button_ = window_button::maximize;
        return HTMAXBUTTON;
    }
    if (contains(minimize_button_rect))
    {
        hovered_window_button_ = window_button::minimize;
        return HTMINBUTTON;
    }
    hovered_window_button_ = window_button::none;
    if (contains(title_drag_rect))
    {
        return HTCAPTION;
    }
    return HTCLIENT;
}

void application::execute_window_command(window_button button)
{
    WPARAM command{};
    switch (button)
    {
    case window_button::minimize:
    {
        command = SC_MINIMIZE;
        break;
    }
    case window_button::maximize:
    {
        command = ::IsZoomed(hwnd()) ? SC_RESTORE : SC_MAXIMIZE;
        break;
    }
    case window_button::close:
    {
        command = SC_CLOSE;
        break;
    }
    case window_button::none:
    {
        return;
    }
    default:
        return;
    }
    ::PostMessageW(hwnd(), WM_SYSCOMMAND, command, 0);
}

void application::draw_window_button(window_button button, RECT rect) const
{
    ImDrawList* const draw_list{ImGui::GetForegroundDrawList(ImGui::GetMainViewport())};
    const ImVec2 min{static_cast<float>(rect.left), static_cast<float>(rect.top)};
    const ImVec2 max{static_cast<float>(rect.right), static_cast<float>(rect.bottom)};
    if (hovered_window_button_ == button)
    {
        const ImU32 color{button == window_button::close ? IM_COL32(196, 43, 28, 255) : ImGui::GetColorU32(ImGuiCol_ButtonHovered)};
        draw_list->AddRectFilled(min, max, color);
    }
    if (pressed_window_button_ == button)
    {
        draw_list->AddRectFilled(min, max, ImGui::GetColorU32(ImGuiCol_ButtonActive));
    }

    const float center_x{(min.x + max.x) * 0.5f};
    const float center_y{(min.y + max.y) * 0.5f};
    const float glyph_size{std::max((max.y - min.y) * 0.36f, 8.0f)};
    const float half{glyph_size * 0.5f};
    const ImU32 color{ImGui::GetColorU32(ImGuiCol_Text)};
    switch (button)
    {
    case window_button::minimize:
    {
        draw_list->AddLine(ImVec2{center_x - half, center_y + half * 0.5f}, ImVec2{center_x + half, center_y + half * 0.5f}, color);
        break;
    }
    case window_button::maximize:
    {
        if (::IsZoomed(hwnd()))
        {
            draw_list->AddRect(ImVec2{center_x - half * 0.5f, center_y - half}, ImVec2{center_x + half, center_y + half * 0.5f}, color);
            draw_list->AddRect(ImVec2{center_x - half, center_y - half * 0.5f}, ImVec2{center_x + half * 0.5f, center_y + half}, color);
        }
        else
        {
            draw_list->AddRect(ImVec2{center_x - half, center_y - half}, ImVec2{center_x + half, center_y + half}, color);
        }
        break;
    }
    case window_button::close:
    {
        draw_list->AddLine(ImVec2{center_x - half, center_y - half}, ImVec2{center_x + half, center_y + half}, color);
        draw_list->AddLine(ImVec2{center_x + half, center_y - half}, ImVec2{center_x - half, center_y + half}, color);
        break;
    }
    default:
        break;
    }
}

void application::update_previews()
{
    for (document_editor& document : context_->documents().documents())
    {
        if (document.closing())
        {
            continue;
        }
        if (const auto res{preview_.update(document, context_->workspace(document.root()))}; !res)
        {
            log::error(std::format("미리보기 갱신 실패: {}", nori::graphics::to_string(res.error())));
        }
    }
}

bool application::render_frame()
{
    if (!nori::graphics::begin_frame())
    {
        return false;
    }
    if (!text_outline_example_.render())
    {
        return false;
    }
    constexpr nori::graphics::begin_op_desc begin{
        .type = nori::graphics::begin_op::clear,
        .clear = {.color = editor_clear_color}
    };
    constexpr nori::graphics::end_op_desc end{
        .type = nori::graphics::end_op::store
    };
    const nori::graphics::render_pass_desc pass{
        .render_targets = {{nori::graphics::back_buffer(main_swap_chain_.get()), begin, end}}
    };
    if (!nori::graphics::imgui::begin_render_pass(pass))
    {
        return false;
    }
    draw_ui();
    if (!nori::graphics::imgui::end_render_pass() ||
        !nori::graphics::end_frame())
    {
        return false;
    }
    return nori::graphics::present(main_swap_chain_.get());
}

void application::draw_ui()
{
    context_->preferences().push();
    draw_main_menu_bar();
    draw_status_bar();
    draw_main_dock_space();
    draw_document_workspaces();
    draw_explorer();
    draw_file_drop_target();
    draw_console();
    text_outline_example_.draw();
    workflow_view_->draw_document_dialog();
    hierarchy_->draw_dialog();
    inspector_->draw_dialog();
    context_->preferences().pop();
    process_shortcuts();
}

void application::draw_main_menu_bar()
{
    const ImVec2 frame_padding{ImGui::GetStyle().FramePadding};
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{frame_padding.x, title_bar_vertical_padding});
    if (!ImGui::BeginMainMenuBar())
    {
        ImGui::PopStyleVar();
        return;
    }
    if (workflow_->closing())
    {
        ImGui::BeginDisabled();
    }
    if (ImGui::BeginMenu("파일"))
    {
        if (ImGui::MenuItem("새 파일", "Ctrl+N"))
        {
            workflow_->create_document();
        }
        if (ImGui::MenuItem("열기...", "Ctrl+O"))
        {
            workflow_->open_document_dialog();
        }
        if (ImGui::MenuItem("저장", "Ctrl+S", false, context_->documents().active() != nullptr))
        {
            workflow_->save_active_document();
        }
        if (ImGui::MenuItem("다른 이름으로 저장...", nullptr, false, context_->documents().active() != nullptr))
        {
            workflow_->open_save_as_dialog();
        }
        if (ImGui::MenuItem("모두 저장", "Ctrl+Shift+S", false, !context_->documents().documents().empty()))
        {
            workflow_->save_all_documents();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("종료"))
        {
            workflow_->request_exit();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("편집"))
    {
        const document_editor* const active_document{context_->documents().active()};
        const bool can_undo{active_document && !active_document->closing() && active_document->can_undo()};
        const bool can_redo{active_document && !active_document->closing() && active_document->can_redo()};
        if (ImGui::MenuItem("실행 취소", "Ctrl+Z", false, can_undo))
        {
            undo();
        }
        if (ImGui::MenuItem("다시 실행", "Ctrl+Y", false, can_redo))
        {
            redo();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("창"))
    {
        bool console_open{console_.is_open()};
        if (ImGui::MenuItem("콘솔", nullptr, &console_open))
        {
            console_.set_open(console_open);
        }
        bool outline_example_open{text_outline_example_.is_open()};
        if (ImGui::MenuItem("텍스트 외곽선 예제", nullptr, &outline_example_open))
        {
            text_outline_example_.set_open(outline_example_open);
        }
        ImGui::EndMenu();
    }
    context_->preferences().render_menu();
    if (workflow_->closing())
    {
        ImGui::EndDisabled();
    }

    draw_title_bar();
    ImGui::EndMainMenuBar();
    ImGui::PopStyleVar();
}

void application::draw_title_bar()
{
    const ImVec2 bar_min{ImGui::GetWindowPos()};
    const ImVec2 bar_max{bar_min + ImGui::GetWindowSize()};
    const float button_width{(bar_max.y - bar_min.y) * window_button_width_ratio};
    const float buttons_left{bar_max.x - button_width * 3.0f};
    const float drag_left{ImGui::GetCursorScreenPos().x + ImGui::GetStyle().ItemSpacing.x};
    const RECT title_drag_rect{
        static_cast<LONG>(drag_left),
        static_cast<LONG>(bar_min.y),
        static_cast<LONG>(buttons_left),
        static_cast<LONG>(bar_max.y)
    };
    const RECT minimize_button_rect{
        static_cast<LONG>(buttons_left),
        static_cast<LONG>(bar_min.y),
        static_cast<LONG>(buttons_left + button_width),
        static_cast<LONG>(bar_max.y)
    };
    const RECT maximize_button_rect{
        minimize_button_rect.right,
        static_cast<LONG>(bar_min.y),
        static_cast<LONG>(buttons_left + button_width * 2.0f),
        static_cast<LONG>(bar_max.y)
    };
    const RECT close_button_rect{
        maximize_button_rect.right,
        static_cast<LONG>(bar_min.y),
        static_cast<LONG>(bar_max.x),
        static_cast<LONG>(bar_max.y)
    };

    {
        const std::lock_guard lock{title_bar_mutex_};
        title_drag_rect_ = title_drag_rect;
        minimize_button_rect_ = minimize_button_rect;
        maximize_button_rect_ = maximize_button_rect;
        close_button_rect_ = close_button_rect;
    }
    ImDrawList* const draw_list{ImGui::GetWindowDrawList()};
    constexpr std::string_view title{"Noritor"};
    const ImVec2 title_size{ImGui::CalcTextSize(title.data(), title.data() + title.size())};
    const float title_x{std::max((bar_min.x + bar_max.x - title_size.x) * 0.5f, static_cast<float>(title_drag_rect.left))};
    const float title_y{bar_min.y + (bar_max.y - bar_min.y - title_size.y) * 0.5f};
    if (title_x + title_size.x < static_cast<float>(title_drag_rect.right))
    {
        draw_list->AddText(ImVec2{title_x, title_y}, ImGui::GetColorU32(ImGuiCol_Text), title.data(), title.data() + title.size());
    }
    draw_window_button(window_button::minimize, minimize_button_rect);
    draw_window_button(window_button::maximize, maximize_button_rect);
    draw_window_button(window_button::close, close_button_rect);
}

void application::draw_status_bar()
{
    ImGuiViewport* const viewport{ImGui::GetMainViewport()};
    const ImGuiStyle& style{ImGui::GetStyle()};
    const ImVec2 padding{style.WindowPadding.x, style.FramePadding.y};
    const float height{ImGui::GetTextLineHeight() + padding.y * 2.0f};
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
    const bool visible{ImGui::BeginViewportSideBar(
        "##status_bar",
        viewport,
        ImGuiDir_Down,
        height,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav
    )};
    if (visible)
    {
        ImGui::Text("문서 %zu개", context_->documents().documents().size());
        if (!context_->status().empty())
        {
            ImGui::SameLine();
            ImGui::TextDisabled("%.*s", static_cast<int>(context_->status().size()), context_->status().data());
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void application::draw_main_dock_space()
{
    const ImGuiViewport* const viewport{ImGui::GetMainViewport()};
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    constexpr const char* main_dock_space_name{"main_dock_space"};
    const ImGuiID main_dock_space{ImGui::GetID(main_dock_space_name)};

    constexpr ImGuiWindowFlags flags{
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoDecoration
    };
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{});
    ImGui::Begin(main_dock_space_name, nullptr, flags);
    ImGui::PopStyleVar();
    ImGui::DockSpace(main_dock_space, ImVec2{}, ImGuiDockNodeFlags_NoWindowMenuButton | ImGuiDockNodeFlags_NoCloseButton);

    if (!explorer_dock_space_ || !document_dock_space_)
    {
        ImGui::DockBuilderSplitNode(main_dock_space, ImGuiDir_Down, 0.4f, &explorer_dock_space_, &document_dock_space_);
        ImGui::DockBuilderDockWindow("탐색기", explorer_dock_space_);
        ImGui::DockBuilderDockWindow("콘솔", explorer_dock_space_);
        ImGui::DockBuilderFinish(main_dock_space);
    }

    ImGui::End();
}

void application::draw_document_workspaces()
{
    for (document_editor& document : context_->documents().documents())
    {
        draw_document(document);
    }
    workflow_->process_close_request();
}

void application::draw_document(document_editor& doc)
{
    std::string title{nori::core::path_to_utf8(doc.path().filename())};
    if (doc.dirty())
    {
        title.append(" *");
    }
    const nori::resource::handle root{doc.root()};
    title.append(std::format("##document_{}_{}_{}_{}", root.hier.index, root.hier.generation, root.node.index, root.node.generation));

    if (document_dock_space_)
    {
        ImGui::SetNextWindowDockID(document_dock_space_, ImGuiCond_FirstUseEver);
    }
    bool is_open{true};
    const bool visible{ImGui::Begin(title.c_str(), &is_open)};
    if (ImGui::IsWindowDocked())
    {
        const std::string path{nori::core::path_to_utf8(doc.path())};
        ImGui::SetItemTooltip("%s", path.c_str());
    }
    if (!visible)
    {
        if (!is_open)
        {
            workflow_->request_close_document(doc.root());
        }
        workflow_view_->draw_close_confirmation(doc);
        ImGui::End();
        return;
    }

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
    {
        context_->documents().set_active(doc.root());
    }

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
    if (doc.closing())
    {
        ImGui::BeginDisabled();
    }

    constexpr float inspector_width{320.0f};
    constexpr float minimum_inspector_width{240.0f};
    constexpr float minimum_hierarchy_width{200.0f};

    const ImVec2 available{ImGui::GetContentRegionAvail()};
    const float item_spacing{ImGui::GetStyle().ItemSpacing.x};
    const float hierarchy_width{std::max(0.0f, available.x - inspector_width - item_spacing)};
    const std::string hierarchy_name{std::format("hierarchy##hierarchy_{}_{}_{}_{}", root.hier.index, root.hier.generation, root.node.index, root.node.generation)};
    const float maximum_hierarchy_width{std::max(minimum_hierarchy_width, available.x - minimum_inspector_width - item_spacing)};
    ImGui::SetNextWindowSizeConstraints(
        ImVec2{std::min(minimum_hierarchy_width, maximum_hierarchy_width), -1.0f},
        ImVec2{maximum_hierarchy_width, -1.0f}
    );
    ImGui::BeginChild(hierarchy_name.c_str(), ImVec2{hierarchy_width, 0.0f}, ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
    hierarchy_->draw(doc);
    const float actual_hierarchy_width{ImGui::GetWindowSize().x};
    ImGui::EndChild();

    ImGui::SameLine();
    const std::string details_name{std::format("Details##details_{}_{}_{}_{}", root.hier.index, root.hier.generation, root.node.index, root.node.generation)};
    const float actual_inspector_width{std::max(0.0f, available.x - actual_hierarchy_width - item_spacing)};
    ImGui::BeginChild(details_name.c_str(), ImVec2{actual_inspector_width, 0.0f}, ImGuiChildFlags_Borders);
    ImGui::TextUnformatted("속성");
    ImGui::Separator();
    inspector_->draw(doc);
    if (preview_.has_content(doc, context_->workspace(doc.root())) && ImGui::CollapsingHeader("미리보기", ImGuiTreeNodeFlags_DefaultOpen))
    {
        preview_.draw(doc, context_->workspace(doc.root()));
    }
    ImGui::EndChild();

    ImGui::PopStyleColor();
    if (doc.closing())
    {
        ImGui::EndDisabled();
    }

    if (!is_open)
    {
        workflow_->request_close_document(doc.root());
    }
    workflow_view_->draw_close_confirmation(doc);
    ImGui::End();
}

void application::draw_explorer()
{
    if (workflow_->closing())
    {
        ImGui::BeginDisabled();
    }
    explorer_->render();
    if (workflow_->closing())
    {
        ImGui::EndDisabled();
    }
}

void application::draw_file_drop_target()
{
    const ImGuiDockNode* const document_node{ImGui::DockBuilderGetNode(document_dock_space_)};
    const ImGuiPayload* const drag_payload{ImGui::GetDragDropPayload()};
    if (!document_node || !drag_payload || std::string_view{drag_payload->DataType} != "nori_file_path")
    {
        return;
    }

    const ImRect document_rect{document_node->Pos, document_node->Pos + document_node->Size};
    constexpr ImGuiWindowFlags overlay_flags{
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBackground
    };
    ImGui::SetNextWindowPos(document_rect.Min);
    ImGui::SetNextWindowSize(document_rect.GetSize());
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{});
    if (!ImGui::Begin("##main_file_drop_overlay", nullptr, overlay_flags))
    {
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    ImGui::InvisibleButton("##main_file_drop_area", ImGui::GetContentRegionAvail());
    if (ImGui::IsMouseHoveringRect(document_rect.Min, document_rect.Max))
    {
        const ImU32 color{ImGui::GetColorU32(ImGuiCol_DragDropTarget)};
        ImGui::GetForegroundDrawList()->AddRect(document_rect.Min, document_rect.Max, color, 0.0f, 2.0f, ImDrawFlags_None);
    }
    if (ImGui::BeginDragDropTarget())
    {
        constexpr ImGuiDragDropFlags accept_flags{
            ImGuiDragDropFlags_AcceptBeforeDelivery |
            ImGuiDragDropFlags_AcceptNoDrawDefaultRect
        };
        if (const ImGuiPayload* payload{ImGui::AcceptDragDropPayload("nori_file_path", accept_flags)};
            payload && payload->DataSize > 0)
        {
            if (payload->IsDelivery())
            {
                const std::string_view path{
                    static_cast<const char*>(payload->Data),
                    static_cast<std::size_t>(payload->DataSize - 1)
                };
                workflow_->open_document(nori::core::utf8_to_path(path));
            }
        }
        ImGui::EndDragDropTarget();
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void application::draw_console()
{
    if (!console_.is_open())
    {
        return;
    }
    if (explorer_dock_space_)
    {
        ImGui::SetNextWindowDockID(explorer_dock_space_, ImGuiCond_FirstUseEver);
    }
    console_.draw(context_->assets());
}

void application::process_shortcuts()
{
    if (workflow_->requested() || ImGui::GetIO().WantTextInput)
    {
        return;
    }

    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_F, ImGuiInputFlags_RouteGlobal))
    {
        if (document_editor* const document{context_->documents().active()})
        {
            hierarchy_->focus_search(*document);
        }
        return;
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal))
    {
        undo();
        return;
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, ImGuiInputFlags_RouteGlobal) ||
        ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal))
    {
        redo();
        return;
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_N, ImGuiInputFlags_RouteGlobal))
    {
        workflow_->create_document();
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_O, ImGuiInputFlags_RouteGlobal))
    {
        workflow_->open_document_dialog();
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal))
    {
        workflow_->save_active_document();
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S, ImGuiInputFlags_RouteGlobal))
    {
        workflow_->save_all_documents();
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_W, ImGuiInputFlags_RouteGlobal) && context_->documents().active())
    {
        workflow_->request_close_document(context_->documents().active()->root());
    }
    if (ImGui::Shortcut(ImGuiKey_Delete, ImGuiInputFlags_RouteGlobal))
    {
        if (context_->documents().active())
        {
            if (document_editor* const document{context_->documents().active()})
            {
                hierarchy_->remove_selection(*document);
            }
        }
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_A, ImGuiInputFlags_RouteGlobal))
    {
        if (document_editor* const document{context_->documents().active()})
        {
            hierarchy_->select_all(*document);
        }
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_C, ImGuiInputFlags_RouteGlobal))
    {
        if (document_editor* const document{context_->documents().active()})
        {
            hierarchy_->copy_selection(*document);
        }
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_V, ImGuiInputFlags_RouteGlobal))
    {
        if (document_editor* const document{context_->documents().active()})
        {
            hierarchy_->paste_selection(*document);
        }
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_D, ImGuiInputFlags_RouteGlobal))
    {
        if (document_editor* const document{context_->documents().active()})
        {
            hierarchy_->duplicate_selection(*document);
        }
    }
    if (ImGui::Shortcut(ImGuiKey_F2, ImGuiInputFlags_RouteGlobal))
    {
        if (document_editor* const document{context_->documents().active()}; document && document->selection().selected())
        {
            hierarchy_->begin_rename(*document);
        }
    }
}

void application::undo()
{
    document_editor* const target{context_->documents().active()};
    if (!target || target->closing() || !target->can_undo())
    {
        return;
    }
    if (const auto res{target->undo()}; !res)
    {
        set_status(std::string{nori::resource::to_string(res.error())});
    }
}

void application::redo()
{
    document_editor* const target{context_->documents().active()};
    if (!target || target->closing() || !target->can_redo())
    {
        return;
    }
    if (const auto res{target->redo()}; !res)
    {
        set_status(std::string{nori::resource::to_string(res.error())});
    }
}

void application::set_status(std::string message)
{
    context_->set_status(std::move(message));
}

void application::shutdown_io()
{
    if (worker_context_)
    {
        nori::io::stop(worker_context_.get());
    }
    if (main_context_)
    {
        nori::io::stop(main_context_.get());
    }
    worker_threads_.clear();
    worker_context_.reset();
    main_context_.reset();
}
} // namespace nori::editor
