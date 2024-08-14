cmake_minimum_required(VERSION 3.19)

set(DOPBVH_VERSION "0.0.1")
project(dopbvh VERSION ${DOPBVH_VERSION} LANGUAGES C CXX DESCRIPTION "graphics shenanigans")

find_package(Threads REQUIRED)

add_subdirectory(extern)

add_subdirectory(support)
add_subdirectory(src)
