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

#include "lua_draw.h"

#include "ZenGin/zGothicAPI.h"

#include <unordered_set>

using namespace Gothic_II_Addon;

namespace gmp::gothic {

class LuaDrawView : public zCView {
 public:
  explicit LuaDrawView(LuaDraw& owner) : zCView(0, 0, 8192, 8192), owner_(owner) {}

  void Blit() override { owner_.Blit(); }

 private:
  LuaDraw& owner_;
};

std::unordered_set<LuaDraw*> LuaDraw::active_draws_;

LuaDraw::LuaDraw()
    : view_(nullptr),
      text_(""),
      fontName_("FONT_DEFAULT.TGA"),
      posX_(0),
      posY_(0),
      color_(255, 255, 255, 255),
      visible_(true),
      attached_to_screen_(false) {
  Initialize();
}

LuaDraw::LuaDraw(int x, int y, const std::string& text)
    : view_(nullptr),
      text_(text),
      fontName_("FONT_DEFAULT.TGA"),
      posX_(x),
      posY_(y),
      color_(255, 255, 255, 255),
      visible_(true),
      attached_to_screen_(false) {
  Initialize();
}

LuaDraw::~LuaDraw() {
  if (screen && view_ && attached_to_screen_) {
    screen->RemoveItem(view_);
    attached_to_screen_ = false;
  }

  delete view_;
  view_ = nullptr;

  active_draws_.erase(this);
}

void LuaDraw::setPosition(int x, int y) {
  posX_ = x;
  posY_ = y;
  if (view_) {
    view_->SetPos(x, y);
  }
}

void LuaDraw::setPositionPx(int x, int y) {
  if (!screen) {
    return;
  }
  setPosition(screen->anx(x), screen->any(y));
}

sol::table LuaDraw::getPosition(sol::this_state s) {
  sol::state_view lua(s);
  sol::table pos = lua.create_table();
  int x = posX_;
  int y = posY_;
  if (view_) {
    view_->GetPos(x, y);
  }
  pos["x"] = x;
  pos["y"] = y;
  return pos;
}

sol::table LuaDraw::getPositionPx(sol::this_state s) {
  sol::state_view lua(s);
  sol::table pos = lua.create_table();
  int x = posX_;
  int y = posY_;
  if (view_ && screen) {
    view_->GetPos(x, y);
    pos["x"] = screen->nax(x);
    pos["y"] = screen->nay(y);
  }
  return pos;
}

void LuaDraw::setText(const std::string& text) {
  text_ = text;
}

std::string LuaDraw::getText() const {
  return text_;
}

void LuaDraw::setFont(const std::string& fontName) {
  fontName_ = fontName;
  if (view_) {
    view_->SetFont(fontName_.c_str());
  }
}

std::string LuaDraw::getFont() const {
  return fontName_;
}

void LuaDraw::setColor(int r, int g, int b) {
  color_.SetRGB(r, g, b);
  if (view_) {
    view_->SetFontColor(color_);
  }
}

sol::table LuaDraw::getColor(sol::this_state s) {
  sol::state_view lua(s);
  sol::table color = lua.create_table();
  color["r"] = color_.r;
  color["g"] = color_.g;
  color["b"] = color_.b;
  return color;
}

void LuaDraw::setAlpha(int a) {
  color_.alpha = a;
  if (view_) {
    view_->SetFontColor(color_);
  }
}

int LuaDraw::getAlpha() const {
  return color_.alpha;
}

void LuaDraw::setVisible(bool visible) {
  visible_ = visible;
}

bool LuaDraw::getVisible() const {
  return visible_;
}

void LuaDraw::render() {
  if (view_) {
    view_->Blit();
  }
}

void LuaDraw::Initialize() {
  active_draws_.insert(this);
  view_ = new LuaDrawView(*this);
  if (view_) {
    view_->SetFont(fontName_.c_str());
    view_->SetFontColor(color_);
    view_->SetPos(posX_, posY_);
    if (screen) {
      screen->InsertItem(view_);
      attached_to_screen_ = true;
    }
  }
}

void LuaDraw::CleanupViews() {
  if (!screen) {
    return;
  }

  for (auto* draw : active_draws_) {
    if (draw && draw->view_ && draw->attached_to_screen_) {
      screen->RemoveItem(draw->view_);
      draw->attached_to_screen_ = false;
    }
  }
}

void LuaDraw::Blit() {
  if (!visible_ || text_.empty()) {
    return;
  }

  if (view_) {
    view_->ClrPrintwin();
    view_->Print(posX_, posY_, text_.c_str());
    view_->zCView::Blit();
  }
}

}  // namespace gmp::gothic