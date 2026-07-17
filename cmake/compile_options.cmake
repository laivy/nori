add_library(nori_compile_options INTERFACE)
target_compile_features(nori_compile_options INTERFACE cxx_std_23)

if (MSVC)
    target_compile_options(nori_compile_options INTERFACE /EHsc /utf-8 /W4)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
    target_compile_options(nori_compile_options INTERFACE
        -finput-charset=UTF-8
        -fexec-charset=UTF-8
        -Wall
    )
endif ()

function(nori_configure_target target)
    get_target_property(compile_features nori_compile_options INTERFACE_COMPILE_FEATURES)
    get_target_property(compile_options nori_compile_options INTERFACE_COMPILE_OPTIONS)

    target_compile_features(${target} PRIVATE ${compile_features})
    if (compile_options)
        target_compile_options(${target} PRIVATE ${compile_options})
    endif ()
    set_property(TARGET ${target} PROPERTY CXX_EXTENSIONS OFF)
endfunction ()
