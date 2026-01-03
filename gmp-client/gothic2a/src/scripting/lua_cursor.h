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

#include <string>

#include "ZenGin/DirectX8/include/dinput.h"
#include "ZenGin/zGothicAPI.h"
#include "sol/sol.hpp"

namespace gmp::gothic {

class LuaCursorView;

class LuaCursor {
public:
  static LuaCursor& Instance();

  void UpdateFromInput(zCInput* input);
  void Render();

  void setPosition(int x, int y);
  void setPositionPx(int x, int y);
  sol::table getPosition(sol::this_state s) const;
  sol::table getPositionPx(sol::this_state s) const;

  void setSize(int width, int height);
  void setSizePx(int width, int height);
  sol::table getSize(sol::this_state s) const;
  sol::table getSizePx(sol::this_state s) const;

  void setTexture(const std::string& file);
  std::string getTexture() const;

  void setVisible(bool visible);
  bool isVisible() const;

  void setSensitivity(float sensitivity);
  float getSensitivity() const;

  bool isButtonPressed(int button) const;

  static void CleanupViews();

private:
  LuaCursor();
  ~LuaCursor();

  void EnsureView();
  void UpdateViewSize();
  void UpdateViewPosition();
  void ApplyDelta(float dx, float dy);
  void ClampPosition();
  sol::table MakePosTable(sol::this_state s, bool pixels) const;
  sol::table MakeSizeTable(sol::this_state s, bool pixels) const;
  bool PollDirectInput(float& dx, float& dy, float& wheel);

  LuaCursorView* view_;
  zCTexture* texture_;
  std::string texture_name_;
  float sensitivity_;
  bool visible_;
  bool attached_to_screen_;
  float posX_;
  float posY_;
  int width_;
  int height_;

  LPDIRECTINPUTDEVICE8A mouse_device_;
};

}  // namespace gmp::gothic