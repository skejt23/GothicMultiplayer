cmake_minimum_required(VERSION 3.10)

project(chillout CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED on)

set(GTEST_SOURCE_DIR ${PROJECT_SOURCE_DIR}/vendors/gtest)
set(CHILLOUT_SOURCE_DIR ${PROJECT_SOURCE_DIR}/src/chillout)

if (MSVC)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING")
   set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif (MSVC)

#add_definitions(-DGTEST_HAS_SEH=0)

add_subdirectory(src/chillout)

option (BUILD_TESTING "Build the testing tree." OFF)
# Only build all if we are the top-level project
# Allows this to be used by super projects with `add_subdirectory`
if (BUILD_TESTING OR (PROJECT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR))
    enable_testing()

    add_subdirectory(vendors/gtest)
    add_subdirectory(src/tests)
endif()
