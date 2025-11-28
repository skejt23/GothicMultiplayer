#pragma once

#include <string>
#include <unordered_set>
#include "sol/sol.hpp"
#include "ZenGin/zGothicAPI.h"

namespace gmp::gothic {

class LuaTextureView;

class LuaTexture {
 public:
  friend class LuaTextureView;

  LuaTexture(int x, int y, int width, int height, const std::string& file);
  ~LuaTexture();

  static void CleanupViews();

  sol::table getPosition(sol::this_state s);
  void setPosition(int x, int y);

  sol::table getPositionPx(sol::this_state s);
  void setPositionPx(int x, int y);

  sol::table getSize(sol::this_state s);
  void setSize(int width, int height);

  sol::table getSizePx(sol::this_state s);
  void setSizePx(int width, int height);

  sol::table getRect(sol::this_state s);
  void setRect(int x, int y, int width, int height);

  sol::table getRectPx(sol::this_state s);
  void setRectPx(int x, int y, int width, int height);

  sol::table getColor(sol::this_state s);
  void setColor(unsigned char r, unsigned char g, unsigned char b);

  unsigned char getAlpha() const;
  void setAlpha(unsigned char alpha);

  std::string getFile() const;
  void setFile(const std::string& file);

  void setVisible(bool visible);
  bool getVisible() const;

  void top();
  void render();
  void Blit();

 private:

  LuaTextureView* view_;
  Gothic_II_Addon::zCTexture* texture_;
  Gothic_II_Addon::zVEC2 uvPos_;
  Gothic_II_Addon::zVEC2 uvSize_;
  Gothic_II_Addon::zCOLOR color_;
  Gothic_II_Addon::zTRnd_AlphaBlendFunc alphaFunc_;
  bool fillZ_;
  bool visible_;
  std::string fileName_;
  bool attached_to_screen_;

  static std::unordered_set<LuaTexture*> active_textures_;

  void updateViewSize(int width, int height);
  void updateViewPos(int x, int y);
};

}  // namespace gmp::gothic