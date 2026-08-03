cmake_minimum_required(VERSION 3.16)
project(GL_Library LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(GL SHARED
    src/global.cpp
    src/lib.cpp
    src/framebuffer.cpp
    src/fbdev/fbdev.cpp
    src/maths.cpp
    src/displayLists.cpp
    src/functions/function.cpp
    src/shim.cpp
)

target_include_directories(GL PRIVATE include
    src/
    src/render
    src/functions
    src/include
    src/fbdev
)

set(RENDERTARGET "HARDWARE" CACHE STRING "Renderer target: SOFTWARE or HARDWARE")
set_property(CACHE RENDERTARGET PROPERTY STRINGS SOFTWARE HARDWARE)

if(RENDERTARGET STREQUAL "SOFTWARE")
    target_compile_definitions(GL PRIVATE RENDERTARGET_SOFTWARE)
    target_sources(GL PRIVATE src/render/renderSoftware.cpp)
else()
    target_compile_definitions(GL PRIVATE RENDERTARGET_HARDWARE)
    target_sources(GL PRIVATE src/render/renderHardware.cpp)
endif()


target_link_libraries(GL PRIVATE X11 dl)

set_target_properties(GL PROPERTIES OUTPUT_NAME "GL")
