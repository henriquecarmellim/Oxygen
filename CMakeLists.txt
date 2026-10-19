cmake_minimum_required(VERSION 3.16)
project(OxygenClicker VERSION 2.1.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.6)
FetchContent_MakeAvailable(imgui)

set(IMGUI_SRC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

if(WIN32)
    add_executable(OxygenClicker WIN32
        src/main.cpp
        src/platform/windows/PlatformWindows.cpp
        ${IMGUI_SRC})
    # exe estático: não precisa de SDL2.dll nem das DLLs do MinGW
    target_link_libraries(OxygenClicker PRIVATE
        -static
        ${SDL2_STATIC_LDFLAGS}
        opengl32 winmm gdi32)
else()
    pkg_check_modules(X11 REQUIRED x11 xfixes)
    # LEGACY → libGL.so.1 (presente em toda distro); GLVND puxaria libOpenGL.so.0, que muitas não têm
    set(OpenGL_GL_PREFERENCE LEGACY)
    find_package(OpenGL REQUIRED)

    add_executable(OxygenClicker
        src/main.cpp
        src/platform/linux/PlatformLinux.cpp
        ${IMGUI_SRC})
    target_link_libraries(OxygenClicker PRIVATE
        ${SDL2_LIBRARIES}
        ${X11_LIBRARIES}
        OpenGL::GL
        dl pthread)
endif()

target_include_directories(OxygenClicker PRIVATE
    include
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${SDL2_INCLUDE_DIRS})
target_compile_options(OxygenClicker PRIVATE -O2 ${SDL2_CFLAGS_OTHER})
install(TARGETS OxygenClicker DESTINATION bin)
