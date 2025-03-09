
# MIT License

# Copyright (c) 2022 Gothic Multiplayer Team (pampi, skejt23, mecio)

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

cmake_minimum_required(VERSION 3.2)

include(FetchContent)

cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "")
endif()

set(VCPKG_OVERLAY_TRIPLETS "${CMAKE_SOURCE_DIR}/triplets")

project(GothicMultiplayer VERSION 0.2 DESCRIPTION "Gothic Multiplayer" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

FetchContent_Declare(
    dylib
    GIT_REPOSITORY "https://github.com/martin-olivier/dylib"
    GIT_TAG        "v2.1.0"
)

FetchContent_MakeAvailable(dylib)

find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(PugiXML CONFIG REQUIRED)
find_package(Lua CONFIG REQUIRED)
find_package(sol2 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_compile_definitions(SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
if (MSVC)
  add_compile_options(/utf-8)
endif()

add_subdirectory(common)
add_subdirectory(Shared)
add_subdirectory(GMP_Serv)
add_subdirectory(thirdparty)

if (WIN32)
    find_package(SDL2 CONFIG REQUIRED)
    find_library(InjectMage InjectMage.lib PATHS ./InjectMage)
    find_path(
        InjectMage_INCLUDE_DIR
        NAMES "InjectMage.h"
        PATHS "./InjectMage"
    )

    find_path(
        DX7SDK_INCLUDE_DIR
        NAMES "d3d.h" "ddraw.h" "dinput.h"
        PATHS "./GMP_Client/dx7sdk/inc"
        REQUIRED
    )

    find_library(BugTrap BugTrap.lib PATHS ./thirdparty/BugTrap)
    find_path(
        BugTrap_INCLUDE_DIR
        NAMES "BugTrap.h"
        PATHS "./thirdparty/BugTrap"
    )

    add_subdirectory(GMP_Client)
    
    enable_testing()

    add_subdirectory(tests)
endif()