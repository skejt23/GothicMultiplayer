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

#include "menu/menu_scene.h"
#include "frame_rate_limiter.h"

namespace menu::scenes {

/**
 * @brief Basic menu scene (weapon rotation, time advancement, and camera position)
 *
 * This class provides FPS-independent animations for the menu background scene.
 * It uses a single frame rate limiter to ensure consistent behavior at any framerate.
 */
class DefaultMenuScene : public MenuScene {
public:
  DefaultMenuScene(oCGame* game,
                   zCVob* weapon,
                   MenuSceneSettings settings,
                   int targetFps = 60)
      : game_(game),
        weapon_(weapon),
        settings_(settings),
        frame_limiter_(targetFps) {
  }

  MenuSceneSettings GetSettings() const override {
    return settings_;
  }

  void Update(float /*delta_time*/) override {
    if (frame_limiter_.ShouldUpdate()) {
      UpdateWeaponRotation();
      UpdateGameTime();
    }
  }

  void SetWeapon(zCVob* weapon) override {
    weapon_ = weapon;
  }

  bool Start() override {
    frame_limiter_.Reset();
    has_frozen_time_ = false;
    return true;
  }

private:
  void UpdateWeaponRotation() {
    if (weapon_) {
      weapon_->RotateLocalX(0.6f);
    }
  }

  void UpdateGameTime() {
    if (settings_.freeze_time) {
      FreezeTime();
      return;
    }

    if (!settings_.enable_timelapse) {
      return;
    }

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

  void FreezeTime() {
    if (!game_ || !game_->GetWorldTimer()) {
      return;
    }

    if (!has_frozen_time_) {
      game_->GetWorldTimer()->GetTime(frozen_hour_, frozen_minute_);
      has_frozen_time_ = true;
    }

    game_->GetWorldTimer()->SetTime(frozen_hour_, frozen_minute_);
  }

  oCGame* game_;
  zCVob* weapon_;
  MenuSceneSettings settings_;
  bool has_frozen_time_ = false;
  int frozen_hour_ = 0;
  int frozen_minute_ = 0;
  FrameRateLimiter frame_limiter_;
};

}  // namespace menu::scenes
