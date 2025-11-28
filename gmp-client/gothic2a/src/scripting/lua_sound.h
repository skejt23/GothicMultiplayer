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

#include "ZenGin/zGothicAPI.h"

namespace gmp::gothic {

class LuaSound {
 public:
  explicit LuaSound(const std::string& filename);
  ~LuaSound();

  void play();
  void stop();
  bool isPlaying() const;

  std::string getFile() const;
  void setFile(const std::string& filename);

  float getPlayingTime() const;

  float getVolume() const;
  void setVolume(float volume);

  bool getLooping() const;
  void setLooping(bool looping);

  float getBalance() const;
  void setBalance(float balance);

 private:
  void ReloadSound();
  void ApplyProperties();
  void StopIfNeeded();

  std::string file_;
  float volume_;
  bool looping_;
  float balance_;
  int handle_;

  Gothic_II_Addon::zCSoundFX* sound_fx_;
};

}  // namespace gmp::gothic