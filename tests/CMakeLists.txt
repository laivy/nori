include(${PROJECT_SOURCE_DIR}/cmake/tests.cmake)

# 모듈 타겟과 같은 순서로 테스트 타겟 등록
# 규칙: tests/<module>/ 아래에 헤더 단위 소파일로 분할하고, 공용 픽스처/헬퍼는 *_helpers.hpp에 둔다.
# CTest 라벨: unit(순수 로직) / gpu(GPU·윈도우 필요) / integration(외부 도구·입출력 필요).
# backend 라벨은 내부 상태 제어·실패 주입이 필요한 테스트에 추가
if (TARGET nori::core)
    nori_add_test(nori_core_tests nori::core unit
        SOURCES
            core/application_test.cpp
            core/delegate_test.cpp
            core/encoding_test.cpp
            core/endian_test.cpp
            core/hash_test.cpp
            core/matrix_test.cpp
            core/multicast_delegate_test.cpp
            core/rtti_test.cpp
            core/slot_map_test.cpp
            core/timer_test.cpp
            core/vector_test.cpp
            core/windows_application_test.cpp
    )
    nori_add_test(nori_core_backend_tests nori::core "unit;backend"
        SOURCES core/input_backend_test.cpp)
    target_include_directories(nori_core_backend_tests PRIVATE ${PROJECT_SOURCE_DIR}/src/core)

    add_executable(nori_console_test_process)
    target_sources(nori_console_test_process PRIVATE core/console_test_process.cpp)
    if (WIN32)
        target_sources(nori_console_test_process PRIVATE core/console_test_faults.cpp)
    endif ()
    nori_configure_target(nori_console_test_process)
    target_link_libraries(nori_console_test_process PRIVATE nori::core)

    nori_add_test(nori_console_tests nori::core integration
        PROPERTIES TIMEOUT 30
        SOURCES core/console_application_test.cpp)
    target_compile_definitions(nori_console_tests PRIVATE NORI_CONSOLE_TEST_PROCESS="$<TARGET_FILE:nori_console_test_process>")
    add_dependencies(nori_console_tests nori_console_test_process)
endif ()

if (TARGET nori::io)
    nori_add_test(nori_io_tests nori::io unit
        SOURCES
            io/acceptor_test.cpp
            io/awaitable_test.cpp
            io/connection_test.cpp
            io/context_test.cpp
            io/packet_test.cpp
            io/post_test.cpp
            io/steady_timer_test.cpp
            io/strand_test.cpp
    )
    target_link_libraries(nori_io_tests PRIVATE nori::asio)
endif ()

if (TARGET nori::resource)
    nori_add_test(nori_resource_tests nori::resource unit
        SOURCES
            resource/codec_test.cpp
            resource/file_registry_test.cpp
            resource/lifecycle_test.cpp
            resource/loading_test.cpp
            resource/path_resolution_test.cpp
            resource/path_test.cpp
            resource/save_test.cpp
            resource/structure_test.cpp
            resource/value_test.cpp
    )
endif ()

if (TARGET nori::graphics)
    add_library(nori_graphics_test_support STATIC)
    target_sources(nori_graphics_test_support PRIVATE
        graphics/diagnostics_test_helpers.cpp
        graphics/text_test_helpers.cpp
    )
    target_include_directories(
        nori_graphics_test_support
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/graphics
            ${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>
        PRIVATE ${PROJECT_SOURCE_DIR}/src/graphics
    )
    nori_configure_target(nori_graphics_test_support)
    target_link_libraries(nori_graphics_test_support PRIVATE nori::graphics GTest::gtest PUBLIC Microsoft::DirectX-Headers)

    nori_add_test(nori_graphics_tests nori::graphics gpu
        PROPERTIES TIMEOUT 300 RESOURCE_LOCK nori-gpu
        SOURCES
            graphics/command_encoder_test.cpp
            graphics/frame_upload_buffer_lifetime_test.cpp
            graphics/frame_upload_buffer_test.cpp
            graphics/graphics_test.cpp
            graphics/line_test.cpp
            graphics/rect_test.cpp
            graphics/text_test.cpp
            graphics/transfer_order_test.cpp
    )
    target_link_libraries(nori_graphics_tests PRIVATE nori_graphics_test_support)

    nori_add_test(nori_graphics_backend_tests nori::graphics "gpu;backend"
        PROPERTIES TIMEOUT 300 RESOURCE_LOCK nori-gpu
        SOURCES
            graphics/frame_data_backend_test.cpp
            graphics/lifetime_backend_test.cpp
            graphics/queue_backend_test.cpp
            graphics/recording_backend_test.cpp
            graphics/resource_lifetime_backend_test.cpp
            graphics/submission_backend_test.cpp
            graphics/text_failure_backend_test.cpp
    )
    target_include_directories(nori_graphics_backend_tests PRIVATE ${PROJECT_SOURCE_DIR}/src/graphics)
    target_link_libraries(nori_graphics_backend_tests PRIVATE nori_graphics_test_support)

    foreach (source IN ITEMS
        graphics/shader/quad.ps.hlsl
        graphics/shader/quad.vs.hlsl
        graphics/shader/sample.ps.hlsl
        graphics/shader/sample.vs.hlsl
        graphics/shader/text_effect.ps.hlsl
        graphics/shader/text_identity.ps.hlsl
        graphics/shader/text_identity.vs.hlsl
        graphics/shader/text_masks.ps.hlsl
        graphics/shader/text_translate.vs.hlsl
    )
        cmake_path(GET source STEM LAST_ONLY name)
        string(REPLACE "." "_" symbol "${name}")
        string(REGEX REPLACE "^.*\\.(vs|ps)$" "\\1" stage "${name}")
        set(output "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>/test_shader/${symbol}.hpp")
        add_custom_command(
            OUTPUT "${output}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>/test_shader"
            COMMAND nori_dxc
                -E main -T "${stage}_6_0" -HV 2021 -WX
                -I "${PROJECT_SOURCE_DIR}/include"
                "$<$<CONFIG:Debug>:-Od>"
                "$<$<CONFIG:Debug>:-Zi>"
                "$<$<CONFIG:Debug>:-Qembed_debug>"
                "$<$<NOT:$<CONFIG:Debug>>:-O3>"
                -Fh "${output}" -Vn "${symbol}" "${CMAKE_CURRENT_SOURCE_DIR}/${source}"
            DEPENDS "${source}" graphics/shader/quad.hlsli ${PROJECT_SOURCE_DIR}/include/nori/graphics/shader/text.hlsli nori_dxc
            VERBATIM
        )
        target_sources(nori_graphics_test_support PRIVATE "${output}" "${source}")
        set_source_files_properties("${source}" PROPERTIES HEADER_FILE_ONLY TRUE)
    endforeach ()
    target_sources(nori_graphics_test_support PRIVATE graphics/shader/quad.hlsli)
endif ()

if (TARGET nori::graphics::imgui)
    nori_add_test(nori_imgui_tests nori::graphics::imgui gpu
        PROPERTIES TIMEOUT 300 RESOURCE_LOCK nori-gpu
        SOURCES
            imgui/lifetime_test.cpp
            imgui/render_test.cpp
    )
    target_link_libraries(nori_imgui_tests PRIVATE nori_graphics_test_support)

    nori_add_test(nori_imgui_backend_tests nori::graphics::imgui "gpu;backend"
        PROPERTIES TIMEOUT 300 RESOURCE_LOCK nori-gpu
        SOURCES imgui/viewport_lifetime_backend_test.cpp)
    target_include_directories(nori_imgui_backend_tests PRIVATE ${PROJECT_SOURCE_DIR}/src/graphics)
    target_link_libraries(nori_imgui_backend_tests PRIVATE nori_graphics_test_support)
endif ()

if (TARGET nori::physics)
    nori_add_test(nori_physics_tests nori::physics unit
        SOURCES physics/physics2d/scene_test.cpp)
endif ()

if (TARGET nori::gameplay)
    nori_add_test(nori_gameplay_tests nori::gameplay unit
        SOURCES gameplay/framework_test.cpp)
endif ()

if (TARGET nori_cli_core)
    nori_add_test(nori_cli_tests nori_cli_core unit
        PROPERTIES TIMEOUT 600
        SOURCES
            cli/command_test.cpp
            cli/protocol_test.cpp)
    target_link_libraries(nori_cli_tests PRIVATE nori::core)
    nori_copy_asset_dxcompiler(nori_cli_tests)

    nori_add_test(nori_cli_import_tests nori_cli_core integration
        PROPERTIES TIMEOUT 600
        SOURCES cli/import_test.cpp)
    target_compile_definitions(
        nori_cli_import_tests
        PRIVATE
            NORI_UFBX_AXES_PATH="${ufbx_SOURCE_DIR}/data/blender_axes/axes_nypz_7400_binary.fbx"
            NORI_UFBX_CUBE_PATH="${ufbx_SOURCE_DIR}/data/blender_272_cube_7400_binary.fbx"
            NORI_UFBX_SKIN_PATH="${ufbx_SOURCE_DIR}/data/max_transformed_skin_7500_ascii.fbx"
            NORI_UFBX_OTHER_SKIN_PATH="${ufbx_SOURCE_DIR}/data/maya_skinned_pivot_7700_ascii.fbx"
            NORI_UFBX_TEXTURED_PATH="${ufbx_SOURCE_DIR}/data/maya_textured_cube_7500_ascii.fbx"
            NORI_UFBX_TEXTURE_DIR="${ufbx_SOURCE_DIR}/data/textures"
            NORI_UFBX_MULTI_ANIM_PATH="${ufbx_SOURCE_DIR}/data/synthetic_anim_stack_no_props_7100_ascii.fbx"
    )
    target_link_libraries(nori_cli_import_tests PRIVATE nori::core)
    nori_copy_asset_dxcompiler(nori_cli_import_tests)
endif ()

if (TARGET noritor_core)
    nori_add_test(noritor_tests noritor_core integration
        PROPERTIES TIMEOUT 600
        SOURCES
            noritor/command_test.cpp
            noritor/document_editor_test.cpp)
    nori_add_test(noritor_preview_tests noritor_core "gpu;integration"
        PROPERTIES TIMEOUT 600 RESOURCE_LOCK nori-gpu
        SOURCES noritor/preview_test.cpp)
endif ()
