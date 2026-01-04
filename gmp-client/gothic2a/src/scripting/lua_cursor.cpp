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
constexpr std::array<int, 8> kMouseButtonCodes = {MOUSE_BUTTONLEFT, MOUSE_BUTTONRIGHT, MOUSE_BUTTONMID, MOUSE_XBUTTON1,
                                                  MOUSE_XBUTTON2,   MOUSE_XBUTTON3,    MOUSE_XBUTTON4,  MOUSE_XBUTTON5};
}  // namespace

class LuaCursorView : public zCView {
public:
  LuaCursorView(LuaCursor& owner, int x, int y, int width, int height) : zCView(x, y, x + width, y + height, VIEW_ITEM), owner_(owner) {
  }

  void Blit() override {
    owner_.Render();
  }

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
      pos_x_(0.0f),
      pos_y_(0.0f),
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

  const int initial_x = static_cast<int>(std::lround(pos_x_));
  const int initial_y = static_cast<int>(std::lround(pos_y_));
  view_ = new LuaCursorView(*this, initial_x, initial_y, width_, height_);

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
    const int x = static_cast<int>(std::lround(pos_x_));
    const int y = static_cast<int>(std::lround(pos_y_));
    view_->SetPos(x, y);
  }
}

void LuaCursor::ClampPosition() {
  if (!screen || !zrenderer) {
    pos_x_ = std::max(0.0f, pos_x_);
    pos_y_ = std::max(0.0f, pos_y_);
    return;
  }

  const float max_virtual_x = static_cast<float>(screen->anx(zrenderer->vid_xdim - 1));
  const float max_virtual_y = static_cast<float>(screen->any(zrenderer->vid_ydim - 1));

  const float max_x = std::max(0.0f, max_virtual_x);
  const float max_y = std::max(0.0f, max_virtual_y);

  pos_x_ = std::clamp(pos_x_, 0.0f, max_x);
  pos_y_ = std::clamp(pos_y_, 0.0f, max_y);
}

void LuaCursor::ApplyDelta(float delta_x, float delta_y) {
  if (!view_) {
    return;
  }

  const float scale_x = screen ? static_cast<float>(screen->anx(1)) : 1.0f;
  const float scale_y = screen ? static_cast<float>(screen->any(1)) : 1.0f;

  pos_x_ += delta_x * sensitivity_ * scale_x;
  pos_y_ += delta_y * sensitivity_ * scale_y;
  ClampPosition();
  UpdateViewPosition();
}

void LuaCursor::UpdateFromInput(zCInput* input) {
  if (!input) {
    return;
  }

  EnsureView();

  float delta_x = 0.0f;
  float delta_y = 0.0f;
  float wheel = 0.0f;
  const bool using_direct_input = PollDirectInput(delta_x, delta_y, wheel);
  if (!using_direct_input) {
    input->GetMousePos(delta_x, delta_y, wheel);
  }
  (void)wheel;

  if (delta_x != 0.0f || delta_y != 0.0f) {
    ApplyDelta(delta_x, delta_y);
  }

  if (view_) {
    view_->Blit();
  }
}

void LuaCursor::Render() {
  if (!visible_ || !view_ || !texture_ || !zrenderer || !screen) {
    return;
  }

  int virtual_width = 0;
  int virtual_height = 0;
  int virtual_pos_x = 0;
  int virtual_pos_y = 0;
  view_->GetPos(virtual_pos_x, virtual_pos_y);
  view_->GetSize(virtual_width, virtual_height);

  zVEC2 pos_min(static_cast<float>(screen->nax(virtual_pos_x)), static_cast<float>(screen->nay(virtual_pos_y)));
  zVEC2 pos_max(pos_min[VX] + static_cast<float>(screen->nax(virtual_width)), pos_min[VY] + static_cast<float>(screen->nay(virtual_height)));

  if (pos_min[VX] > zrenderer->vid_xdim - 1 || pos_min[VY] > zrenderer->vid_ydim - 1) {
    return;
  }

  if (pos_max[VX] < 0 || pos_max[VY] < 0) {
    return;
  }

  zREAL on_screen_pos_min_x = std::max(pos_min[VX], 0.0f);
  zREAL on_screen_pos_min_y = std::max(pos_min[VY], 0.0f);
  zREAL on_screen_pos_max_x = std::min(pos_max[VX], static_cast<zREAL>(zrenderer->vid_xdim - 1));
  zREAL on_screen_pos_max_y = std::min(pos_max[VY], static_cast<zREAL>(zrenderer->vid_ydim - 1));

  zREAL on_screen_size_width = on_screen_pos_max_x - on_screen_pos_min_x;
  zREAL on_screen_size_height = on_screen_pos_max_y - on_screen_pos_min_y;

  if (on_screen_size_width <= 0 || on_screen_size_height <= 0) {
    return;
  }

  zrenderer->SetViewport(on_screen_pos_min_x, on_screen_pos_min_y, on_screen_size_width, on_screen_size_height);

  int old_z_write = zrenderer->GetZBufferWriteEnabled();
  zrenderer->SetZBufferWriteEnabled(1);
  zTRnd_ZBufferCmp old_cmp = zrenderer->GetZBufferCompare();
  zrenderer->SetZBufferCompare(zRND_ZBUFFER_CMP_ALWAYS);
  zTRnd_AlphaBlendFunc old_blend_func = zrenderer->GetAlphaBlendFunc();
  zrenderer->SetAlphaBlendFunc(zRND_ALPHA_FUNC_BLEND);

  float far_z = (zCCamera::activeCam) ? zCCamera::activeCam->nearClipZ + 1.0f : 1.0f;
  zrenderer->DrawTile(texture_, pos_min, pos_max, far_z, zVEC2(0.0f, 0.0f), zVEC2(1.0f, 1.0f), zCOLOR(255, 255, 255, 255));
  zrenderer->SetAlphaBlendFunc(old_blend_func);
  zrenderer->SetZBufferWriteEnabled(old_z_write);
  zrenderer->SetZBufferCompare(old_cmp);
}

void LuaCursor::setPosition(int x, int y) {
  EnsureView();
  pos_x_ = static_cast<float>(x);
  pos_y_ = static_cast<float>(y);
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
  int x = static_cast<int>(std::lround(pos_x_));
  int y = static_cast<int>(std::lround(pos_y_));
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

sol::table LuaCursor::getPosition(sol::this_state s) const {
  return MakePosTable(s, false);
}

sol::table LuaCursor::getPositionPx(sol::this_state s) const {
  return MakePosTable(s, true);
}

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

sol::table LuaCursor::getSize(sol::this_state s) const {
  return MakeSizeTable(s, false);
}

sol::table LuaCursor::getSizePx(sol::this_state s) const {
  return MakeSizeTable(s, true);
}

void LuaCursor::setTexture(const std::string& file) {
  texture_name_ = file;
  zSTRING fileString(file.c_str());
  texture_ = zCTexture::Load(fileString, 0);
  if (view_) {
    view_->InsertBack(fileString);
  }
}

std::string LuaCursor::getTexture() const {
  return texture_name_;
}

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

bool LuaCursor::isVisible() const {
  return visible_;
}

void LuaCursor::setSensitivity(float sensitivity) {
  sensitivity_ = std::clamp(sensitivity, kMinSensitivity, kMaxSensitivity);
}

float LuaCursor::getSensitivity() const {
  return sensitivity_;
}

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

bool LuaCursor::PollDirectInput(float& delta_x, float& delta_y, float& wheel) {
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

  delta_x = static_cast<float>(state.lX);
  delta_y = static_cast<float>(state.lY);
  wheel = static_cast<float>(state.lZ);
  return true;
}

}  // namespace gmp::gothic