#pragma once
#include "context.hpp"
#include "explorer.hpp"
#include "file_dialog.hpp"
#include "hierarchy.hpp"
#include "inspector.hpp"
#include "settings.hpp"

class noritor_application : public nori::core::windows_application
{
public:
    noritor_application(specification spec);
    ~noritor_application();

    void load_data_file(const std::filesystem::path& path);
    void save_selected_file();
    void save_all_files();

    void set_modified(nori::resource::handle handle);
    void select_resource(nori::resource::handle handle);

    context& get_context();
    context::root* get_root(nori::resource::handle handle);

protected:
    virtual LRESULT on_window_message(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) override;
    virtual void on_tick(float delta_seconds) override;

private:
    void render();
    void main_menu();
    void main_dock_space();
    void status_bar();
    void shortcuts();
    void data_file_dialog();

    void new_file();
    void open_data_file_dialog();

private:
    nori::resource::runtime resource_;
    nori::graphics::runtime graphics_;
    nori::graphics::swap_chain_handle main_swap_chain_;
    nori::graphics::shader_handle fullscreen_vertex_shader_;
    nori::graphics::shader_handle fullscreen_pixel_shader_;
    nori::graphics::pipeline_handle fullscreen_pipeline_;
    settings settings_;
    context context_;
    explorer explorer_;
    hierarchy hierarchy_;
    inspector inspector_;
};
