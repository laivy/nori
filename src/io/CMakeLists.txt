add_library(nori_io STATIC)
add_library(nori::io ALIAS nori_io)

target_sources(nori_io
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${PROJECT_SOURCE_DIR}/include
        FILES
            ${PROJECT_SOURCE_DIR}/include/nori/io.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/io/acceptor.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/io/awaitable.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/io/co_spawn.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/io/connect.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/io/connection.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/io/context.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/io/endpoint.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/io/error_code.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/io/packet.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/io/post.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/io/steady_timer.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/io/strand.hpp
            ${PROJECT_SOURCE_DIR}/include/nori/io/task_handle.hpp
    PRIVATE
        acceptor.cpp
        acceptor.hpp
        awaitable.cpp
        co_spawn.cpp
        connect.cpp
        connection.cpp
        connection.hpp
        context.cpp
        context.hpp
        coroutine_entry.cpp
        coroutine_entry.hpp
        error.cpp
        error.hpp
        executor.cpp
        executor.hpp
        io_operation.cpp
        io_operation.hpp
        packet.cpp
        registry.cpp
        registry.hpp
        steady_timer.cpp
        steady_timer.hpp
        strand.cpp
        strand.hpp
)

nori_configure_target(nori_io)
target_link_libraries(nori_io
    PUBLIC
        nori::core
        Threads::Threads
    PRIVATE
        "$<BUILD_LOCAL_INTERFACE:nori::asio>"
)

if (WIN32)
    target_link_libraries(nori_io PRIVATE mswsock ws2_32)
endif ()

set_target_properties(nori_io PROPERTIES EXPORT_NAME io)
