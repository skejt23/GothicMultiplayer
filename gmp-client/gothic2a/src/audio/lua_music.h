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

#ifndef GMP_CLIENT_GOTHIC2A_LIB_AUDIO_LUA_MUSIC_H_
#define GMP_CLIENT_GOTHIC2A_LIB_AUDIO_LUA_MUSIC_H_

#include <memory>
#include <string>

#include <sol/sol.hpp>

#include "music_player.h"

namespace gmp::lua {

/**
 * @brief Lua wrapper for MusicPlayer that provides a script-friendly API.
 *
 * Example Lua usage:
 *   local music = Music.new("music/theme.mp3")
 *   music.volume = 0.8
 *   music.looping = true
 *   music:play()
 *
 *   -- Check status
 *   if music:isPlaying() then
 *       print("Position: " .. music.position .. "s / " .. music.duration .. "s")
 *   end
 *
 *   music:stop()
 */
class LuaMusic {
 public:
  explicit LuaMusic(const std::string& filepath);
  ~LuaMusic();

  // Playback control
  void play();
  void playLooped();
  void pause();
  void resume();
  void stop();

  // Status
  bool isPlaying() const;
  bool isPaused() const;

  // Properties (getters)
  std::string getFile() const;
  float getVolume() const;
  bool getLooping() const;
  float getPosition() const;
  float getDuration() const;
  bool getMuteGothic() const;
  float getOptionsVolume() const;
  bool getUseOptionsVolume() const;

  // Properties (setters)
  void setFile(const std::string& filepath);
  void setVolume(float volume);
  void setLooping(bool looping);
  void setPosition(float position);
  void setMuteGothic(bool mute);
  void setUseOptionsVolume(bool use);

 private:
  std::unique_ptr<audio::MusicPlayer> player_;
  std::string filepath_;
  bool looping_ = false;
};

/// Bind Music class to Lua state
void BindMusic(sol::state& lua);

}  // namespace gmp::lua

#endif  // GMP_CLIENT_GOTHIC2A_LIB_AUDIO_LUA_MUSIC_H_
