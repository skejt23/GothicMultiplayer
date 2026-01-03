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

#include "lua_cursor.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

using namespace Gothic_II_Addon;

namespace gmp::gothic {
namespace {
constexpr float kMinSensitivity = 1.0f;
constexpr float kMaxSensitivity = 10.0f;
constexpr int kDefaultCursorSizePx = 96;
constexpr uintptr_t kGothicMouseDeviceAddress = 0x008D1D70;
constexpr std::array<int, 8> kMouseButtonCodes = {
    MOUSE_BUTTONLEFT, MOUSE_BUTTONRIGHT, MOUSE_BUTTONMID, MOUSE_XBUTTON1,
    MOUSE_XBUTTON2,  MOUSE_XBUTTON3,    MOUSE_XBUTTON4,  MOUSE_XBUTTON5
};
}  // namespace

class LuaCursorView : public zCView {
public:
  LuaCursorView(LuaCursor& owner, int x, int y, int width, int height)
      : zCView(x, y, x + width, y + height, VIEW_ITEM), owner_(owner) {}

  void Blit() override { owner_.Render(); }

private:
  LuaCursor& owner_;
};

LuaCursor& LuaCursor::Instance() {
  static LuaCursor cursor;
  return cursor;
}

LuaCursor::LuaCursor()
    : view_(nullptr),
      texture_(nullptr),
      texture_name_("LO.TGA"),
      sensitivity_(1.0f),
      visible_(false),
      attached_to_screen_(false),
      posX_(0.0f),
      posY_(0.0f),
      width_(kDefaultCursorSizePx),
      height_(kDefaultCursorSizePx),
      mouse_device_(nullptr) {
  EnsureView();
  setSizePx(kDefaultCursorSizePx, kDefaultCursorSizePx);
  setTexture(texture_name_);
}

LuaCursor::~LuaCursor() {
  if (screen && view_ && attached_to_screen_) {
    screen->RemoveItem(view_);
  }

  delete view_;
  view_ = nullptr;
}

void LuaCursor::EnsureView() {
  if (view_) {
    if (!attached_to_screen_ && screen && visible_) {
      screen->InsertItem(view_);
      attached_to_screen_ = true;
    }
    return;
  }

  const int initialX = static_cast<int>(std::lround(posX_));
  const int initialY = static_cast<int>(std::lround(posY_));
  view_ = new LuaCursorView(*this, initialX, initialY, width_, height_);

  if (screen && view_ && visible_) {
    screen->InsertItem(view_);
    attached_to_screen_ = true;
  }
}

void LuaCursor::UpdateViewSize() {
  if (view_) {
    view_->SetSize(width_, height_);
  }
}

void LuaCursor::UpdateViewPosition() {
  if (view_) {
    const int x = static_cast<int>(std::lround(posX_));
    const int y = static_cast<int>(std::lround(posY_));
    view_->SetPos(x, y);
  }
}

void LuaCursor::ClampPosition() {
  if (!screen || !zrenderer) {
    posX_ = std::max(0.0f, posX_);
    posY_ = std::max(0.0f, posY_);
    return;
  }

  const float max_virtual_x = static_cast<float>(screen->anx(zrenderer->vid_xdim - 1));
  const float max_virtual_y = static_cast<float>(screen->any(zrenderer->vid_ydim - 1));

  const float maxX = std::max(0.0f, max_virtual_x);
  const float maxY = std::max(0.0f, max_virtual_y);

  posX_ = std::clamp(posX_, 0.0f, maxX);
  posY_ = std::clamp(posY_, 0.0f, maxY);
}

void LuaCursor::ApplyDelta(float dx, float dy) {
  if (!view_) {
    return;
  }

  const float scaleX = screen ? static_cast<float>(screen->anx(1)) : 1.0f;
  const float scaleY = screen ? static_cast<float>(screen->any(1)) : 1.0f;

  posX_ += dx * sensitivity_ * scaleX;
  posY_ += dy * sensitivity_ * scaleY;
  ClampPosition();
  UpdateViewPosition();
}

void LuaCursor::UpdateFromInput(zCInput* input) {
  if (!input) {
    return;
  }

  EnsureView();

  float dx = 0.0f;
  float dy = 0.0f;
  float wheel = 0.0f;
  const bool using_direct_input = PollDirectInput(dx, dy, wheel);
  if (!using_direct_input) {
    input->GetMousePos(dx, dy, wheel);
  }
  (void)wheel;

  if (dx != 0.0f || dy != 0.0f) {
    ApplyDelta(dx, dy);
  }

  if (view_) {
    view_->Blit();
  }
}

void LuaCursor::Render() {
  if (!visible_ || !view_ || !texture_ || !zrenderer || !screen) {
    return;
  }

  int virtualWidth = 0;
  int virtualHeight = 0;
  int virtualPosX = 0;
  int virtualPosY = 0;
  view_->GetPos(virtualPosX, virtualPosY);
  view_->GetSize(virtualWidth, virtualHeight);

  zVEC2 posMin(static_cast<float>(screen->nax(virtualPosX)), static_cast<float>(screen->nay(virtualPosY)));
  zVEC2 posMax(posMin[VX] + static_cast<float>(screen->nax(virtualWidth)),
               posMin[VY] + static_cast<float>(screen->nay(virtualHeight)));

  if (posMin[VX] > zrenderer->vid_xdim - 1 || posMin[VY] > zrenderer->vid_ydim - 1) {
    return;
  }

  if (posMax[VX] < 0 || posMax[VY] < 0) {
    return;
  }

  zREAL onScreenPosMinX = std::max(posMin[VX], 0.0f);
  zREAL onScreenPosMinY = std::max(posMin[VY], 0.0f);
  zREAL onScreenPosMaxX = std::min(posMax[VX], static_cast<zREAL>(zrenderer->vid_xdim - 1));
  zREAL onScreenPosMaxY = std::min(posMax[VY], static_cast<zREAL>(zrenderer->vid_ydim - 1));

  zREAL onScreenSizeWidth = onScreenPosMaxX - onScreenPosMinX;
  zREAL onScreenSizeHeight = onScreenPosMaxY - onScreenPosMinY;

  if (onScreenSizeWidth <= 0 || onScreenSizeHeight <= 0) {
    return;
  }

  zrenderer->SetViewport(onScreenPosMinX, onScreenPosMinY, onScreenSizeWidth, onScreenSizeHeight);
  zREAL farZ = (zCCamera::activeCam) ? zCCamera::activeCam->nearClipZ + 1.0f : 1.0f;
  zrenderer->DrawTile(texture_, posMin, posMax, farZ, zVEC2(0.0f, 0.0f), zVEC2(1.0f, 1.0f), zCOLOR(255, 255, 255, 255));
}

void LuaCursor::setPosition(int x, int y) {
  EnsureView();
  posX_ = static_cast<float>(x);
  posY_ = static_cast<float>(y);
  ClampPosition();
  UpdateViewPosition();
}

void LuaCursor::setPositionPx(int x, int y) {
  if (!screen) {
    return;
  }
  setPosition(screen->anx(x), screen->any(y));
}

sol::table LuaCursor::MakePosTable(sol::this_state s, bool pixels) const {
  sol::state_view lua(s);
  sol::table pos = lua.create_table();
  int x = static_cast<int>(std::lround(posX_));
  int y = static_cast<int>(std::lround(posY_));
  if (view_) {
    view_->GetPos(x, y);
  }
  if (pixels && screen) {
    pos["x"] = screen->nax(x);
    pos["y"] = screen->nay(y);
  } else {
    pos["x"] = x;
    pos["y"] = y;
  }
  return pos;
}

sol::table LuaCursor::getPosition(sol::this_state s) const { return MakePosTable(s, false); }

sol::table LuaCursor::getPositionPx(sol::this_state s) const { return MakePosTable(s, true); }

void LuaCursor::setSize(int width, int height) {
  EnsureView();
  width_ = std::max(1, width);
  height_ = std::max(1, height);
  ClampPosition();
  UpdateViewSize();
}

void LuaCursor::setSizePx(int width, int height) {
  if (!screen) {
    return;
  }
  setSize(screen->anx(width), screen->any(height));
}

sol::table LuaCursor::MakeSizeTable(sol::this_state s, bool pixels) const {
  sol::state_view lua(s);
  sol::table size = lua.create_table();
  int width = width_;
  int height = height_;
  if (view_) {
    view_->GetSize(width, height);
  }
  if (pixels && screen) {
    size["width"] = screen->nax(width);
    size["height"] = screen->nay(height);
  } else {
    size["width"] = width;
    size["height"] = height;
  }
  return size;
}

sol::table LuaCursor::getSize(sol::this_state s) const { return MakeSizeTable(s, false); }

sol::table LuaCursor::getSizePx(sol::this_state s) const { return MakeSizeTable(s, true); }

void LuaCursor::setTexture(const std::string& file) {
  texture_name_ = file;
  zSTRING fileString(file.c_str());
  texture_ = zCTexture::Load(fileString, 0);
  if (view_) {
    view_->InsertBack(fileString);
  }
}

std::string LuaCursor::getTexture() const { return texture_name_; }

void LuaCursor::setVisible(bool visible) {
  visible_ = visible;

  if (!view_) {
    return;
  }

  if (visible_ && screen && !attached_to_screen_) {
    screen->InsertItem(view_);
    attached_to_screen_ = true;
  } else if (!visible_ && screen && attached_to_screen_) {
    screen->RemoveItem(view_);
    attached_to_screen_ = false;
  }
}

bool LuaCursor::isVisible() const { return visible_; }

void LuaCursor::setSensitivity(float sensitivity) {
  sensitivity_ = std::clamp(sensitivity, kMinSensitivity, kMaxSensitivity);
}

float LuaCursor::getSensitivity() const { return sensitivity_; }

bool LuaCursor::isButtonPressed(int button) const {
  if (!zinput || button < 0 || static_cast<std::size_t>(button) >= kMouseButtonCodes.size()) {
    return false;
  }

  return zinput->KeyPressed(kMouseButtonCodes[button]) != 0;
}

void LuaCursor::CleanupViews() {
  LuaCursor& cursor = Instance();

  if (screen && cursor.view_ && cursor.attached_to_screen_) {
    screen->RemoveItem(cursor.view_);
    cursor.attached_to_screen_ = false;
  }

  cursor.visible_ = false;
}

bool LuaCursor::PollDirectInput(float& dx, float& dy, float& wheel) {
  if (!mouse_device_) {
    auto device_ptr = reinterpret_cast<LPDIRECTINPUTDEVICE8A*>(kGothicMouseDeviceAddress);
    if (!device_ptr) {
      return false;
    }
    mouse_device_ = *device_ptr;
  }

  if (!mouse_device_) {
    return false;
  }

  DIMOUSESTATE2 state{};
  HRESULT hr = mouse_device_->GetDeviceState(sizeof(state), &state);
  if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
    mouse_device_->Acquire();
    hr = mouse_device_->GetDeviceState(sizeof(state), &state);
  }

  if (FAILED(hr)) {
    return false;
  }

  dx = static_cast<float>(state.lX);
  dy = static_cast<float>(state.lY);
  wheel = static_cast<float>(state.lZ);
  return true;
}

}  // namespace gmp::gothic