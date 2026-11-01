#pragma once

#include <string>
#include <unordered_set>

#include "ZenGin/zGothicAPI.h"
#include "sol/sol.hpp"

namespace gmp::gothic {

class LuaSpriteView;

class LuaSprite {
public:
  friend class LuaSpriteView;

  LuaSprite(int x, int y, int width, int height, const std::string& file);
  LuaSprite(const LuaSprite&) = delete;
  LuaSprite& operator=(const LuaSprite&) = delete;
  LuaSprite(LuaSprite&& other) noexcept;
  LuaSprite& operator=(LuaSprite&& other) noexcept;
  ~LuaSprite();

  static void CleanupViews();
  static void EnsureViewsAttached();

  sol::table getPosition(sol::this_state s);
  void setPosition(int x, int y);
  void setPositionValue(sol::object value);

  sol::table getPositionPx(sol::this_state s);
  void setPositionPx(int x, int y);
  void setPositionPxValue(sol::object value);

  sol::table getSize(sol::this_state s);
  void setSize(int width, int height);
  void setSizeValue(sol::object value);

  sol::table getSizePx(sol::this_state s);
  void setSizePx(int width, int height);
  void setSizePxValue(sol::object value);

  sol::table getUVRect(sol::this_state s) const;
  void setUVRect(float u, float v, float width, float height);
  void setUVRectValue(sol::table value);

  sol::table getColor(sol::this_state s);
  void setColor(int r, int g, int b);
  void setColorValue(sol::object value);

  int getAlpha() const;
  void setAlpha(int alpha);

  std::string getFile() const;
  void setFile(const std::string& file);

  void setVisible(bool visible);
  bool getVisible() const;

  void bringToFront();
  void Blit();

private:
  LuaSpriteView* view_;
  Gothic_II_Addon::zCTexture* texture_;
  Gothic_II_Addon::zVEC2 uv_min_;
  Gothic_II_Addon::zVEC2 uv_max_;
  Gothic_II_Addon::zCOLOR color_;
  Gothic_II_Addon::zTRnd_AlphaBlendFunc alphaFunc_;
  bool fillZ_;
  bool visible_;
  std::string fileName_;
  bool attached_to_screen_;

  static std::unordered_set<LuaSprite*> active_sprites_;

  void Destroy();
  void EnsureAttached();
  void updateViewSize(int width, int height);
  void updateViewPos(int x, int y);
};

void BindSprite(sol::state& lua);

}  // namespace gmp::gothic
