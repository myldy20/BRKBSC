# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.20)
project(brkbsc VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BRKBSC_BUILD_APP "Build the SDL2 application" ON)
option(BRKBSC_BUILD_TESTS "Build core tests" ON)
option(BRKBSC_FETCH_SDL2 "Build a private SDL2 copy" OFF)

function(brkbsc_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
    endif()
endfunction()

add_library(brkbsc_core
    src/pitch.cpp
    src/phrase.cpp
    src/composer.cpp
)
target_include_directories(brkbsc_core PUBLIC src)
brkbsc_enable_warnings(brkbsc_core)

if(BRKBSC_BUILD_APP)
    set(BRKBSC_SDL_TARGET "")
    if(BRKBSC_FETCH_SDL2)
        include(FetchContent)
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
        set(SDL_TEST OFF CACHE BOOL "" FORCE)
        set(SDL_TESTS OFF CACHE BOOL "" FORCE)
        set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            SDL2
            URL https://github.com/libsdl-org/SDL/archive/refs/tags/release-2.30.12.tar.gz
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable(SDL2)
        set(BRKBSC_SDL_TARGET SDL2::SDL2-static)
    else()
        find_package(SDL2 CONFIG QUIET)
        if(TARGET SDL2::SDL2-static)
            set(BRKBSC_SDL_TARGET SDL2::SDL2-static)
        elseif(TARGET SDL2::SDL2)
            set(BRKBSC_SDL_TARGET SDL2::SDL2)
        else()
            find_package(PkgConfig REQUIRED)
            pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
            set(BRKBSC_SDL_TARGET PkgConfig::SDL2)
        endif()
    endif()

    if(APPLE)
        add_executable(brkbsc MACOSX_BUNDLE src/main.cpp src/audio_runtime.cpp src/ui.cpp)
        set_target_properties(brkbsc PROPERTIES
            OUTPUT_NAME "BRKBSC"
            MACOSX_BUNDLE_BUNDLE_NAME "BRKBSC"
            MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.myldy20.brkbsc"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in"
        )
    else()
        add_executable(brkbsc src/main.cpp src/audio_runtime.cpp src/ui.cpp)
        set_target_properties(brkbsc PROPERTIES OUTPUT_NAME "brkbsc")
    endif()

    target_link_libraries(brkbsc PRIVATE brkbsc_core ${BRKBSC_SDL_TARGET})
    target_compile_definitions(brkbsc PRIVATE SDL_MAIN_HANDLED)
    brkbsc_enable_warnings(brkbsc)
    install(TARGETS brkbsc BUNDLE DESTINATION . RUNTIME DESTINATION bin)
endif()

if(BRKBSC_BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_executable(brkbsc_core_tests tests/core_tests.cpp)
    target_link_libraries(brkbsc_core_tests PRIVATE brkbsc_core)
    brkbsc_enable_warnings(brkbsc_core_tests)
    add_test(NAME brkbsc_core_tests COMMAND brkbsc_core_tests)
endif()
