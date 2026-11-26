# 내부 언어 및 컴파일러 설정
add_library(nori_compile_options INTERFACE)

target_compile_options(nori_compile_options INTERFACE
    $<$<CXX_COMPILER_ID:MSVC>:/EHsc>
    $<$<CXX_COMPILER_ID:MSVC>:/GR->
    $<$<CXX_COMPILER_ID:MSVC>:/permissive->
    $<$<CXX_COMPILER_ID:MSVC>:/Zc:inline>
    $<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus>
    $<$<CXX_COMPILER_ID:GNU>:-finput-charset=UTF-8>
    $<$<CXX_COMPILER_ID:GNU>:-fexec-charset=UTF-8>
    $<$<CXX_COMPILER_ID:GNU>:-fno-rtti>
    $<$<AND:$<CXX_COMPILER_ID:GNU>,$<CONFIG:Release>>:-g>
)

target_link_options(nori_compile_options INTERFACE
    "$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/DEBUG:FULL;/INCREMENTAL:NO;/OPT:REF;/OPT:ICF>"
)

function(nori_configure_target target)
    target_compile_features("${target}" PUBLIC cxx_std_23)
    target_compile_options("${target}"
        PUBLIC
            $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
        PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:/W4>
            "$<$<CXX_COMPILER_ID:GNU>:-Wall;-Wextra;-Wpedantic;-Wno-missing-field-initializers>"
    )
    if (WIN32)
        target_compile_definitions("${target}"
            PUBLIC
                NOMINMAX
            PRIVATE
                _UNICODE
                _WIN32_WINNT=0x0A00 # Windows 10 1803 이상
                UNICODE
                WIN32_LEAN_AND_MEAN
        )
    endif ()
    target_link_libraries("${target}" PRIVATE
        "$<BUILD_LOCAL_INTERFACE:nori_compile_options>"
    )
    set_target_properties("${target}" PROPERTIES
        COMPILE_WARNING_AS_ERROR ON
        CXX_EXTENSIONS OFF
    )
endfunction ()
