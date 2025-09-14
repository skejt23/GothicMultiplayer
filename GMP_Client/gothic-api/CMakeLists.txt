cmake_minimum_required(VERSION 3.21)

option(GOTHIC_API_G1  "This option compiles Gothic API for Gothic I. By default this option is set to OFF" OFF)
option(GOTHIC_API_G1A "This option compiles Gothic API for Gothic Sequel (Gothic I Addon). By default this option is set to OFF" OFF)
option(GOTHIC_API_G2  "This option compiles Gothic API for Gothic II Classic. By default this option is set to OFF" OFF)
option(GOTHIC_API_G2A "This option compiles Gothic API for Gothic II Night of the Raven. By default this option is set to OFF" OFF)

project(GothicAPI)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PLATFORMS_COUNT 0)

if(GOTHIC_API_G1)
    math(EXPR PLATFORMS_COUNT "${PLATFORMS_COUNT} + 1")
endif()

if(GOTHIC_API_G1A)
    math(EXPR PLATFORMS_COUNT "${PLATFORMS_COUNT} + 1")
endif()

if(GOTHIC_API_G2)
    math(EXPR PLATFORMS_COUNT "${PLATFORMS_COUNT} + 1")
endif()

if(GOTHIC_API_G2A)
    math(EXPR PLATFORMS_COUNT "${PLATFORMS_COUNT} + 1")
endif()

add_library(gothic_api INTERFACE)

target_include_directories(gothic_api
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/ZenGin/Gothic_UserAPI"
)

target_link_directories(gothic_api
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/"
)

target_sources(gothic_api
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/ZenGin/zGothicAPI.cpp"
)

target_compile_definitions(gothic_api
    INTERFACE
        $<$<BOOL:${GOTHIC_API_G1}>:__G1>
        $<$<BOOL:${GOTHIC_API_G1A}>:__G1A>
        $<$<BOOL:${GOTHIC_API_G2}>:__G2>
        $<$<BOOL:${GOTHIC_API_G2A}>:__G2A>
)

if (PLATFORMS_COUNT GREATER_EQUAL 2)
    target_compile_definitions(gothic_api INTERFACE UNION_MULTIPLATFORM)
endif()