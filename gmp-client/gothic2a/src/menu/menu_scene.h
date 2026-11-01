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

namespace menu {

struct MenuWeaponBaseline {
  zVEC3 camera_position;
  float camera_pitch = 0.0f;
  float camera_yaw = 0.0f;
  zVEC3 weapon_position;
  float weapon_yaw = 0.0f;
};

struct MenuSceneSettings {
  zVEC3 camera_position;
  float camera_pitch = 0.0f;
  float camera_yaw = 0.0f;
  const char* weapon_visual_name = nullptr;
  bool show_weapon = true;
  bool enable_timelapse = false;
  bool freeze_time = false;
  const MenuWeaponBaseline* weapon_baseline = nullptr;
  float camera_roll = 0.0f;
};

class MenuScene {
public:
  virtual ~MenuScene() = default;

  virtual MenuSceneSettings GetSettings() const = 0;
  virtual bool Start() {
    return true;
  }
  virtual void Stop() {
  }
  virtual void Update(float delta_time) = 0;
  virtual void Render() {
  }
  virtual bool IsHealthy() const {
    return true;
  }
  virtual void SetWeapon(zCVob* /*weapon*/) {
  }
};

}  // namespace menu
