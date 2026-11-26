include_guard(GLOBAL)

function(nori_compile_shader target source output_dir)
    cmake_path(GET source STEM LAST_ONLY name)
    if (NOT name MATCHES "^(.+)\\.(vs|ps|cs|gs|hs|ds)$")
        message(FATAL_ERROR "Shader filename must be <name>.<stage>.hlsl: ${source}")
    endif ()

    set(symbol "${CMAKE_MATCH_1}_${CMAKE_MATCH_2}")
    set(stage "${CMAKE_MATCH_2}")
    set(output "${output_dir}/${symbol}.hpp")
    add_custom_command(
        OUTPUT "${output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${output_dir}"
        COMMAND nori_dxc
            -E main -T "${stage}_6_0" -HV 2021 -WX
            -I "${PROJECT_SOURCE_DIR}/include"
            "$<$<CONFIG:Debug>:-Od>"
            "$<$<CONFIG:Debug>:-Zi>"
            "$<$<CONFIG:Debug>:-Qembed_debug>"
            "$<$<NOT:$<CONFIG:Debug>>:-O3>"
            -Fh "${output}" -Vn "${symbol}" "${source}"
        DEPENDS "${source}" ${ARGN} nori_dxc
        VERBATIM
    )
    target_sources(${target} PRIVATE "${output}")
endfunction ()
