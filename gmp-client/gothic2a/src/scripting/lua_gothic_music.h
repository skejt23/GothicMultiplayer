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

#include "sol/sol.hpp"

namespace gmp::gothic {

class LuaGothicMusic {
public:
  bool loadTheme(const std::string& file_name);
  bool playTheme();
  void stopTheme();

  std::string getFileName();
  void setFileName(const std::string& file_name);

  float getVolume();
  void setVolume(float volume);

  bool getLooping();
  void setLooping(bool loop);

  bool getEnabled();
  void setEnabled(bool disabled);

  sol::object getActiveThemeName(sol::this_state ts);

  float getReverbMix();
  void setReverbMix(float value);

  float getReverbTime();
  void setReverbTime(float value);

  int getTransType();
  void setTransType(int value);

  int getTransSubType();
  void setTransSubType(int value);

  std::string getName();
  void setName(const std::string& name);
};

void BindGothicMusic(sol::state& lua);

}  // namespace gmp::gothic
