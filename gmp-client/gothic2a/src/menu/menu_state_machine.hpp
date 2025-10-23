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

#include <spdlog/spdlog.h>

#include <memory>

#include "menu/states/menu_state.hpp"

namespace menu {

/**
 * @brief State machine that manages menu state transitions
 *
 * This class is responsible for:
 * - Holding the current active state
 * - Transitioning between states
 * - Calling state lifecycle methods (OnEnter/OnExit)
 * - Updating the active state each frame
 */
class MenuStateMachine {
private:
  std::unique_ptr<MenuState> currentState_;

public:
  MenuStateMachine() = default;

  /**
   * @brief Transition to a new state
   *
   * This will:
   * 1. Call OnExit() on the current state (if any)
   * 2. Switch to the new state
   * 3. Call OnEnter() on the new state
   *
   * @param newState The state to transition to (ownership is transferred)
   */
  void SetState(std::unique_ptr<MenuState> newState) {
    if (currentState_) {
      SPDLOG_INFO("Menu state transition: {} -> {}", currentState_->GetStateName(), newState ? newState->GetStateName() : "nullptr");
      currentState_->OnExit();
    } else if (newState) {
      SPDLOG_INFO("Menu state initialized: {}", newState->GetStateName());
    }

    currentState_ = std::move(newState);

    if (currentState_) {
      currentState_->OnEnter();
    }
  }

  /**
   * @brief Update the current state
   *
   * This should be called every frame. It will:
   * 1. Call Update() on the current state
   * 2. Check the result - if Exit, shutdown the state machine
   * 3. Check if a state transition should occur
   * 4. Perform the transition if needed
   *
   * @return true if the state machine should continue, false if it should shut down
   */
  bool Update() {
    if (!currentState_) {
      return false;  // No state means we should shut down
    }

    // Update current state and check if it wants to exit
    StateResult result = currentState_->Update();

    if (result == StateResult::Exit) {
      // State requested shutdown - call OnExit and clear
      SPDLOG_INFO("State {} requested exit", currentState_->GetStateName());
      currentState_->OnExit();
      currentState_.reset();
      return false;
    }

    // Check for state transition
    MenuState* nextState = currentState_->CheckTransition();

    if (nextState != nullptr) {
      // Normal transition to a new state
      SetState(std::unique_ptr<MenuState>(nextState));
    }
    // nullptr means no transition - stay in current state
    return true;
  }

  /**
   * @brief Get the current state (for debugging/inspection)
   *
   * @return Pointer to current state, or nullptr if no state is active
   */
  MenuState* GetCurrentState() const {
    return currentState_.get();
  }

  /**
   * @brief Check if a state is currently active
   */
  bool HasActiveState() const {
    return currentState_ != nullptr;
  }

  /**
   * @brief Clear the current state without transitioning
   *
   * This will call OnExit() and leave the state machine empty.
   * Useful for cleanup on shutdown.
   */
  void Clear() {
    if (currentState_) {
      SPDLOG_INFO("Clearing menu state: {}", currentState_->GetStateName());
      currentState_->OnExit();
      currentState_.reset();
    }
  }
};

}  // namespace menu
