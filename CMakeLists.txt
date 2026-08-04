cmake_minimum_required(VERSION 3.16)
project(GL_Library LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(X11 REQUIRED)

add_library(GL SHARED
    src/global.cpp
    src/lib.cpp
    src/framebuffer.cpp
    src/fbdev/fbdev.cpp
    src/maths.cpp
    src/displayLists.cpp
    src/functions/function.cpp
    src/shim.cpp
    src/display/x11_display.cpp
)

target_include_directories(GL PRIVATE
    include
    src
    src/render
    src/functions
    src/include
    src/fbdev
    src/display
    src/wm
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

target_link_libraries(GL PRIVATE X11::X11 X11::Xext ${CMAKE_DL_LIBS})
set_target_properties(GL PROPERTIES OUTPUT_NAME "GL")

add_executable(pixsoftwm
    src/wm/main.cpp
    src/wm/wm.cpp
    src/fbdev/fbdev.cpp
)

target_include_directories(pixsoftwm PRIVATE
    src
    src/include
    src/fbdev
    src/display
    src/wm
)

target_link_libraries(pixsoftwm PRIVATE X11::X11)

# WM needs the atom name constant from the display header (header-only usage).
target_compile_features(pixsoftwm PRIVATE cxx_std_23)
