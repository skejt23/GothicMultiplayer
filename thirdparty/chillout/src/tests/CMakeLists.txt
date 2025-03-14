cmake_minimum_required(VERSION 3.10)

project(chillout_tests_project CXX)

set(SOURCES tests_main.cpp tests_source.h tests_source.cpp tests_crashes.cpp tests_recovery.cpp gtest_extensions.h gtest_extensions.cpp)

add_executable(chillout_test ${SOURCES})

FILE(TO_NATIVE_PATH ${CMAKE_CURRENT_LIST_DIR} CRASH_DUMPS_ROOT)

if(WIN32)
  STRING(REGEX REPLACE "\\\\" "\\\\\\\\" CRASH_DUMPS_ROOT ${CRASH_DUMPS_ROOT} )
endif()

add_definitions(-DCRASHDUMPSDIR=${CRASH_DUMPS_ROOT})
message("Crash dumps root: ${CRASH_DUMPS_ROOT}")

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -rdynamic")
endif()

target_include_directories(chillout_test PRIVATE ${CHILLOUT_SOURCE_DIR})
target_include_directories(chillout_test PRIVATE ${GTEST_SOURCE_DIR}/googletest/include)
target_include_directories(chillout_test PRIVATE ${GTEST_SOURCE_DIR}/include)

# GTest needs threading support

if(WIN32)
  # stackwalker stuff
  target_link_libraries(chillout_test Advapi32 DbgHelp)
endif()

target_link_libraries(chillout_test gtest_main)
target_link_libraries(chillout_test chillout)

add_test(NAME ChilloutTest COMMAND chillout_test)
