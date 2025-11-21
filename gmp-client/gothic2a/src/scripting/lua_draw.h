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
#include <unordered_set>

#include "ZenGin/zGothicAPI.h"
#include "sol/sol.hpp"

namespace gmp::gothic {

class LuaDrawView;

  class LuaDraw {
 public:
  LuaDraw();
  LuaDraw(int x, int y, const std::string& text);
  ~LuaDraw();

  static void CleanupViews();

  void setPosition(int x, int y);
  void setPositionPx(int x, int y);
  sol::table getPosition(sol::this_state s);
  sol::table getPositionPx(sol::this_state s);

  void setText(const std::string& text);
  std::string getText() const;

  void setFont(const std::string& fontName);
  std::string getFont() const;

  void setColor(int r, int g, int b);
  sol::table getColor(sol::this_state s);

  void setAlpha(int a);
  int getAlpha() const;

  void setVisible(bool visible);
  bool getVisible() const;

  void render();

 private:
  friend class LuaDrawView;

  void Blit();
  void Initialize();

  LuaDrawView* view_;
  std::string text_;
  std::string fontName_;
  int posX_;
  int posY_;
  Gothic_II_Addon::zCOLOR color_;
  bool visible_;
  bool attached_to_screen_;

  static std::unordered_set<LuaDraw*> active_draws_;
};

}  // namespace gmp::gothic