find_package(Threads REQUIRED)

FetchContent_MakeAvailable(boost)

add_library(nori_network STATIC)
add_library(nori::network ALIAS nori_network)
set_property(TARGET nori_network PROPERTY EXPORT_NAME network)
target_compile_features(nori_network PUBLIC cxx_std_23)
nori_configure_target(nori_network)

target_sources(nori_network
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${PROJECT_SOURCE_DIR}/include
        FILES
            ${PROJECT_SOURCE_DIR}/include/nori/network.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/network/client.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/network/packet.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/network/server.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/network/types.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/client.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/client_impl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/client_impl.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/connection.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io_context_pool.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/packet.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/server.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/server_impl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/server_impl.hpp
)

target_include_directories(nori_network
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include/nori/network
    SYSTEM PRIVATE
        ${boost_SOURCE_DIR}
)
target_link_libraries(nori_network PRIVATE Threads::Threads)

if (WIN32)
    target_compile_definitions(nori_network PRIVATE _WIN32_WINNT=0x0A00)
    target_link_libraries(nori_network PRIVATE mswsock ws2_32)
endif ()

install(TARGETS nori_network EXPORT nori_targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
