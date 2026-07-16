set(gtest_force_shared_crt ON)
set(INSTALL_GTEST OFF)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

if (TARGET nori::resource)
    add_executable(nori_resource_tests resource_tests.cpp)
    nori_configure_target(nori_resource_tests)
    target_link_libraries(nori_resource_tests PRIVATE nori::resource GTest::gtest_main)
    gtest_discover_tests(nori_resource_tests)
endif ()
