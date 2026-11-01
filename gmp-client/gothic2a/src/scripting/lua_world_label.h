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
#include <vector>

#include "ZenGin/zGothicAPI.h"
#include "sol/sol.hpp"

namespace gmp::gothic {

class LuaWorldLabelView;

class LuaWorldLabel {
public:
  LuaWorldLabel();
  LuaWorldLabel(float x, float y, float z);
  LuaWorldLabel(float x, float y, float z, const std::string& text);
  ~LuaWorldLabel();

  static void CleanupViews();
  static void RenderActiveLabels();

  void setPosition(float x, float y, float z);
  void setPositionValue(sol::object value);
  sol::table getPosition(sol::this_state s) const;

  void appendLine(const std::string& text);
  void clearLines();
  sol::table getLines(sol::this_state s) const;
  void setLines(sol::table lines);
  std::string getText() const;
  void setText(const std::string& text);
  bool removeLine(int index);
  bool setLine(int index, const std::string& text);
  sol::object getLine(int index, sol::this_state s) const;

  void setFont(const std::string& font_name);
  std::string getFont() const;

  void setColor(int r, int g, int b);
  void setColorValue(sol::object value);
  sol::table getColor(sol::this_state s) const;

  void setAlpha(int alpha);
  int getAlpha() const;

  void setVisible(bool visible);
  bool getVisible() const;

  void setMaxRenderDistance(float distance);
  float getMaxRenderDistance() const;

  void bringToFront();
  void render();

private:
  friend class LuaWorldLabelView;

  void Initialize();
  void Blit();

  LuaWorldLabelView* view_;
  std::vector<std::string> lines_;
  std::string font_name_;
  Gothic_II_Addon::zVEC3 position_;
  Gothic_II_Addon::zCOLOR color_;
  bool visible_;
  bool attached_to_screen_;
  float max_render_distance_;

  static std::unordered_set<LuaWorldLabel*> active_labels_;
};

void BindWorldLabel(sol::state& lua);

}  // namespace gmp::gothic
