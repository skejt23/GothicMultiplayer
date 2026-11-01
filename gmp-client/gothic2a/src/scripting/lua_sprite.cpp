#include "lua_sprite.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <utility>

#include "Shared/lua_runtime/bind_helpers.h"
#include "ZenGin/zGothicAPI.h"
#include "lua_helpers.h"

using namespace Gothic_II_Addon;

namespace gmp::gothic {

class LuaSpriteView : public zCView {
public:
  LuaSpriteView(LuaSprite* owner, int x, int y, int width, int height) : zCView(x, y, x + width, y + height, VIEW_ITEM), owner_(owner) {
  }

  void Blit() override {
    if (owner_) {
      owner_->Blit();
    }
  }

  void SetOwner(LuaSprite* owner) {
    owner_ = owner;
  }

private:
  LuaSprite* owner_;
};

std::unordered_set<LuaSprite*> LuaSprite::active_sprites_;

/* luagmp (class)
 *
 * This class represents a 2d Sprite on screen.
 *
 * @version  0.3.0
 * @name     Sprite
 * @side     client
 * @category UI
 *
 */

/* luagmp (constructor)
 *
 * Creates a new Sprite.
 *
 * @param    (number) x      X position (virtual units).
 * @param    (number) y      Y position (virtual units).
 * @param    (number) width  Width (virtual units).
 * @param    (number) height Height (virtual units).
 * @param    (string) file   Texture file path displayed by the sprite.
 *
 */
LuaSprite::LuaSprite(int x, int y, int width, int height, const std::string& file)
    : view_(nullptr),
      texture_(nullptr),
      uv_min_(0.0f, 0.0f),
      uv_max_(1.0f, 1.0f),
      color_(255, 255, 255, 255),
      alphaFunc_(zRND_ALPHA_FUNC_BLEND),
      fillZ_(false),
      visible_(true),
      fileName_(file),
      attached_to_screen_(false) {
  active_sprites_.insert(this);
  view_ = new LuaSpriteView(this, x, y, width, height);

  setFile(file);

  if (screen && view_) {
    screen->InsertItem(view_);
    attached_to_screen_ = true;
  }
}

LuaSprite::LuaSprite(LuaSprite&& other) noexcept
    : view_(other.view_),
      texture_(other.texture_),
      uv_min_(other.uv_min_),
      uv_max_(other.uv_max_),
      color_(other.color_),
      alphaFunc_(other.alphaFunc_),
      fillZ_(other.fillZ_),
      visible_(other.visible_),
      fileName_(std::move(other.fileName_)),
      attached_to_screen_(other.attached_to_screen_) {
  active_sprites_.erase(&other);
  active_sprites_.insert(this);
  if (view_) {
    view_->SetOwner(this);
  }
  other.view_ = nullptr;
  other.texture_ = nullptr;
  other.attached_to_screen_ = false;
}

LuaSprite& LuaSprite::operator=(LuaSprite&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  Destroy();

  view_ = other.view_;
  texture_ = other.texture_;
  uv_min_ = other.uv_min_;
  uv_max_ = other.uv_max_;
  color_ = other.color_;
  alphaFunc_ = other.alphaFunc_;
  fillZ_ = other.fillZ_;
  visible_ = other.visible_;
  fileName_ = std::move(other.fileName_);
  attached_to_screen_ = other.attached_to_screen_;

  active_sprites_.erase(&other);
  active_sprites_.insert(this);
  if (view_) {
    view_->SetOwner(this);
  }

  other.view_ = nullptr;
  other.texture_ = nullptr;
  other.attached_to_screen_ = false;
  return *this;
}

LuaSprite::~LuaSprite() {
  Destroy();
}

void LuaSprite::Destroy() {
  if (screen && view_ && attached_to_screen_) {
    screen->RemoveItem(view_);
    attached_to_screen_ = false;
  }

  delete view_;
  view_ = nullptr;

  if (texture_) {
    texture_->Release();
  }
  texture_ = nullptr;

  active_sprites_.erase(this);
}

/* luagmp (method)
 *
 * This method will set the Sprite position in virtual screen units.
 *
 * @name     setPosition
 * @param    (number) x    X position (virtual units).
 * @param    (number) y    Y position (virtual units).
 *
 */
void LuaSprite::setPosition(int x, int y) {
  updateViewPos(x, y);
}

/// Assigns the position property using the same value format as its getter.
void LuaSprite::setPositionValue(sol::object value) {
  int x = 0;
  int y = 0;
  if (lua_helpers::ReadVec2(value, x, y)) {
    setPosition(x, y);
  }
}

/* luagmp (property)
 *
 * Represents the Sprite position in virtual screen units.
 *
 * @name     position
 * @return   ({x, y})
 *
 */
/* luagmp (method)
 *
 * This method will return the Sprite position in virtual screen units.
 *
 * @name     getPosition
 * @return   ({x, y})      Table containing x and y (virtual units).
 *
 */
sol::table LuaSprite::getPosition(sol::this_state s) {
  sol::state_view lua(s);
  sol::table pos = lua.create_table();
  int x = 0;
  int y = 0;
  if (view_) {
    view_->GetPos(x, y);
  }
  pos["x"] = x;
  pos["y"] = y;
  return pos;
}

/* luagmp (method)
 *
 * This method will set the Sprite position in pixel coordinates.
 *
 * @name     setPositionPx
 * @param    (number) x    X position (pixels).
 * @param    (number) y    Y position (pixels).
 *
 */
void LuaSprite::setPositionPx(int x, int y) {
  if (!screen) {
    return;
  }
  updateViewPos(screen->anx(x), screen->any(y));
}

/// Assigns the positionPx property using the same value format as its getter.
void LuaSprite::setPositionPxValue(sol::object value) {
  int x = 0;
  int y = 0;
  if (lua_helpers::ReadVec2(value, x, y)) {
    setPositionPx(x, y);
  }
}

/* luagmp (property)
 *
 * Represents the Sprite position in pixel coordinates.
 *
 * @name     positionPx
 * @return   ({x, y})
 *
 */
/* luagmp (method)
 *
 * This method will return the Sprite position in pixel coordinates.
 *
 * @name     getPositionPx
 * @return   ({x, y})      Table containing x and y (pixels).
 *
 */
sol::table LuaSprite::getPositionPx(sol::this_state s) {
  sol::state_view lua(s);
  sol::table pos = lua.create_table();
  int virtualX = 0;
  int virtualY = 0;
  if (view_ && screen) {
    view_->GetPos(virtualX, virtualY);
    pos["x"] = screen->nax(virtualX);
    pos["y"] = screen->nay(virtualY);
  }
  return pos;
}

/* luagmp (method)
 *
 * This method will set the Sprite size in virtual screen units.
 *
 * @name     setSize
 * @param    (number) width    Width (virtual units).
 * @param    (number) height   Height (virtual units).
 *
 */
void LuaSprite::setSize(int width, int height) {
  updateViewSize(width, height);
}

/// Assigns the size property using the same value format as its getter.
void LuaSprite::setSizeValue(sol::object value) {
  int width = 0;
  int height = 0;
  if (lua_helpers::ReadSize(value, width, height)) {
    setSize(width, height);
  }
}

/* luagmp (property)
 *
 * Represents the Sprite size in virtual screen units.
 *
 * @name     size
 * @return   ({width, height})
 *
 */
/* luagmp (method)
 *
 * This method will return the Sprite size in virtual screen units.
 *
 * @name     getSize
 * @return   ({width, height}) Table containing width and height (virtual units).
 *
 */
sol::table LuaSprite::getSize(sol::this_state s) {
  sol::state_view lua(s);
  sol::table size = lua.create_table();
  int width = 0;
  int height = 0;
  if (view_) {
    view_->GetSize(width, height);
  }
  size["width"] = width;
  size["height"] = height;
  return size;
}

/* luagmp (method)
 *
 * This method will set the Sprite size in pixel coordinates.
 *
 * @name     setSizePx
 * @param    (number) width    Width (pixels).
 * @param    (number) height   Height (pixels).
 *
 */
void LuaSprite::setSizePx(int width, int height) {
  if (!screen) {
    return;
  }
  updateViewSize(screen->anx(width), screen->any(height));
}

/// Assigns the sizePx property using the same value format as its getter.
void LuaSprite::setSizePxValue(sol::object value) {
  int width = 0;
  int height = 0;
  if (lua_helpers::ReadSize(value, width, height)) {
    setSizePx(width, height);
  }
}

/* luagmp (property)
 *
 * Represents the Sprite size in pixel coordinates.
 *
 * @name     sizePx
 * @return   ({width, height})
 *
 */
/* luagmp (method)
 *
 * This method will return the Sprite size in pixel coordinates.
 *
 * @name     getSizePx
 * @return   ({width, height}) Table containing width and height (pixels).
 *
 */
sol::table LuaSprite::getSizePx(sol::this_state s) {
  sol::state_view lua(s);
  sol::table size = lua.create_table();
  int width = 0;
  int height = 0;
  if (view_ && screen) {
    view_->GetSize(width, height);
    size["width"] = screen->nax(width);
    size["height"] = screen->nay(height);
  }
  return size;
}

/* luagmp (method)
 *
 * This method will set the Sprite color.
 *
 * @name     setColor
 * @param    (number) r    The red color component in RGB model.
 * @param    (number) g    The green color component in RGB model.
 * @param    (number) b    The blue color component in RGB model.
 *
 */
void LuaSprite::setColor(int r, int g, int b) {
  color_.SetRGB(::lua::bind_helpers::ClampByte(r), ::lua::bind_helpers::ClampByte(g), ::lua::bind_helpers::ClampByte(b));
}

/* luagmp (property)
 *
 * Represents the Sprite color.
 *
 * @name     color
 * @return   ({r, g, b, a})
 *
 */
/* luagmp (method)
 *
 * This method will return the Sprite color.
 *
 * @name     getColor
 * @return   ({r, g, b, a}) Table containing color in RGBA model.
 *
 */
sol::table LuaSprite::getColor(sol::this_state s) {
  sol::state_view lua(s);
  sol::table colorTable = lua.create_table();
  colorTable["r"] = color_.r;
  colorTable["g"] = color_.g;
  colorTable["b"] = color_.b;
  colorTable["a"] = color_.alpha;
  return colorTable;
}

/// Assigns the color property using the same value format as its getter.
void LuaSprite::setColorValue(sol::object value) {
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
 * This method will set the Sprite alpha.
 *
 * @name     setAlpha
 * @param    (number) alpha Opacity value (0-255).
 *
 */
void LuaSprite::setAlpha(int alpha) {
  color_.alpha = ::lua::bind_helpers::ClampByte(alpha);
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
 * This method will return the current Sprite alpha (opacity).
 *
 * @name     getAlpha
 * @return   (number)      Opacity value (0-255).
 *
 */
int LuaSprite::getAlpha() const {
  return color_.alpha;
}

/* luagmp (method)
 *
 * This method will set the Texture file name.
 *
 * @name     setFile
 * @param    (string) file  Texture file name.
 *
 */
void LuaSprite::setFile(const std::string& file) {
  if (texture_) {
    texture_->Release();
    texture_ = nullptr;
  }

  fileName_ = file;
  zSTRING fileString(file.c_str());
  texture_ = zCTexture::Load(fileString, zTEX_LOAD_FLAG_TILE);
}

/* luagmp (property)
 *
 * Represents the texture file path.
 *
 * @name     file
 * @return   (string)
 *
 */
/* luagmp (method)
 *
 * This method will return the Texture file name.
 *
 * @name     getFile
 * @return   (string)    Texture file name.
 *
 */
std::string LuaSprite::getFile() const {
  return fileName_;
}

/* luagmp (method)
 *
 * This method will set whether the Sprite should be rendered.
 *
 * @name     setVisible
 * @param    (boolean) visible   True to render, false to hide.
 *
 */
void LuaSprite::setVisible(bool visible) {
  visible_ = visible;
  if (visible_) {
    EnsureAttached();
  }
}

/* luagmp (property)
 *
 * Represents whether the texture is rendered.
 *
 * @name     visible
 * @return   (boolean)
 *
 */
/* luagmp (method)
 *
 * This method will return whether the Sprite is visible.
 *
 * @name     getVisible
 * @return   (boolean)   True if visible.
 *
 */
bool LuaSprite::getVisible() const {
  return visible_;
}

/* luagmp (method)
 *
 * This method will move the Sprite to the front of the render order.
 *
 * @name     bringToFront
 *
 */
void LuaSprite::bringToFront() {
  EnsureAttached();
  if (view_) {
    view_->Top();
  }
}

void LuaSprite::CleanupViews() {
  if (!screen) {
    return;
  }

  for (auto* texture : active_sprites_) {
    if (texture && texture->view_ && texture->attached_to_screen_) {
      screen->RemoveItem(texture->view_);
      texture->attached_to_screen_ = false;
    }
  }
}

void LuaSprite::EnsureAttached() {
  if (screen && view_ && visible_ && !attached_to_screen_) {
    screen->InsertItem(view_);
    attached_to_screen_ = true;
  }
}

void LuaSprite::EnsureViewsAttached() {
  for (auto* texture : active_sprites_) {
    if (texture) {
      texture->EnsureAttached();
    }
  }
}

void LuaSprite::Blit() {
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
  zVEC2 posMax(posMin[VX] + static_cast<float>(screen->nax(virtualWidth)), posMin[VY] + static_cast<float>(screen->nay(virtualHeight)));

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

  zBOOL oldzWrite = zrenderer->GetZBufferWriteEnabled();
  zrenderer->SetZBufferWriteEnabled(fillZ_);

  zTRnd_ZBufferCmp oldCmp = zrenderer->GetZBufferCompare();
  zrenderer->SetZBufferCompare(zRND_ZBUFFER_CMP_ALWAYS);

  zTRnd_AlphaBlendFunc oldBlendFunc = zrenderer->GetAlphaBlendFunc();
  zrenderer->SetAlphaBlendFunc(alphaFunc_);

  zBOOL oldBilerpFilter = zrenderer->GetBilerpFilterEnabled();
  zrenderer->SetBilerpFilterEnabled(1);

  zREAL farZ;
  if (fillZ_) {
    farZ = (zCCamera::activeCam) ? zCCamera::activeCam->farClipZ - 1 : 65534.0f;
  } else {
    farZ = (zCCamera::activeCam) ? zCCamera::activeCam->nearClipZ + 1 : 1.0f;
  }

  zrenderer->DrawTile(texture_, posMin, posMax, farZ, uv_min_, uv_max_, color_);

  zrenderer->SetBilerpFilterEnabled(oldBilerpFilter);
  zrenderer->SetAlphaBlendFunc(oldBlendFunc);
  zrenderer->SetZBufferWriteEnabled(oldzWrite);
  zrenderer->SetZBufferCompare(oldCmp);
  ::lua::bind_helpers::RestoreFullViewport(zrenderer);
}

void LuaSprite::updateViewSize(int width, int height) {
  if (!view_) {
    return;
  }
  view_->SetSize(width, height);
}

void LuaSprite::updateViewPos(int x, int y) {
  if (!view_) {
    return;
  }
  view_->SetPos(x, y);
}

/* luagmp (property)
 * Normalized texture rectangle {x, y, width, height}, independent of screen position and size.
 *
 * @name uvRect
 * @return (table)
 */
/* luagmp (method)
 * Returns the normalized texture rectangle; screen position and size are independent.
 * @name getUVRect
 * @return (table) {x, y, width, height} in normalized UV coordinates.
 */

sol::table LuaSprite::getUVRect(sol::this_state s) const {
  sol::state_view lua(s);
  return lua.create_table_with("x", uv_min_[VX], "y", uv_min_[VY], "width", uv_max_[VX] - uv_min_[VX], "height", uv_max_[VY] - uv_min_[VY]);
}

/* luagmp (method)
 * Sets the normalized texture rectangle. Coordinates must be finite; values outside 0..1 follow renderer addressing behavior.
 * @name setUVRect
 * @param (number) u Left texture coordinate.
 * @param (number) v Top texture coordinate.
 * @param (number) width Texture rectangle width.
 * @param (number) height Texture rectangle height.
 */
void LuaSprite::setUVRect(float u, float v, float width, float height) {
  if (!std::isfinite(u) || !std::isfinite(v) || !std::isfinite(width) || !std::isfinite(height)) {
    throw sol::error("UV coordinates must be finite numbers");
  }
  uv_min_ = zVEC2(u, v);
  uv_max_ = zVEC2(u + width, v + height);
}

/// Assigns the uvRect property using the same value format as its getter.
void LuaSprite::setUVRectValue(sol::table value) {
  setUVRect(value.get<float>("x"), value.get<float>("y"), value.get<float>("width"), value.get<float>("height"));
}

void BindSprite(sol::state& lua) {
  sol::usertype<LuaSprite> sprite_type =
      lua.new_usertype<LuaSprite>("Sprite", sol::constructors<LuaSprite(int, int, int, int, const std::string&)>());

  sprite_type["setPosition"] = &LuaSprite::setPosition;
  sprite_type["getPosition"] = &LuaSprite::getPosition;
  sprite_type["setPositionPx"] = &LuaSprite::setPositionPx;
  sprite_type["getPositionPx"] = &LuaSprite::getPositionPx;

  sprite_type["setSize"] = &LuaSprite::setSize;
  sprite_type["getSize"] = &LuaSprite::getSize;
  sprite_type["setSizePx"] = &LuaSprite::setSizePx;
  sprite_type["getSizePx"] = &LuaSprite::getSizePx;

  sprite_type["setColor"] = &LuaSprite::setColor;
  sprite_type["getColor"] = &LuaSprite::getColor;
  sprite_type["setAlpha"] = &LuaSprite::setAlpha;
  sprite_type["getAlpha"] = &LuaSprite::getAlpha;

  sprite_type["setVisible"] = &LuaSprite::setVisible;
  sprite_type["getVisible"] = &LuaSprite::getVisible;
  sprite_type["setFile"] = &LuaSprite::setFile;
  sprite_type["getFile"] = &LuaSprite::getFile;

  // Properties (Lua table access)
  sprite_type["position"] = sol::property(&LuaSprite::getPosition, &LuaSprite::setPositionValue);
  sprite_type["positionPx"] = sol::property(&LuaSprite::getPositionPx, &LuaSprite::setPositionPxValue);
  sprite_type["size"] = sol::property(&LuaSprite::getSize, &LuaSprite::setSizeValue);
  sprite_type["sizePx"] = sol::property(&LuaSprite::getSizePx, &LuaSprite::setSizePxValue);
  sprite_type["color"] = sol::property(&LuaSprite::getColor, &LuaSprite::setColorValue);
  sprite_type["alpha"] = sol::property(&LuaSprite::getAlpha, &LuaSprite::setAlpha);
  sprite_type["visible"] = sol::property(&LuaSprite::getVisible, &LuaSprite::setVisible);
  sprite_type["file"] = sol::property(&LuaSprite::getFile, &LuaSprite::setFile);
  sprite_type["getUVRect"] = &LuaSprite::getUVRect;
  sprite_type["setUVRect"] = &LuaSprite::setUVRect;
  sprite_type["uvRect"] = sol::property(&LuaSprite::getUVRect, &LuaSprite::setUVRectValue);
  sprite_type["bringToFront"] = &LuaSprite::bringToFront;
}

}  // namespace gmp::gothic
