# MIT License

# Copyright (c) 2022 Gothic Multiplayer Team.

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

# For git metadata in C++.
FetchContent_Declare(
  cmake_git_version_tracking
  GIT_REPOSITORY https://github.com/andrew-hardin/cmake-git-version-tracking.git
  GIT_TAG 6c0cb87edd029ddfb403a8e24577c144a03605a6)
FetchContent_MakeAvailable(cmake_git_version_tracking)

set(CLIENT_SOURCES
    Button.cpp
    CActiveAniID.cpp
    CAnimMenu.cpp
    CChat.cpp
    CConfig.cpp
    CGmpClient.cpp
    CHeroClass.cpp
    CIngame.cpp
    CInterpolatePos.cpp
    CInventory.cpp
    CLanguage.cpp
    CLocalPlayer.cpp
    CMainMenu.cpp
    CMap.cpp
    CMenu.cpp
    CObjectMenu.cpp
    CObservation.cpp
    CPlayer.cpp
    CPlayerList.cpp
    CSelectClass.cpp
    CServerList.cpp
    CShrinker.cpp
    CSpawnPoint.cpp
    CSyncFuncs.cpp
    CWatch.cpp
    ExceptionHandler.cpp
    ExtendedServerList.cpp
    Font.cpp
    GMPSplash.cpp
    GMP_Client.cpp
    HooksManager.cpp
    Interface.cpp
    keyboard.cpp
    Mod.cpp
    Network.cpp
    HTTPDownloader.cpp
    patch.cpp
    Table.cpp
    WorldBuilder/CBuilder.cpp
    WorldBuilder/load.cpp
    WorldBuilder/save.cpp)

add_library(GMP SHARED ${CLIENT_SOURCES})
target_link_libraries(
  GMP
  PRIVATE common
          g2api
          ${InjectMage}
          ${BugTrap}
          SharedLib
          wsock32
          ws2_32
          Iphlpapi
          httplib
          spdlog::spdlog
          cmake_git_version_tracking
          SDL3
          dylib
          zNetInterface
          pugixml
          Client.Voice)
target_include_directories(
  GMP PRIVATE ${InjectMage_INCLUDE_DIR} ${BugTrap_INCLUDE_DIR}
              ${CPP_HTTPLIB_INCLUDE_DIRS})

# Automatically copies GMP client DLL to the directories specified in
# GMP_DLL_OUTPUT_DIR environment variable after the build. The variable can
# contain multiple directories separated by a ';' character. For example,
# "D:/Program Files (x86)/JoWooD/Gothic2_First/System/;D:/Program Files
# (x86)/JoWooD/Gothic2_Second/System/"
set(GOTHIC_2_SYSTEM_DIRECTORY_LIST $ENV{GMP_DLL_OUTPUT_DIR})

if(GOTHIC_2_SYSTEM_DIRECTORY_LIST)
  foreach(GOTHIC_2_SYSTEM_DIRECTORY IN LISTS GOTHIC_2_SYSTEM_DIRECTORY_LIST)
    add_custom_command(
      TARGET GMP
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:GMP>
              ${GOTHIC_2_SYSTEM_DIRECTORY}/GMP.dll
      COMMENT "Copying GMP.dll to ${GOTHIC_2_SYSTEM_DIRECTORY}\n")

    add_custom_command(TARGET GMP POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_SOURCE_DIR}/thirdparty/SDL3/lib/SDL3.dll"
          ${GOTHIC_2_SYSTEM_DIRECTORY}/SDL3.dll
      COMMENT "Copying SDL3.dll to ${GOTHIC_2_SYSTEM_DIRECTORY} directory\n")
  endforeach()
endif()

add_subdirectory(g2api)
add_subdirectory(znet)
add_subdirectory(znet_rak)
add_subdirectory(Voice)
