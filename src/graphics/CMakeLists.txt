if (NOT WIN32)
    message(FATAL_ERROR "nori graphics support is only supported on Windows.")
endif ()

add_library(nori_graphics STATIC)
add_library(nori::graphics ALIAS nori_graphics)
set_property(TARGET nori_graphics PROPERTY EXPORT_NAME graphics)
target_compile_features(nori_graphics PUBLIC cxx_std_23)
nori_configure_target(nori_graphics)

target_sources(nori_graphics
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${PROJECT_SOURCE_DIR}/include
        FILES
            ${PROJECT_SOURCE_DIR}/include/nori/graphics.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/graphics/accessor.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/graphics/error_code.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/graphics/imgui_accessor.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/graphics/pipeline.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/graphics/draw_list.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/graphics/render_target.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/graphics/runtime.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/accessor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/constants.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/context.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/context.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/descriptor_allocator.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/descriptor_allocator.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/fence.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/fence.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/gpu_timeline.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/gpu_timeline.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui_accessor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui_context.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui_context.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/pipeline_manager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/pipeline_manager.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/draw_list.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/render_assets.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/render_assets.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/swap_chain.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/swap_chain.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/swap_chain_manager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/swap_chain_manager.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/swap_chain_render_target.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/swap_chain_render_target.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/render_target_manager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/render_target_manager.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/renderer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/renderer.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/runtime.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/texture.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/texture_manager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/texture_manager.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/upload_manager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/upload_manager.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/viewport.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/viewport.hpp
)
target_include_directories(nori_graphics PRIVATE ${PROJECT_SOURCE_DIR}/include/nori/graphics)
target_link_libraries(nori_graphics PUBLIC nori::core nori::resource)

# DirectX12
set(DXHEADERS_BUILD_GOOGLE_TEST OFF)
set(DXHEADERS_BUILD_TEST OFF)
set(DXHEADERS_INSTALL OFF)
set(BUILD_TESTING OFF)
FetchContent_MakeAvailable(DirectX-Headers)
target_link_libraries(nori_graphics PRIVATE
    "$<BUILD_LOCAL_INTERFACE:Microsoft::DirectX-Headers>$<INSTALL_INTERFACE:nori::directx_headers>"
    "$<BUILD_LOCAL_INTERFACE:Microsoft::DirectX-Guids>$<INSTALL_INTERFACE:nori::directx_guids>"
)

target_compile_definitions(nori_graphics PRIVATE USING_DIRECTX_HEADERS)
target_link_libraries(nori_graphics PRIVATE d3d12 dxgi)

include(${PROJECT_SOURCE_DIR}/cmake/compile_hlsl.cmake)

if (NORI_ENABLE_IMGUI)
    FetchContent_MakeAvailable(imgui)

    set(imgui_files
        ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.h
        ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.h
        ${imgui_SOURCE_DIR}/imconfig.h
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui.h
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_internal.h
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imstb_rectpack.h
        ${imgui_SOURCE_DIR}/imstb_textedit.h
        ${imgui_SOURCE_DIR}/imstb_truetype.h
        ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
        ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.h
    )

    add_library(nori_imgui_client INTERFACE)
    target_include_directories(nori_imgui_client SYSTEM INTERFACE ${imgui_SOURCE_DIR})
    target_compile_definitions(nori_imgui_client INTERFACE
        IMGUI_DEFINE_MATH_OPERATORS
        IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    )

    target_sources(nori_graphics PRIVATE ${imgui_files})
    if (MSVC)
        set_property(SOURCE ${imgui_files} APPEND PROPERTY COMPILE_OPTIONS /W0)
    else ()
        set_property(SOURCE ${imgui_files} APPEND PROPERTY COMPILE_OPTIONS -w)
    endif ()
    target_include_directories(nori_graphics SYSTEM PRIVATE ${imgui_SOURCE_DIR})
    target_compile_definitions(nori_graphics PRIVATE
        IMGUI_DEFINE_MATH_OPERATORS
        IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    )
else ()
    target_compile_definitions(nori_graphics PUBLIC IMGUI_DISABLE)
endif ()

install(TARGETS nori_graphics EXPORT noriTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(FILES
    $<TARGET_FILE:DirectX-Headers>
    $<TARGET_FILE:DirectX-Guids>
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
)
install(FILES ${PROJECT_SOURCE_DIR}/cmake/compile_hlsl.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/nori
)
