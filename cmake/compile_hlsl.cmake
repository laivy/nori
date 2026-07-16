include_guard(GLOBAL)

include(FetchContent)
if (NOT TARGET nori_dxc)
    FetchContent_Declare(
        nori_dxc_archive
        URL https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2602.24/dxc_2026_05_27.zip
        URL_HASH SHA256=cf658aacf070d3045e31b8f1f8a696c2945f37c1095019481ef7c513368db3b4
        DOWNLOAD_NO_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(nori_dxc_archive)
    add_executable(nori_dxc IMPORTED GLOBAL)
    set_target_properties(nori_dxc PROPERTIES
        IMPORTED_LOCATION "${nori_dxc_archive_SOURCE_DIR}/bin/x64/dxc.exe"
    )
    add_executable(nori::dxc ALIAS nori_dxc)
endif ()

function(nori_compile_hlsl)
    cmake_parse_arguments(
        ARG
        ""
        "TARGET;SOURCE;ENTRY;PROFILE;OUTPUT;VARIABLE"
        "DEPENDS;INCLUDE_DIRECTORIES;DEFINES"
        ${ARGN}
    )
    foreach(required TARGET SOURCE ENTRY PROFILE OUTPUT VARIABLE)
        if (NOT ARG_${required})
            message(FATAL_ERROR "nori_compile_hlsl requires ${required}.")
        endif ()
    endforeach ()
    if (NOT TARGET ${ARG_TARGET})
        message(FATAL_ERROR "nori_compile_hlsl TARGET '${ARG_TARGET}' does not exist.")
    endif ()

    get_filename_component(source "${ARG_SOURCE}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    get_filename_component(output "${ARG_OUTPUT}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    get_filename_component(output_dir "${output}" DIRECTORY)

    set(include_args)
    foreach(directory IN LISTS ARG_INCLUDE_DIRECTORIES)
        get_filename_component(include_directory "${directory}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        list(APPEND include_args -I "${include_directory}")
    endforeach ()
    
    set(define_args)
    foreach(definition IN LISTS ARG_DEFINES)
        list(APPEND define_args -D "${definition}")
    endforeach ()

    add_custom_command(
        OUTPUT "${output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${output_dir}"
        COMMAND $<TARGET_FILE:nori_dxc>
            -E "${ARG_ENTRY}"
            -T "${ARG_PROFILE}"
            -HV 2021
            -WX
            $<$<CONFIG:Debug>:-Zi>
            $<$<CONFIG:Debug>:-Od>
            $<$<CONFIG:Debug>:-Qembed_debug>
            $<$<NOT:$<CONFIG:Debug>>:-O3>
            ${include_args}
            ${define_args}
            -Fh "${output}"
            -Vn "${ARG_VARIABLE}"
            "${source}"
        DEPENDS "${source}" ${ARG_DEPENDS} nori_dxc
        VERBATIM
        COMMAND_EXPAND_LISTS
        COMMENT "Compiling HLSL ${ARG_SOURCE} (${ARG_PROFILE})"
    )
    set_source_files_properties("${output}" PROPERTIES GENERATED TRUE)
    target_sources(${ARG_TARGET} PRIVATE "${output}")
    set(${ARG_TARGET}_OUTPUT "${output}" PARENT_SCOPE)
endfunction()
