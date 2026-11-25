add_library(noritor_core STATIC)

target_sources(noritor_core
    PRIVATE
        asset_manager.cpp
        asset_manager.hpp
        clipboard.cpp
        clipboard.hpp
        command.cpp
        command.hpp
        console.cpp
        console.hpp
        context.cpp
        context.hpp
        document.cpp
        document.hpp
        document_editor.cpp
        document_editor.hpp
        document_manager.cpp
        document_manager.hpp
        document_workspace.cpp
        document_workspace.hpp
        exit_workflow.cpp
        exit_workflow.hpp
        log.cpp
        log.hpp
        preview.cpp
        preview.hpp
        resource_helpers.hpp
        settings.cpp
        settings.hpp
        temporary_storage.cpp
        temporary_storage.hpp
)
target_include_directories(noritor_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_precompile_headers(noritor_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/pch.hpp)
nori_configure_target(noritor_core)
target_link_libraries(noritor_core PUBLIC nori::core nori::graphics nori::graphics::imgui nori::asset)

add_executable(noritor WIN32)

target_sources(noritor
    PRIVATE
        application.cpp
        application.hpp
        application_workflow.cpp
        application_workflow.hpp
        explorer.cpp
        explorer.hpp
        file_dialog.cpp
        file_dialog.hpp
        hierarchy.cpp
        hierarchy.hpp
        inspector.cpp
        inspector.hpp
        import_workflow.cpp
        import_workflow.hpp
        main.cpp
        pch.hpp
        text_outline_example.cpp
        text_outline_example.hpp
        utils.cpp
        utils.hpp
        workflow_view.cpp
        workflow_view.hpp
)
target_precompile_headers(noritor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/pch.hpp)
nori_configure_target(noritor)
target_link_libraries(noritor PRIVATE noritor_core nori::core nori::io nori::asset)

add_custom_target(noritor_assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:noritor>/data"
    VERBATIM
)
add_dependencies(noritor noritor_assets)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data")
    add_custom_target(noritor_data
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${CMAKE_CURRENT_SOURCE_DIR}/data" "$<TARGET_FILE_DIR:noritor>/data"
        VERBATIM
    )
    add_dependencies(noritor noritor_data)
endif ()

if (NORI_INSTALL)
    install(TARGETS noritor
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif ()
