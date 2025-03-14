# MIT License

# Copyright (c) 2023 Gothic Multiplayer.

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

set(SERVER_SOURCES
    character_definition.cpp
    config.cpp
    gothic_clock.cpp
    game_server.cpp
    platform_depend.cpp
    HTTPServer.cpp
    Script.cpp
    Lua/spdlog_bind.cpp
    Lua/event_bind.cpp
    Lua/function_bind.cpp)

add_library(ServerLib STATIC ${SERVER_SOURCES})
target_link_libraries(
  ServerLib
  PRIVATE SharedLib
          spdlog::spdlog
          fmt::fmt
          httplib
          pugixml
          dylib
          ${LUA_LIBRARIES}
          sol2
          cmake_git_version_tracking
          nlohmann_json::nlohmann_json
  PUBLIC common zNetServerInterface)
target_include_directories(
  ServerLib
  PRIVATE ${CPP_HTTPLIB_INCLUDE_DIRS} ${LUA_INCLUDE_DIR} sol2
  PUBLIC .)

add_executable(gmp_server main.cpp)
target_link_libraries(gmp_server PRIVATE ServerLib chillout spdlog::spdlog)

add_subdirectory(znet)
add_subdirectory(znet_rak)
