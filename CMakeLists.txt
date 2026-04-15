cmake_minimum_required(VERSION 3.16)
project(OxygenClicker VERSION 2.0.0 LANGUAGES CXX)
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

set(SOURCES
    src/main.cpp
    src/core/Clicker.cpp
    src/input/HotkeyManager.cpp
    src/ui/UI.cpp
    ${IMGUI_SRC})

if(WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Windows")
    list(APPEND SOURCES
        src/platform/windows/PlatformWindows.cpp
        src/input/windows/HotkeyWindows.cpp)

    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(SDL2 REQUIRED sdl2)
    else()
        find_package(SDL2 REQUIRED)
    endif()

    add_executable(OxygenClicker WIN32 ${SOURCES})
    target_link_libraries(OxygenClicker PRIVATE
        ${SDL2_LIBRARIES}
        opengl32
        -static-libgcc -static-libstdc++ -static -lpthread)
else()
    list(APPEND SOURCES
        src/platform/linux/PlatformLinux.cpp
        src/input/linux/HotkeyLinux.cpp)

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    find_package(OpenGL REQUIRED)

    add_executable(OxygenClicker ${SOURCES})
    target_link_libraries(OxygenClicker PRIVATE
        ${SDL2_LIBRARIES}
        OpenGL::GL
        X11 Xtst dl pthread)
endif()

target_include_directories(OxygenClicker PRIVATE
    include
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${SDL2_INCLUDE_DIRS})

target_compile_options(OxygenClicker PRIVATE -O2)

install(TARGETS OxygenClicker DESTINATION bin)
