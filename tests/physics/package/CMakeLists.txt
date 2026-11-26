cmake_minimum_required(VERSION 4.2)
project(nori_physics_consumer LANGUAGES CXX)

find_package(nori CONFIG REQUIRED COMPONENTS physics)

add_executable(physics_consumer main.cpp)
target_link_libraries(physics_consumer PRIVATE nori::physics)

enable_testing()
add_test(NAME physics_consumer COMMAND physics_consumer)
