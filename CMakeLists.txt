cmake_minimum_required(VERSION 3.20)
project(kusart)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options("/utf-8" "/Zc:__cplusplus")
endif()

set(KUSART_INCLUDE_DIRS             "${CMAKE_CURRENT_SOURCE_DIR}/include")
file(GLOB_RECURSE KUSART_SOURCES    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc")

add_library(kusart STATIC ${KUSART_SOURCES})
target_include_directories(kusart PRIVATE ${KUSART_INCLUDE_DIRS})

enable_testing()
add_subdirectory("test")