/*
MIT License

Copyright (c) 2025 Gothic Multiplayer Team.

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

namespace menu {

// Forward declaration
class MenuContext;

/**
 * @brief Result of a state update
 */
enum class StateResult {
  Continue,  // Continue running this state
  Exit       // Exit the state machine (call OnExit and shutdown)
};

/**
 * @brief Base interface for all menu states
 *
 * Each menu state (main menu, server list, options, etc.) should implement
 * this interface. The state is responsible for handling its own rendering,
 * input processing, and determining when to transition to another state.
 */
class MenuState {
public:
  virtual ~MenuState() = default;

  /**
   * @brief Called once when entering this state
   *
   * Use this to initialize resources, setup camera, play music, etc.
   */
  virtual void OnEnter() = 0;

  /**
   * @brief Called once when leaving this state
   *
   * Use this to cleanup resources, save config, etc.
   */
  virtual void OnExit() = 0;

  /**
   * @brief Main update loop - called every frame
   *
   * This should handle:
   * - Rendering the menu
   * - Processing input
   * - Updating animations/time-based logic
   *
   * @return StateResult::Continue to keep running, StateResult::Exit to shutdown
   */
  virtual StateResult Update() = 0;

  /**
   * @brief Check if this state should transition to another state
   *
   * @return Pointer to new state if transition should occur, nullptr otherwise
   *
   * The caller takes ownership of the returned pointer.
   * Return nullptr to stay in the current state.
   */
  virtual MenuState* CheckTransition() = 0;

  /**
   * @brief Get a debug name for this state (useful for logging)
   */
  virtual const char* GetStateName() const = 0;
};

}  // namespace menu
