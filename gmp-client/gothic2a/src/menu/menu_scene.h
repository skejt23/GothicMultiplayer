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

#include "ZenGin/zGothicAPI.h"
#include "frame_rate_limiter.h"

namespace menu {

/**
 * @brief Manages menu scene animations (weapon rotation, time advancement)
 *
 * This class provides FPS-independent animations for the menu background scene.
 * It uses a single frame rate limiter to ensure consistent behavior at any framerate.
 */
class MenuScene {
public:
  /**
   * @brief Construct a new Menu Scene
   *
   * @param game Pointer to the game instance
   * @param weapon Pointer to the title weapon item (can be nullptr)
   * @param targetFps Target frame rate for animations (default: 60 FPS)
   */
  MenuScene(oCGame* game, oCItem* weapon, int targetFps = 60) : game_(game), weapon_(weapon), frame_limiter_(targetFps) {
  }

  /**
   * @brief Update the menu scene (weapon rotation and time)
   *
   * Call this every frame from the menu state's Update() method.
   * The actual updates will only happen at the target FPS rate.
   */
  void Update() {
    if (frame_limiter_.ShouldUpdate()) {
      UpdateWeaponRotation();
      UpdateGameTime();
    }
  }

  /**
   * @brief Set the weapon to animate
   *
   * @param weapon Pointer to the weapon item (can be nullptr to disable rotation)
   */
  void SetWeapon(oCItem* weapon) {
    weapon_ = weapon;
  }

  /**
   * @brief Get the current weapon
   *
   * @return Pointer to the weapon item (may be nullptr)
   */
  oCItem* GetWeapon() const {
    return weapon_;
  }

  /**
   * @brief Reset the frame limiter
   *
   * Useful when entering/exiting states to avoid sudden jumps
   */
  void Reset() {
    frame_limiter_.Reset();
  }

private:
  void UpdateWeaponRotation() {
    if (weapon_) {
      weapon_->RotateWorldX(0.6f);
    }
  }

  void UpdateGameTime() {
    if (game_ && game_->GetWorldTimer()) {
      int hour, minute;
      game_->GetWorldTimer()->GetTime(hour, minute);

      if (minute >= 59) {
        hour++;
      }
      minute++;

      game_->GetWorldTimer()->SetTime(hour, minute);
    }
  }

  oCGame* game_;
  oCItem* weapon_;
  FrameRateLimiter frame_limiter_;
};

}  // namespace menu
