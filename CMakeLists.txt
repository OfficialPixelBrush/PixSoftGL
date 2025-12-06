cmake_minimum_required(VERSION 3.16)
project(GL_Library LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(SDL3 REQUIRED)

add_library(GL SHARED
    src/global.cpp
    src/lib.cpp
    src/sdl.cpp
    src/render.cpp
    src/maths.cpp
    src/displayLists.cpp
    src/functions/function.cpp
)

target_include_directories(GL PRIVATE include)

target_link_libraries(GL PRIVATE SDL3::SDL3)

set_target_properties(GL PROPERTIES OUTPUT_NAME "GL")
