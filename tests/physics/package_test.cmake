set(test_directory "${test_binary_dir}/physics-package-test/${test_config}")
file(REMOVE_RECURSE "${test_directory}")

execute_process(COMMAND "${CMAKE_COMMAND}" --install "${test_binary_dir}"
    --config "${test_config}" --prefix "${test_directory}/install"
    COMMAND_ERROR_IS_FATAL ANY)

set(generator_args -G "${test_generator}")
if (test_generator_platform)
    list(APPEND generator_args -A "${test_generator_platform}")
endif ()
if (test_generator_toolset)
    list(APPEND generator_args -T "${test_generator_toolset}")
endif ()

execute_process(COMMAND "${CMAKE_COMMAND}"
    -S "${test_source_dir}" -B "${test_directory}/build"
    ${generator_args}
    "-DCMAKE_CXX_COMPILER=${test_cxx_compiler}"
    "-DCMAKE_BUILD_TYPE=${test_config}"
    "-DCMAKE_PREFIX_PATH=${test_directory}/install"
    -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF
    -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF
    COMMAND_ERROR_IS_FATAL ANY)

execute_process(COMMAND "${CMAKE_COMMAND}" --build "${test_directory}/build"
    --config "${test_config}"
    COMMAND_ERROR_IS_FATAL ANY)

execute_process(COMMAND "${test_ctest_command}" --test-dir "${test_directory}/build"
    -C "${test_config}" --output-on-failure
    COMMAND_ERROR_IS_FATAL ANY)
