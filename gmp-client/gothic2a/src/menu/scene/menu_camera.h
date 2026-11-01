/*
MIT License

Copyright (c) 2026 Gothic Multiplayer Team.

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

// Temporarily controls the existing engine camera vob and restores its state.
class MenuCamera {
public:
  explicit MenuCamera(oCGame* game) : game_(game) {
  }
  ~MenuCamera();
  MenuCamera(const MenuCamera&) = delete;
  MenuCamera& operator=(const MenuCamera&) = delete;

  bool IsReady() const;
  bool Apply(const zVEC3& position, float pitch, float yaw, float roll = 0.0f);
  void SetPosition(const zVEC3& position);
  void SetRotation(float pitch, float yaw, float roll = 0.0f);
  void LookAt(const zVEC3& target);
  bool IsBehindCamera(const zVEC3& position, float margin = 0.0f) const;
  bool Project(const zVEC3& position, float& x, float& y) const;
  zCVob* GetAnchor() const {
    return anchor_;
  }
  void Reset();

private:
  oCGame* game_;
  zCVob* anchor_ = nullptr;  // Retained engine camera, not a new world vob.
  zCAIBase* saved_ai_ = nullptr;
  zMAT4 saved_transform_;
  zTVobSleepingMode saved_sleeping_mode_ = zVOB_SLEEPING;
};

}  // namespace menu
