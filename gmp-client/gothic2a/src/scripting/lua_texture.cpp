#include "lua_texture.h"

#include <algorithm>
#include <unordered_set>

#include "ZenGin/zGothicAPI.h"

using namespace Gothic_II_Addon;

namespace gmp::gothic {

class LuaTextureView : public zCView {
 public:
  LuaTextureView(LuaTexture& owner, int x, int y, int width, int height)
      : zCView(x, y, x + width, y + height, VIEW_ITEM), owner_(owner) {}

  void Blit() override { owner_.Blit(); }

 private:
  LuaTexture& owner_;
};

std::unordered_set<LuaTexture*> LuaTexture::active_textures_;

LuaTexture::LuaTexture(int x, int y, int width, int height, const std::string& file)
    : view_(nullptr),
      texture_(nullptr),
      uvPos_(0.0f, 0.0f),
      uvSize_(1.0f, 1.0f),
      color_(255, 255, 255, 255),
      alphaFunc_(zRND_ALPHA_FUNC_BLEND),
      fillZ_(false),
      visible_(true),
      fileName_(file),
      attached_to_screen_(false) {
  active_textures_.insert(this);
  view_ = new LuaTextureView(*this, x, y, width, height);

  setFile(file);

  if (screen && view_) {
    screen->InsertItem(view_);
    attached_to_screen_ = true;
  }
}

LuaTexture::~LuaTexture() {
  if (screen && view_ && attached_to_screen_) {
    screen->RemoveItem(view_);
    attached_to_screen_ = false;
  }

  delete view_;
  view_ = nullptr;

  active_textures_.erase(this);
}

sol::table LuaTexture::getPosition(sol::this_state s) {
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

void LuaTexture::setPosition(int x, int y) {
  updateViewPos(x, y);
}

sol::table LuaTexture::getPositionPx(sol::this_state s) {
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

void LuaTexture::setPositionPx(int x, int y) {
  if (!screen) {
    return;
  }
  updateViewPos(screen->anx(x), screen->any(y));
}

sol::table LuaTexture::getSize(sol::this_state s) {
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

void LuaTexture::setSize(int width, int height) {
  updateViewSize(width, height);
}

sol::table LuaTexture::getSizePx(sol::this_state s) {
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

void LuaTexture::setSizePx(int width, int height) {
  if (!screen) {
    return;
  }
  updateViewSize(screen->anx(width), screen->any(height));
}

sol::table LuaTexture::getRect(sol::this_state s) {
  sol::state_view lua(s);
  sol::table rect = lua.create_table();
  int width = 0;
  int height = 0;
  if (view_) {
    view_->GetSize(width, height);
  }
  rect["width"] = static_cast<int>(uvSize_[VX] * width);
  rect["height"] = static_cast<int>(uvSize_[VY] * height);
  return rect;
}

void LuaTexture::setRect(int x, int y, int width, int height) {
  int virtualWidth = 0;
  int virtualHeight = 0;
  if (!view_) {
    return;
  }

  view_->GetSize(virtualWidth, virtualHeight);
  if (virtualWidth == 0 || virtualHeight == 0) {
    return;
  }

  uvPos_[VX] = static_cast<float>(x) / virtualWidth;
  uvPos_[VY] = static_cast<float>(y) / virtualHeight;
  uvSize_[VX] = uvPos_[VX] + static_cast<float>(width) / virtualWidth;
  uvSize_[VY] = uvPos_[VY] + static_cast<float>(height) / virtualHeight;
}

sol::table LuaTexture::getRectPx(sol::this_state s) {
  sol::state_view lua(s);
  sol::table rect = lua.create_table();
  int pixelWidth = 0;
  int pixelHeight = 0;
  if (view_ && screen) {
    view_->GetSize(pixelWidth, pixelHeight);
    pixelWidth = screen->nax(pixelWidth);
    pixelHeight = screen->nay(pixelHeight);
  }
  rect["width"] = static_cast<int>(uvSize_[VX] * pixelWidth);
  rect["height"] = static_cast<int>(uvSize_[VY] * pixelHeight);
  return rect;
}

void LuaTexture::setRectPx(int x, int y, int width, int height) {
  if (!screen) {
    return;
  }

  setRect(screen->anx(x), screen->any(y), screen->anx(width), screen->any(height));
}

sol::table LuaTexture::getColor(sol::this_state s) {
  sol::state_view lua(s);
  sol::table colorTable = lua.create_table();
  colorTable["r"] = color_.r;
  colorTable["g"] = color_.g;
  colorTable["b"] = color_.b;
  return colorTable;
}

void LuaTexture::setColor(unsigned char r, unsigned char g, unsigned char b) {
  color_.SetRGB(r, g, b);
}

unsigned char LuaTexture::getAlpha() const { return color_.alpha; }

void LuaTexture::setAlpha(unsigned char alpha) { color_.alpha = alpha; }

std::string LuaTexture::getFile() const { return fileName_; }

void LuaTexture::setFile(const std::string& file) {
  fileName_ = file;
  zSTRING fileString(file.c_str());
  texture_ = zCTexture::Load(fileString, 0);
  if (view_) {
    view_->InsertBack(fileString);
  }
}

void LuaTexture::setVisible(bool visible) { visible_ = visible; }

bool LuaTexture::getVisible() const { return visible_; }

void LuaTexture::top() {
  if (view_) {
    view_->Top();
  }
}

void LuaTexture::render() {
  if (view_) {
    view_->Blit();
  }
}

void LuaTexture::CleanupViews() {
  if (!screen) {
    return;
  }

  for (auto* texture : active_textures_) {
    if (texture && texture->view_ && texture->attached_to_screen_) {
      screen->RemoveItem(texture->view_);
      texture->attached_to_screen_ = false;
    }
  }
}

void LuaTexture::Blit() {
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

  zBOOL oldzWrite = zrenderer->GetZBufferWriteEnabled();
  zrenderer->SetZBufferWriteEnabled(fillZ_);

  zTRnd_ZBufferCmp oldCmp = zrenderer->GetZBufferCompare();
  zrenderer->SetZBufferCompare(zRND_ZBUFFER_CMP_ALWAYS);

  zTRnd_AlphaBlendFunc oldBlendFunc = zrenderer->GetAlphaBlendFunc();
  zrenderer->SetAlphaBlendFunc(alphaFunc_);

  zBOOL oldBilerpFilter = zrenderer->GetBilerpFilterEnabled();
  zrenderer->SetBilerpFilterEnabled(oldBilerpFilter);

  zREAL farZ;
  if (fillZ_) {
    farZ = (zCCamera::activeCam) ? zCCamera::activeCam->farClipZ - 1 : 65534.0f;
  } else {
    farZ = (zCCamera::activeCam) ? zCCamera::activeCam->nearClipZ + 1 : 1.0f;
  }

  zrenderer->DrawTile(texture_, posMin, posMax, farZ, uvPos_, uvSize_, color_);

  zrenderer->SetBilerpFilterEnabled(oldBilerpFilter);
  zrenderer->SetAlphaBlendFunc(oldBlendFunc);
  zrenderer->SetZBufferWriteEnabled(oldzWrite);
  zrenderer->SetZBufferCompare(oldCmp);
}

void LuaTexture::updateViewSize(int width, int height) {
  if (!view_) {
    return;
  }
  view_->SetSize(width, height);
}

void LuaTexture::updateViewPos(int x, int y) {
  if (!view_) {
    return;
  }
  view_->SetPos(x, y);
}

}  // namespace gmp::gothic