#include "pch.hpp"
#include <shaders/fullscreen_ps.hpp>
#include <shaders/fullscreen_vs.hpp>
#include "noritor_application.hpp"
#include "utils.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);

noritor_application::noritor_application(specification spec) :
    nori::core::windows_application{std::move(spec)},
    resource_{nori::resource::runtime::specification{}},
    graphics_{},
    explorer_{*this},
    hierarchy_{*this},
    inspector_{*this}
{
    if (auto result{nori::graphics::imgui::initialize(hwnd())}; !result)
    {
        throw std::runtime_error{std::string{nori::graphics::to_string(result.error())}};
    }
    auto swap_chain{nori::graphics::create_swap_chain({.hwnd = hwnd()})};
    if (!swap_chain)
    {
        throw std::runtime_error{std::string{nori::graphics::to_string(swap_chain.error())}};
    }
    main_swap_chain_ = *swap_chain;

    auto vertex_shader{nori::graphics::create_shader(
        {.stage = nori::graphics::shader_stage::vertex, .bytecode = std::as_bytes(std::span{fullscreen_vs})}
    )};
    auto pixel_shader{nori::graphics::create_shader(
        {.stage = nori::graphics::shader_stage::pixel, .bytecode = std::as_bytes(std::span{fullscreen_ps})}
    )};
    if (!vertex_shader || !pixel_shader)
    {
        const auto error{vertex_shader ? pixel_shader.error() : vertex_shader.error()};
        throw std::runtime_error{std::string{nori::graphics::to_string(error)}};
    }
    fullscreen_vertex_shader_ = *vertex_shader;
    fullscreen_pixel_shader_ = *pixel_shader;

    auto pipeline{nori::graphics::create_graphics_pipeline({.vertex_shader = fullscreen_vertex_shader_, .pixel_shader = fullscreen_pixel_shader_, .bindings = {{nori::graphics::shader_binding_type::texture_srv, 0, nori::graphics::shader_stage::pixel}, {nori::graphics::shader_binding_type::static_sampler, 0, nori::graphics::shader_stage::pixel}}, .color_formats = {nori::graphics::render_texture_format::rgba8_unorm}})};
    if (!pipeline)
    {
        throw std::runtime_error{std::string{nori::graphics::to_string(pipeline.error())}};
    }
    fullscreen_pipeline_ = *pipeline;

    add_static_event_listener(
        [this](const nori::core::window_resize_event& e)
        {
            if (!nori::graphics::resize_swap_chain(main_swap_chain_, static_cast<std::uint32_t>(e.width), static_cast<std::uint32_t>(e.height)))
            {
                quit(nori::core::application::exit_code::error);
            }
            return false;
        }
    );
}

noritor_application::~noritor_application()
{
    std::ignore = nori::graphics::destroy(fullscreen_pipeline_);
    std::ignore = nori::graphics::destroy(fullscreen_pixel_shader_);
    std::ignore = nori::graphics::destroy(fullscreen_vertex_shader_);
    std::ignore = nori::graphics::destroy(main_swap_chain_);
}

void noritor_application::load_data_file(const std::filesystem::path& path)
{
    if (!utils::is_nori_file(path))
    {
        return;
    }
    const std::string key{utils::to_utf8(path)};
    const nori::resource::handle root{nori::resource::get(key)};
    if (!root)
    {
        return;
    }
    if (!std::ranges::contains(context_.roots, root, &context::root::handle))
    {
        context_.roots.push_back({path, root, false});
    }
    hierarchy_.open(root);
    hierarchy_.select(root);
}

void noritor_application::save_selected_file()
{
    if (!context_.selected)
    {
        return;
    }
    context::root* root{get_root(*context_.selected)};
    if (!root)
    {
        return;
    }
    if (nori::resource::serialize(root->path, root->handle))
    {
        root->is_modified = false;
    }
}

void noritor_application::save_all_files()
{
    for (context::root& root : context_.roots)
    {
        if (nori::resource::serialize(root.path, root.handle))
        {
            root.is_modified = false;
        }
    }
}

void noritor_application::set_modified(nori::resource::handle handle)
{
    if (context::root * root{get_root(handle)})
    {
        root->is_modified = true;
    }
}

void noritor_application::select_resource(nori::resource::handle handle)
{
    hierarchy_.select(handle);
}

context& noritor_application::get_context()
{
    return context_;
}

context::root* noritor_application::get_root(nori::resource::handle handle)
{
    auto find = [this](nori::resource::handle handle)
    {
        return std::ranges::find(context_.roots, handle, &context::root::handle);
    };
    if (auto it{find(handle)}; it != context_.roots.end())
    {
        return &*it;
    }
    for (nori::resource::handle parent{nori::resource::get_parent(handle)}; parent; parent = nori::resource::get_parent(parent))
    {
        if (auto root{find(parent)}; root != context_.roots.end())
        {
            return &*root;
        }
    }
    return nullptr;
}

LRESULT noritor_application::on_window_message(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param)
{
    if (::ImGui_ImplWin32_WndProcHandler(hwnd, message, w_param, l_param))
    {
        return TRUE;
    }
    return nori::core::windows_application::on_window_message(hwnd, message, w_param, l_param);
}

void noritor_application::on_tick(float)
{
    if (!nori::graphics::begin_frame())
    {
        quit(nori::core::application::exit_code::error);
        return;
    }
    auto queue{nori::graphics::begin_pass(main_swap_chain_)};
    if (!queue)
    {
        quit(nori::core::application::exit_code::error);
        return;
    }
    render();
    if (!nori::graphics::imgui::render() ||
        !nori::graphics::end_pass(*queue) ||
        !nori::graphics::end_frame() ||
        !nori::graphics::present(main_swap_chain_))
    {
        quit(nori::core::application::exit_code::error);
    }
}

void noritor_application::render()
{
    settings_.push();
    main_menu();
    main_dock_space();
    explorer_.render();
    hierarchy_.render();
    inspector_.render();
    status_bar();
    shortcuts();
    data_file_dialog();
    settings_.pop();
}

void noritor_application::main_menu()
{
    if (!ImGui::BeginMainMenuBar())
    {
        return;
    }
    if (ImGui::BeginMenu("파일"))
    {
        if (ImGui::MenuItem("새 파일", "Ctrl+N"))
        {
            new_file();
        }
        if (ImGui::MenuItem("열기...", "Ctrl+O"))
        {
            open_data_file_dialog();
        }
        if (ImGui::MenuItem("저장", "Ctrl+S", false, context_.selected.has_value()))
        {
            save_selected_file();
        }
        if (ImGui::MenuItem("모두 저장", "Ctrl+Shift+S", false, !context_.roots.empty()))
        {
            save_all_files();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("종료"))
        {
            quit(nori::core::application::exit_code::success);
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("창"))
    {
        ImGui::MenuItem("...");
        ImGui::EndMenu();
    }
    // `설정` 메뉴
    settings_.render_menu();
    ImGui::EndMainMenuBar();
}

void noritor_application::main_dock_space()
{
    const ImGuiViewport* const viewport{ImGui::GetMainViewport()};
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    constexpr const char* name{"NoritorMainDockSpace"};
    context_.dock_space_id = ImGui::GetID(name);

    constexpr ImGuiWindowFlags flags{
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoDecoration
    };
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{});
    ImGui::Begin(name, nullptr, flags);
    ImGui::PopStyleVar();
    ImGui::DockSpace(context_.dock_space_id);
    ImGui::End();
}

void noritor_application::status_bar()
{
    const ImGuiViewport* const viewport{ImGui::GetMainViewport()};
    const ImGuiStyle& style{ImGui::GetStyle()};
    const ImVec2 padding{style.WindowPadding.x, style.FramePadding.y};
    const float height{ImGui::GetTextLineHeight() + padding.y * 2.0f};
    ImGui::SetNextWindowPos(ImVec2{viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - height});
    ImGui::SetNextWindowSize(ImVec2{viewport->WorkSize.x, height});
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
    ImGui::Begin(
        "Status Bar",
        nullptr,
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoDecoration
    );
    ImGui::Text("문서 %zu개", context_.roots.size());
    ImGui::End();
    ImGui::PopStyleVar();
}

void noritor_application::shortcuts()
{
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_N, ImGuiInputFlags_RouteGlobal))
    {
        new_file();
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_O, ImGuiInputFlags_RouteGlobal))
    {
        open_data_file_dialog();
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal))
    {
        save_selected_file();
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S, ImGuiInputFlags_RouteGlobal))
    {
        save_all_files();
    }
    if (ImGui::Shortcut(ImGuiKey_Delete, ImGuiInputFlags_RouteGlobal))
    {
        hierarchy_.delete_selected();
    }
}

void noritor_application::data_file_dialog()
{
    const std::filesystem::path path{file_dialog::render("열기(.nori)##noritor_application")};
    if (!path.empty())
    {
        load_data_file(path);
    }
}

void noritor_application::new_file()
{
    const std::size_t index{context_.roots.size() + 1};
    const std::string key{std::format("untitled{}{}", index, nori::resource::extension)};
    const nori::resource::handle root{nori::resource::create(key)};
    if (!root)
    {
        return;
    }
    context_.roots.push_back({std::filesystem::current_path() / key, root, true});
    hierarchy_.open(root);
    hierarchy_.select(root);
}

void noritor_application::open_data_file_dialog()
{
    file_dialog::open("열기(.nori)##noritor_application", {std::filesystem::path{nori::resource::extension}});
}
