#pragma once
#include <array>
#include <atomic>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <Windows.h>

#include <imgui.h>

#include <nori/core/windows_application.hpp>
#include <nori/graphics/swap_chain.hpp>
#include <nori/io/context.hpp>
#include <nori/resource/handle.hpp>

#include "console.hpp"
#include "file_dialog.hpp"
#include "preview.hpp"
#include "text_outline_example.hpp"

namespace nori::editor
{
class application_workflow;
class workflow_view;
class context;
class document_editor;
class import_workflow;
class explorer;
class hierarchy;
class inspector;

class application : public nori::core::windows_application
{
public:
    explicit application(const specification& spec);
    ~application() override;

protected:
    void on_start() override;
    void on_stop() override;

    void on_update(float delta_seconds) override;
    void on_render() override;

    void on_window_resize(int width, int height) override;
    void on_window_close() override;

    void on_text_input(char32_t character) override;

    LRESULT on_window_message(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) override;

private:
    struct input_message
    {
        HWND hwnd;
        UINT message;
        WPARAM w_param;
        LPARAM l_param;
    };

    enum class window_button
    {
        none,
        minimize,
        maximize,
        close
    };

    void process_input_messages();
    void process_input_message(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);

    void initialize_custom_title_bar();
    LRESULT title_bar_hit_test(HWND hwnd, LPARAM l_param);
    void execute_window_command(window_button button);
    void draw_window_button(window_button button, RECT rect) const;

    void update_previews();
    bool render_frame();

    void draw_ui();
    void draw_main_menu_bar();
    void draw_title_bar();
    void draw_status_bar();
    void draw_main_dock_space();
    void draw_document_workspaces();
    void draw_document(document_editor& doc);
    void draw_explorer();
    void draw_file_drop_target();
    void draw_console();
    void process_shortcuts();

    void undo();
    void redo();

    void set_status(std::string message);
    void shutdown_io();

    std::mutex input_mutex_;
    std::deque<input_message> input_messages_;
    std::array<bool, 256> input_keys_{};

    nori::graphics::unique_swap_chain_handle main_swap_chain_;

    nori::io::unique_context_handle main_context_;
    nori::io::unique_context_handle worker_context_;
    std::vector<std::jthread> worker_threads_;

    std::atomic<HCURSOR> mouse_cursor_;
    std::mutex title_bar_mutex_;
    RECT title_drag_rect_;
    RECT minimize_button_rect_;
    RECT maximize_button_rect_;
    RECT close_button_rect_;
    std::atomic<window_button> hovered_window_button_;
    std::atomic<window_button> pressed_window_button_;

    preview preview_;
    text_outline_example text_outline_example_;
    console console_;
    file_dialog file_dialog_;

    std::unique_ptr<context> context_;
    std::unique_ptr<import_workflow> importer_;
    std::unique_ptr<explorer> explorer_;
    std::unique_ptr<inspector> inspector_;
    std::unique_ptr<hierarchy> hierarchy_;

    ImGuiID document_dock_space_; // 상단 파일 편집 영역
    ImGuiID explorer_dock_space_; // 하단 탐색기 영역

    std::unique_ptr<application_workflow> workflow_;
    std::unique_ptr<workflow_view> workflow_view_;
};
} // namespace nori::editor
