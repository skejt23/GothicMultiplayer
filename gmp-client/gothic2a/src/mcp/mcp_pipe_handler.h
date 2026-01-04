/*
MIT License

Copyright (c) 2025 Gothic Multiplayer Team

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include <Windows.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <unordered_map>

namespace gmp {
namespace mcp {

using json = nlohmann::json;

/**
 * @brief Handler function type for MCP commands
 * @param params The parameters from the JSON request
 * @return JSON response to send back
 */
using CommandHandler = std::function<json(const json& params)>;

/**
 * @brief Game state enumeration for tracking loading and readiness
 */
enum class GameState {
  NotStarted,  // Game process not detected
  Starting,    // Process started, waiting for initialization
  Loading,     // World is loading (loading screen visible)
  InGame,      // Player is in-game and controllable
  Menu         // Player is in menu
};

/**
 * @brief Convert GameState to string for JSON serialization
 */
inline const char* GameStateToString(GameState state) {
  switch (state) {
    case GameState::NotStarted:
      return "not_started";
    case GameState::Starting:
      return "starting";
    case GameState::Loading:
      return "loading";
    case GameState::InGame:
      return "in_game";
    case GameState::Menu:
      return "menu";
    default:
      return "unknown";
  }
}

/**
 * @brief Named Pipe server for MCP communication
 *
 * This class creates a Named Pipe server that listens for commands from
 * the Python MCP server and executes them in the Gothic game context.
 */
class MCPPipeHandler {
public:
  static constexpr const char* kPipeName = R"(\\.\pipe\GothicMCP)";
  static constexpr DWORD kBufferSize = 65536;
  static constexpr DWORD kTimeout = 5000;

  /**
   * @brief Get the singleton instance
   */
  static MCPPipeHandler& Instance();

  /**
   * @brief Initialize and start the pipe server
   * @return true if started successfully
   */
  bool Start();

  /**
   * @brief Stop the pipe server
   */
  void Stop();

  /**
   * @brief Check if the server is running
   */
  bool IsRunning() const {
    return running_.load();
  }

  /**
   * @brief Register a command handler
   * @param method The method name to handle
   * @param handler The handler function
   */
  void RegisterHandler(const std::string& method, CommandHandler handler);

  /**
   * @brief Unregister a command handler
   * @param method The method name to unregister
   */
  void UnregisterHandler(const std::string& method);

  /**
   * @brief Register all default Gothic command handlers
   */
  void RegisterDefaultHandlers();

  /**
   * @brief Process a single message (called from game thread for thread safety)
   *
   * This should be called from the main game loop to ensure Gothic API
   * calls are made from the correct thread.
   */
  void ProcessPendingMessages();

  // =========================================================================
  // Game State Tracking
  // =========================================================================

  /**
   * @brief Get the current game state
   */
  GameState GetGameState() const {
    return game_state_.load();
  }

  /**
   * @brief Check if the game is ready (player is in-game and controllable)
   */
  bool IsGameReady() const {
    return game_state_.load() == GameState::InGame;
  }

  /**
   * @brief Called when the loading screen closes (world finished loading)
   *
   * This should be registered with HooksManager as HT_CLOSELOADSCREEN callback.
   */
  void OnLoadingComplete();

  /**
   * @brief Called on each render frame to update state
   */
  void OnRenderFrame();

  /**
   * @brief Get time since game became ready (in milliseconds)
   * @return Milliseconds since ready, or -1 if not ready
   */
  int64_t GetTimeSinceReady() const;

private:
  MCPPipeHandler() = default;
  ~MCPPipeHandler();

  MCPPipeHandler(const MCPPipeHandler&) = delete;
  MCPPipeHandler& operator=(const MCPPipeHandler&) = delete;

  /**
   * @brief Server thread main function
   */
  void ServerThread();

  /**
   * @brief Handle a single client connection
   * @param pipe The pipe handle
   */
  void HandleClient(HANDLE pipe);

  /**
   * @brief Process a JSON request and generate response
   * @param request The JSON request
   * @return JSON response
   */
  json ProcessRequest(const json& request);

  /**
   * @brief Update game state based on current Gothic state
   */
  void UpdateGameState();

  std::thread server_thread_;
  std::atomic<bool> running_{false};
  std::atomic<bool> stop_requested_{false};
  HANDLE pipe_handle_{INVALID_HANDLE_VALUE};
  HANDLE stop_event_{NULL};  // Event signaled when Stop() is called

  std::mutex handlers_mutex_;
  std::unordered_map<std::string, CommandHandler> handlers_;

  // Game state tracking
  std::atomic<GameState> game_state_{GameState::NotStarted};
  std::chrono::steady_clock::time_point ready_time_;
  std::atomic<bool> has_ready_time_{false};
};

}  // namespace mcp
}  // namespace gmp
