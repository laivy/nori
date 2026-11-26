add_library(nori_graphics STATIC)
add_library(nori::graphics ALIAS nori_graphics)

# 셰이더 컴파일
include(${PROJECT_SOURCE_DIR}/cmake/shaders.cmake)
file(GLOB shaders CONFIGURE_DEPENDS
    LIST_DIRECTORIES FALSE
    "${CMAKE_CURRENT_SOURCE_DIR}/shader/*.hlsl"
)
file(GLOB_RECURSE shader_deps CONFIGURE_DEPENDS
    LIST_DIRECTORIES FALSE
    "${CMAKE_CURRENT_SOURCE_DIR}/shader/*.hlsli"
)
set(shader_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>/shader")
foreach (src IN LISTS shaders)
    nori_compile_shader(nori_graphics
        "${src}"
        "${shader_dir}"
        ${shader_deps}
        ${PROJECT_SOURCE_DIR}/include/nori/graphics/shader/text.hlsli
    )
endforeach ()

target_sources(nori_graphics
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${PROJECT_SOURCE_DIR}/include
        FILES
            ${PROJECT_SOURCE_DIR}/include/nori/graphics.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/graphics/buffer.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/graphics/command_encoder.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/graphics/error_code.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/graphics/line.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/graphics/operations.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/graphics/pipeline.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/graphics/rect.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/graphics/render_pass.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/graphics/sampler.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/graphics/shader/text.hlsli
            ${PROJECT_SOURCE_DIR}/include/nori/graphics/swap_chain.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/graphics/text.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/graphics/texture.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/graphics/transfer.hpp
    PRIVATE
        command_buffer.hpp
        command_encoder.cpp
        command_encoder_internal.hpp
        command_manager.cpp
        command_manager.hpp
        command_queue.cpp
        command_queue.hpp
        config.hpp
        context.cpp
        context.hpp
        context_frame.cpp
        context_resource.cpp
        context_text.cpp
        descriptor_allocator.cpp
        descriptor_allocator.hpp
        format.cpp
        format.hpp
        operations.cpp
        rendering/renderer.cpp
        rendering/renderer.hpp
        rendering/renderer_draw.cpp
        rendering/renderer_shape.cpp
        rendering/renderer_target.cpp
        resource/buffer_manager.cpp
        resource/buffer_manager.hpp
        resource/pipeline_manager.cpp
        resource/pipeline_manager.hpp
        resource/render_target_info.hpp
        resource/render_target_signature.hpp
        resource/sampler_manager.cpp
        resource/sampler_manager.hpp
        resource/swap_chain.cpp
        resource/swap_chain.hpp
        resource/swap_chain_manager.cpp
        resource/swap_chain_manager.hpp
        resource/texture_manager.cpp
        resource/texture_manager.hpp
        resource_states.cpp
        resource_states.hpp
        text/font_registry.cpp
        text/font_registry.hpp
        text/layout.cpp
        text/layout.hpp
        text/glyph_atlas.cpp
        text/glyph_atlas.hpp
        text/glyph_rasterizer.cpp
        text/glyph_rasterizer.hpp
        text/layout_objects.cpp
        text/layout_objects.hpp
        text/manager.cpp
        text/manager.hpp
        text/page_refs.cpp
        text/page_refs.hpp
        text/pipeline_cache.cpp
        text/pipeline_cache.hpp
        transfer/manager.cpp
        transfer/manager.hpp
        upload_manager.cpp
        upload_manager.hpp
        ${shader_deps}
        ${shaders}
)
foreach (area IN ITEMS text transfer)
    set_source_files_properties(${area}/manager.cpp PROPERTIES
        VS_SETTINGS "ModuleDependenciesFile=$(IntDir)${area}_manager.cpp.module.json"
    )
endforeach ()

if (nori_enable_imgui)
    target_sources(nori_graphics PRIVATE
        imgui/imgui_accessor.cpp
        imgui/imgui_context.cpp
        imgui/imgui_context.hpp
    )
endif ()

set_source_files_properties(${shaders} ${shader_deps} PROPERTIES HEADER_FILE_ONLY TRUE)

target_include_directories(nori_graphics PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>
    ${CMAKE_CURRENT_SOURCE_DIR}
)
nori_configure_target(nori_graphics)
target_compile_definitions(nori_graphics PRIVATE USING_DIRECTX_HEADERS)
if (nori_enable_imgui)
    target_compile_definitions(nori_graphics PRIVATE NORI_ENABLE_IMGUI)
endif ()

# DirectX12
target_link_libraries(nori_graphics
    PUBLIC
        nori::core nori::resource
    PRIVATE
        "$<BUILD_LOCAL_INTERFACE:Microsoft::DirectX-Headers>"
        "$<BUILD_LOCAL_INTERFACE:Microsoft::DirectX-Guids>$<INSTALL_INTERFACE:nori::directx_guids>"
        d3d12 dxgi dwrite d2d1 gdi32
)

if (nori_enable_imgui)
    target_link_libraries(nori_graphics PRIVATE nori_imgui)
endif ()

set_target_properties(nori_graphics PROPERTIES EXPORT_NAME graphics)

if (nori_enable_imgui)
    add_subdirectory(imgui)
endif ()

if (NORI_INSTALL)
    install(FILES
        $<TARGET_FILE:Microsoft::DirectX-Guids>
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    )
endif ()
