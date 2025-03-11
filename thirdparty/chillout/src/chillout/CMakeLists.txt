cmake_minimum_required(VERSION 3.10)

project(chillout CXX)

set(SOURCES chillout.h chillout.cpp defines.h common/common.h common/common.cpp)

if(WIN32)
  set(SOURCES ${SOURCES} windows/StackWalker.h windows/StackWalker.cpp windows/windowscrashhandler.h windows/windowscrashhandler.cpp)
  add_definitions(-D_UNICODE -D_MBCS -DUNICODE)
else()
  set(SOURCES ${SOURCES} posix/posixcrashhandler.h posix/posixcrashhandler.cpp)
endif()

add_library(chillout SHARED STATIC ${SOURCES})
target_include_directories(chillout PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

set_property(TARGET chillout PROPERTY POSITION_INDEPENDENT_CODE ON)

if(UNIX)
  target_link_libraries(chillout dl)
endif()

if (WIN32)
  target_link_libraries(chillout DbgHelp)
endif()

install(TARGETS chillout DESTINATION lib)
