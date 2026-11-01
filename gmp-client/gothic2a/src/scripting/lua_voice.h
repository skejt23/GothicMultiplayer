/*
MIT License

Copyright (c) 2026 Gothic Multiplayer Team.

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

#include <cstdint>
#include <string>
#include <vector>

#include "sol/sol.hpp"

namespace gmp::gothic {

class LuaVoice {
public:
  bool isAvailable();
  bool isEnabled();
  bool setScriptEnabled(bool enabled);
  float getRange();
  bool setLocalRangeLimit(float range);
  float getOutputVolume();
  void setOutputVolume(float volume);
  int getPushToTalkKey();
  bool setPushToTalkKey(int key);
  sol::as_table_t<std::vector<std::string>> getInputDevices();
  std::string getInputDevice();
  bool setInputDevice(const std::string& device_name);
  sol::as_table_t<std::vector<std::string>> getOutputDevices();
  std::string getOutputDevice();
  bool setOutputDevice(const std::string& device_name);
  std::string getChannel();
  bool setChannel(const std::string& channel);
  bool isTransmitting();
};

void BindVoice(sol::state& lua);

}  // namespace gmp::gothic
