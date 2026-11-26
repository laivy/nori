add_library(nori_imgui STATIC)

target_sources(nori_imgui
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${PROJECT_SOURCE_DIR}/include
        FILES ${PROJECT_SOURCE_DIR}/include/nori/graphics/imgui.hpp
    PUBLIC
        FILE_SET imgui_headers TYPE HEADERS
        BASE_DIRS ${imgui_SOURCE_DIR}
        FILES
            ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.h
            ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.h
            ${imgui_SOURCE_DIR}/imconfig.h
            ${imgui_SOURCE_DIR}/imgui.h
            ${imgui_SOURCE_DIR}/imgui_internal.h
            ${imgui_SOURCE_DIR}/imstb_rectpack.h
            ${imgui_SOURCE_DIR}/imstb_textedit.h
            ${imgui_SOURCE_DIR}/imstb_truetype.h
            ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.h
    PRIVATE
        ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
)
target_include_directories(nori_imgui SYSTEM PUBLIC
    $<BUILD_INTERFACE:${imgui_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/imgui>
)

target_compile_features(nori_imgui PUBLIC cxx_std_23)
if (MSVC)
    target_compile_options(nori_imgui PRIVATE /W0 /WX- /utf-8)
else ()
    target_compile_options(nori_imgui PRIVATE -w -Wno-error)
endif ()
target_compile_definitions(nori_imgui PUBLIC
    IMGUI_DEFINE_MATH_OPERATORS
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS
)
target_link_libraries(nori_imgui PRIVATE "$<BUILD_LOCAL_INTERFACE:nori_compile_options>")
set_target_properties(nori_imgui PROPERTIES
    COMPILE_WARNING_AS_ERROR OFF
    CXX_EXTENSIONS OFF
    EXPORT_NAME imgui
)

add_library(nori_graphics_imgui INTERFACE)
add_library(nori::graphics::imgui ALIAS nori_graphics_imgui)

target_link_libraries(nori_graphics_imgui INTERFACE nori::graphics nori_imgui)
set_target_properties(nori_graphics_imgui PROPERTIES EXPORT_NAME graphics::imgui)
