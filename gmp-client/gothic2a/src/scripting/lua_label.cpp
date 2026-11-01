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

#include "lua_label.h"

#include <unordered_set>
#include <utility>

#include "Shared/lua_runtime/bind_helpers.h"
#include "ZenGin/zGothicAPI.h"
#include "lua_helpers.h"

using namespace Gothic_II_Addon;

namespace gmp::gothic {

class LuaLabelView : public zCView {
public:
  explicit LuaLabelView(LuaLabel* owner) : zCView(0, 0, 8192, 8192), owner_(owner) {
  }

  void Blit() override {
    if (owner_) {
      owner_->Blit();
    }
  }

  void SetOwner(LuaLabel* owner) {
    owner_ = owner;
  }

private:
  LuaLabel* owner_;
};

std::unordered_set<LuaLabel*> LuaLabel::active_labels_;

/* luagmp (class)
 *
 * 2D text drawing helper for rendering overlay text on screen.
 *
 * @version  0.3.0
 * @name     Label
 * @side     client
 * @category UI
 *
 */

/* luagmp (constructor)
 *
 * Creates a new Label object with default settings.
 *
 */
LuaLabel::LuaLabel()
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

/* luagmp (constructor)
 *
 * Creates a new Label object with an initial position and text.
 *
 * @param    (number) x    Initial X position (virtual units).
 * @param    (number) y    Initial Y position (virtual units).
 * @param    (string) text Initial text content.
 *
 */
LuaLabel::LuaLabel(int x, int y, const std::string& text)
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

LuaLabel::LuaLabel(LuaLabel&& other) noexcept
    : view_(other.view_),
      text_(std::move(other.text_)),
      fontName_(std::move(other.fontName_)),
      posX_(other.posX_),
      posY_(other.posY_),
      color_(other.color_),
      visible_(other.visible_),
      attached_to_screen_(other.attached_to_screen_) {
  active_labels_.erase(&other);
  active_labels_.insert(this);
  if (view_) {
    view_->SetOwner(this);
  }
  other.view_ = nullptr;
  other.attached_to_screen_ = false;
}

LuaLabel& LuaLabel::operator=(LuaLabel&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  Destroy();

  view_ = other.view_;
  text_ = std::move(other.text_);
  fontName_ = std::move(other.fontName_);
  posX_ = other.posX_;
  posY_ = other.posY_;
  color_ = other.color_;
  visible_ = other.visible_;
  attached_to_screen_ = other.attached_to_screen_;

  active_labels_.erase(&other);
  active_labels_.insert(this);
  if (view_) {
    view_->SetOwner(this);
  }

  other.view_ = nullptr;
  other.attached_to_screen_ = false;
  return *this;
}

LuaLabel::~LuaLabel() {
  Destroy();
}

void LuaLabel::Destroy() {
  if (screen && view_ && attached_to_screen_) {
    screen->RemoveItem(view_);
    attached_to_screen_ = false;
  }

  delete view_;
  view_ = nullptr;

  active_labels_.erase(this);
}

/* luagmp (method)
 *
 * This function will set the label position in virtual screen units.
 *
 * @name     setPosition
 * @param    (number) x    X position.
 * @param    (number) y    Y position.
 *
 */
void LuaLabel::setPosition(int x, int y) {
  posX_ = x;
  posY_ = y;
}

/// Assigns the position property using the same value format as its getter.
void LuaLabel::setPositionValue(sol::object value) {
  int x = posX_;
  int y = posY_;
  if (lua_helpers::ReadVec2(value, x, y)) {
    setPosition(x, y);
  }
}

/* luagmp (property)
 *
 * Represents the label position in virtual screen units.
 *
 * @name     position
 * @return   ({x, y})
 *
 */
/* luagmp (method)
 *
 * This function will return the label position in virtual screen units.
 *
 * @name     getPosition
 * @return   ({x, y})    Table containing x and y numbers.
 *
 */
sol::table LuaLabel::getPosition(sol::this_state s) {
  sol::state_view lua(s);
  sol::table pos = lua.create_table();
  pos["x"] = posX_;
  pos["y"] = posY_;
  return pos;
}

/* luagmp (method)
 *
 * This function will set the label position in pixel coordinates.
 *
 * @name     setPositionPx
 * @param    (number) x    X position.
 * @param    (number) y    Y position.
 *
 */
void LuaLabel::setPositionPx(int x, int y) {
  if (!screen) {
    return;
  }
  setPosition(screen->anx(x), screen->any(y));
}

/// Assigns the positionPx property using the same value format as its getter.
void LuaLabel::setPositionPxValue(sol::object value) {
  int x = 0;
  int y = 0;
  if (lua_helpers::ReadVec2(value, x, y)) {
    setPositionPx(x, y);
  }
}

/* luagmp (property)
 *
 * Represents the label position in pixel coordinates.
 *
 * @name     positionPx
 * @return   ({x, y})
 *
 */
/* luagmp (method)
 *
 * This function will return the label position in pixel coordinates.
 *
 * @name     getPositionPx
 * @return   ({x, y})    Table containing x and y numbers.
 *
 */
sol::table LuaLabel::getPositionPx(sol::this_state s) {
  sol::state_view lua(s);
  sol::table pos = lua.create_table();
  if (screen) {
    pos["x"] = screen->nax(posX_);
    pos["y"] = screen->nay(posY_);
  }
  return pos;
}

/* luagmp (method)
 *
 * This function will set the text to render.
 *
 * @name     setText
 * @param    (string) text    Text to display.
 *
 */
void LuaLabel::setText(const std::string& text) {
  text_ = text;
}

/* luagmp (property)
 *
 * Represents the displayed text.
 *
 * @name     text
 * @return   (string)
 *
 */
/* luagmp (method)
 *
 * This function will return the current text.
 *
 * @name     getText
 * @return   (string)    Current text.
 *
 */
std::string LuaLabel::getText() const {
  return text_;
}

/* luagmp (method)
 *
 * This function will set the font used for rendering.
 *
 * @name     setFont
 * @param    (string) font     Font file name.
 *
 */
void LuaLabel::setFont(const std::string& fontName) {
  fontName_ = fontName;
  if (view_) {
    view_->SetFont(fontName_.c_str());
  }
}

/* luagmp (property)
 *
 * Represents the font identifier used for rendering.
 *
 * @name     font
 * @return   (string)
 *
 */
/* luagmp (method)
 *
 * This function will return the current font file name.
 *
 * @name     getFont
 * @return   (string)    Font file name.
 *
 */
std::string LuaLabel::getFont() const {
  return fontName_;
}

/* luagmp (property)
 *
 * Represents the rendered text width in virtual screen units.
 *
 * @name     width
 * @return   (number)
 *
 */
/* luagmp (method)
 *
 * This function returns the rendered text width in virtual screen units.
 *
 * @name     getWidth
 * @return   (number)    Text width.
 *
 */
int LuaLabel::getWidth() const {
  if (!view_) {
    return 0;
  }

  zSTRING text(text_.c_str());
  return view_->FontSize(text);
}

/* luagmp (property)
 *
 * Represents the rendered text height in virtual screen units.
 *
 * @name     height
 * @return   (number)
 *
 */
/* luagmp (method)
 *
 * This function returns the rendered text height in virtual screen units.
 *
 * @name     getHeight
 * @return   (number)    Text height.
 *
 */
int LuaLabel::getHeight() const {
  if (!view_) {
    return 0;
  }

  return view_->FontY();
}

/* luagmp (method)
 *
 * This function will set the text color.
 *
 * @name     setColor
 * @param    (number) r    The red color component in RGB model.
 * @param    (number) g    The green color component in RGB model.
 * @param    (number) b    The blue color component in RGB model.
 *
 */
void LuaLabel::setColor(int r, int g, int b) {
  color_.SetRGB(::lua::bind_helpers::ClampByte(r), ::lua::bind_helpers::ClampByte(g), ::lua::bind_helpers::ClampByte(b));
  if (view_) {
    view_->SetFontColor(color_);
  }
}

/* luagmp (property)
 *
 * Represents the label's color.
 *
 * @name     color
 * @return   ({r, g, b, a})
 *
 */
/* luagmp (method)
 *
 * This function will return the current text color.
 *
 * @name     getColor
 * @return   ({r, g, b, a})  Table containing color in RGBA model.
 *
 */
sol::table LuaLabel::getColor(sol::this_state s) {
  sol::state_view lua(s);
  sol::table color = lua.create_table();
  color["r"] = color_.r;
  color["g"] = color_.g;
  color["b"] = color_.b;
  color["a"] = color_.alpha;
  return color;
}

/// Assigns the color property using the same value format as its getter.
void LuaLabel::setColorValue(sol::object value) {
  int r = color_.r;
  int g = color_.g;
  int b = color_.b;
  int a = color_.alpha;
  if (lua_helpers::ReadColor(value, r, g, b, a)) {
    setColor(r, g, b);
    setAlpha(a);
  }
}

/* luagmp (method)
 *
 * This function will set the text alpha.
 *
 * @name     setAlpha
 * @param    (number) alpha    Opacity value (0-255).
 *
 */
void LuaLabel::setAlpha(int a) {
  color_.alpha = ::lua::bind_helpers::ClampByte(a);
  if (view_) {
    view_->SetFontColor(color_);
  }
}

/* luagmp (property)
 * Opacity from 0 (transparent) to 255 (opaque).
 *
 * @name     alpha
 * @return   (number)
 *
 */
/* luagmp (method)
 *
 * This function will return the current alpha
 *
 * @name     getAlpha
 * @return   (number)    Opacity value (0-255).
 *
 */
int LuaLabel::getAlpha() const {
  return color_.alpha;
}

/* luagmp (method)
 *
 * This function will set whether the Label object should render.
 *
 * @name     setVisible
 * @param    (boolean) visible     True to render, false to hide.
 *
 */
void LuaLabel::setVisible(bool visible) {
  visible_ = visible;
  if (visible_) {
    EnsureAttached();
  }
}

/* luagmp (property)
 *
 * Represents whether the Label object is rendered.
 *
 * @name     visible
 * @return   (boolean)
 *
 */
/* luagmp (method)
 *
 * This function will return whether this Label object is visible.
 *
 * @name     getVisible
 * @return   (boolean)     True if visible.
 *
 */
bool LuaLabel::getVisible() const {
  return visible_;
}

void LuaLabel::Initialize() {
  active_labels_.insert(this);
  view_ = new LuaLabelView(this);
  if (view_) {
    view_->SetFont(fontName_.c_str());
    view_->SetFontColor(color_);
    if (screen) {
      screen->InsertItem(view_);
      attached_to_screen_ = true;
    }
  }
}

void LuaLabel::EnsureAttached() {
  if (screen && view_ && visible_ && !attached_to_screen_) {
    screen->InsertItem(view_);
    attached_to_screen_ = true;
  }
}

void LuaLabel::CleanupViews() {
  if (!screen) {
    return;
  }

  for (auto* label : active_labels_) {
    if (label && label->view_ && label->attached_to_screen_) {
      screen->RemoveItem(label->view_);
      label->attached_to_screen_ = false;
    }
  }
}

void LuaLabel::EnsureViewsAttached() {
  for (auto* label : active_labels_) {
    if (label) {
      label->EnsureAttached();
    }
  }
}

void LuaLabel::Blit() {
  if (!visible_ || text_.empty()) {
    return;
  }

  if (view_) {
    view_->ClrPrintwin();
    view_->Print(posX_, posY_, text_.c_str());
    view_->zCView::Blit();
    ::lua::bind_helpers::RestoreFullViewport(zrenderer);
  }
}

void BindLabel(sol::state& lua) {
  sol::usertype<LuaLabel> label_type = lua.new_usertype<LuaLabel>("Label", sol::constructors<LuaLabel(), LuaLabel(int, int, const std::string&)>());

  label_type["setPosition"] = &LuaLabel::setPosition;
  label_type["getPosition"] = &LuaLabel::getPosition;
  label_type["setPositionPx"] = &LuaLabel::setPositionPx;
  label_type["getPositionPx"] = &LuaLabel::getPositionPx;

  label_type["setText"] = &LuaLabel::setText;
  label_type["getText"] = &LuaLabel::getText;

  label_type["setFont"] = &LuaLabel::setFont;
  label_type["getFont"] = &LuaLabel::getFont;
  label_type["getWidth"] = &LuaLabel::getWidth;
  label_type["getHeight"] = &LuaLabel::getHeight;

  label_type["setColor"] = &LuaLabel::setColor;
  label_type["getColor"] = &LuaLabel::getColor;

  label_type["setAlpha"] = &LuaLabel::setAlpha;
  label_type["getAlpha"] = &LuaLabel::getAlpha;

  label_type["setVisible"] = &LuaLabel::setVisible;
  label_type["getVisible"] = &LuaLabel::getVisible;

  // Properties (Lua table access)
  label_type["position"] = sol::property(&LuaLabel::getPosition, &LuaLabel::setPositionValue);
  label_type["positionPx"] = sol::property(&LuaLabel::getPositionPx, &LuaLabel::setPositionPxValue);
  label_type["text"] = sol::property(&LuaLabel::getText, &LuaLabel::setText);
  label_type["font"] = sol::property(&LuaLabel::getFont, &LuaLabel::setFont);
  label_type["width"] = sol::property(&LuaLabel::getWidth);
  label_type["height"] = sol::property(&LuaLabel::getHeight);
  label_type["color"] = sol::property(&LuaLabel::getColor, &LuaLabel::setColorValue);
  label_type["alpha"] = sol::property(&LuaLabel::getAlpha, &LuaLabel::setAlpha);
  label_type["visible"] = sol::property(&LuaLabel::getVisible, &LuaLabel::setVisible);
}

}  // namespace gmp::gothic
