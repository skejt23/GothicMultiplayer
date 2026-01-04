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

#include <unordered_set>

#include "ZenGin/zGothicAPI.h"

using namespace Gothic_II_Addon;

namespace gmp::gothic {

class LuaDrawView : public zCView {
public:
  explicit LuaDrawView(LuaDraw& owner) : zCView(0, 0, 8192, 8192), owner_(owner) {
  }

  void Blit() override {
    owner_.Blit();
  }

private:
  LuaDraw& owner_;
};

std::unordered_set<LuaDraw*> LuaDraw::active_draws_;

/* luadoc (class)
*
* 2D text drawing helper for rendering overlay text on screen.
*
* The Draw class stores position, text, font, color, alpha and visibility.
* Call render() each frame to draw the text using current settings.
*
* @name     Draw
* @side     client
* @category Draw
*
*/

/* luadoc (constructor)
*
* Creates a new Draw object with default settings.
*
*/
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

/* luadoc (constructor)
*
* Creates a new Draw object with an initial position and text.
*
* @param    (int) x Initial X position (virtual units).
* @param    (int) y Initial Y position (virtual units).
* @param    (string) text Initial text content.
*
*/
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

/* luadoc (method)
*
* Sets the draw position in virtual screen units.
*
* @name     setPosition
* @param    (int) x X position (virtual units).
* @param    (int) y Y position (virtual units).
*
*/
void LuaDraw::setPosition(int x, int y) {
  posX_ = x;
  posY_ = y;
  if (view_) {
    view_->SetPos(x, y);
  }
}

/* luadoc (method)
*
* Returns the draw position in virtual screen units.
*
* @name     getPosition
* @return   ({x, y}) Table containing x and y (virtual units).
*
*/
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

/* luadoc (method)
*
* Sets the draw position in pixel coordinates.
*
* @name     setPositionPx
* @param    (int) x X position in pixels.
* @param    (int) y Y position in pixels.
*
*/
void LuaDraw::setPositionPx(int x, int y) {
  if (!screen) {
    return;
  }
  setPosition(screen->anx(x), screen->any(y));
}

/* luadoc (method)
*
* Returns the draw position in pixel coordinates.
*
* @name     getPositionPx
* @return   ({x, y}) Table containing x and y in pixels.
*
*/
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

/* luadoc (method)
*
* Sets the text to render.
*
* @name     setText
* @param    (string) text Text to display.
*
*/
void LuaDraw::setText(const std::string& text) {
  text_ = text;
}

/* luadoc (method)
*
* Returns the current text.
*
* @name     getText
* @return   (string) Current text.
*
*/
std::string LuaDraw::getText() const {
  return text_;
}

/* luadoc (method)
*
* Sets the font used for rendering.
*
* @name     setFont
* @param    (string) font Font file name.
*
*/
void LuaDraw::setFont(const std::string& fontName) {
  fontName_ = fontName;
  if (view_) {
    view_->SetFont(fontName_.c_str());
  }
}

/* luadoc (method)
*
* Returns the current font file name.
*
* @name     getFont
* @return   (string) Font file name.
*
*/
std::string LuaDraw::getFont() const {
  return fontName_;
}

/* luadoc (method)
*
* Sets the text color.
*
* @name     setColor
* @param    (int) r Red component (0-255).
* @param    (int) g Green component (0-255).
* @param    (int) b Blue component (0-255).
*
*/
void LuaDraw::setColor(int r, int g, int b) {
  color_.SetRGB(r, g, b);
  if (view_) {
    view_->SetFontColor(color_);
  }
}

/* luadoc (method)
*
* Returns the current text color.
*
* @name     getColor
* @return   ({r, g, b}) Table containing r,g,b (0-255).
*
*/
sol::table LuaDraw::getColor(sol::this_state s) {
  sol::state_view lua(s);
  sol::table color = lua.create_table();
  color["r"] = color_.r;
  color["g"] = color_.g;
  color["b"] = color_.b;
  return color;
}

/* luadoc (method)
*
* Sets text alpha (opacity).
*
* @name     setAlpha
* @param    (int) alpha Opacity value (0-255).
*
*/
void LuaDraw::setAlpha(int a) {
  color_.alpha = a;
  if (view_) {
    view_->SetFontColor(color_);
  }
}

/* luadoc (method)
*
* Returns the current alpha (opacity).
*
* @name     getAlpha
* @return   (int) Opacity value (0-255).
*
*/
int LuaDraw::getAlpha() const {
  return color_.alpha;
}

/* luadoc (method)
*
* Sets whether the Draw object should render.
*
* @name     setVisible
* @param    (bool) visible True to render, false to hide.
*
*/
void LuaDraw::setVisible(bool visible) {
  visible_ = visible;
}

/* luadoc (method)
*
* Returns whether this Draw object is visible.
*
* @name     getVisible
* @return   (bool) True if visible.
*
*/
bool LuaDraw::getVisible() const {
  return visible_;
}

/* luadoc (property)
*
* Gets or sets the draw position in virtual screen units.
*
* @name     position
* @return   ({x, y}) Table containing x and y (virtual units).
*
*/
/* luadoc (property)
*
* Gets or sets the draw position in pixel coordinates.
*
* @name     positionPx
* @return   ({x, y}) Table containing x and y (pixels).
*
*/
/* luadoc (property)
*
* Gets or sets the displayed text.
*
* @name     text
* @return   (string) Current text.
*
*/
/* luadoc (property)
*
* Gets or sets the font identifier used for rendering.
*
* @name     font
* @return   (string) Font identifier/name.
*
*/
/* luadoc (property)
*
* Returns the current text color.
*
* @name     color
* @readonly
* @return   ({r, g, b}) Table containing r,g,b (0-255).
*
*/
/* luadoc (property)
*
* Gets or sets the alpha (opacity).
*
* @name     alpha
* @return   (int) Opacity value (0-255).
*
*/
/* luadoc (property)
*
* Gets or sets whether the Draw object is rendered.
*
* @name     visible
* @return   (bool) True if visible.
*
*/

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