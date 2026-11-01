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

#include "lua_music.h"

#include <spdlog/spdlog.h>

namespace gmp::lua {

/* luagmp (class)
 *
 * This class controls streamed music playback on the client. For the native Gothic music system, use [GothicMusic](GothicMusic.md).
 *
 * @version  0.3.0
 * @name     Music
 * @side     client
 * @category Game
 *
 */
/* luagmp (constructor)
 *
 * Creates a new Music object from a file path. Available formats are `OGG`, `MP3` and `WAV`.
 *
 * @note Accepts only filenames and subdirectory paths relative to the music roots, such as `MAINMENU.MP3` or `Ambient/forest.mp3`. Roots are `Multiplayer/Music/` and `_Work/Data/Music/`; do not include either prefix.
 * @param    (string) file  Music file path.
 *
 */
LuaMusic::LuaMusic(const std::string& filepath) : player_(std::make_unique<audio::MusicPlayer>()), filepath_(filepath) {
  if (!filepath.empty()) {
    if (!player_->Load(filepath)) {
      SPDLOG_ERROR("LuaMusic: Failed to load '{}'", filepath);
    }
  }
}

LuaMusic::~LuaMusic() {
  if (player_) {
    player_->Stop();
  }
}

/* luagmp (method)
 *
 * This method will start music playback using the current looping state.
 *
 * @name     play
 *
 */
void LuaMusic::play() {
  if (player_) {
    player_->Play(looping_);
  }
}

/* luagmp (method)
 *
 * This method will enable looping and start music playback.
 *
 * @name     playLooped
 *
 */
void LuaMusic::playLooped() {
  looping_ = true;
  if (player_) {
    player_->Play(true);
  }
}

/* luagmp (method)
 *
 * This method will pause music playback.
 *
 * @name     pause
 *
 */
void LuaMusic::pause() {
  if (player_) {
    player_->Pause();
  }
}

/* luagmp (method)
 *
 * This method will resume paused music playback.
 *
 * @name     resume
 *
 */
void LuaMusic::resume() {
  if (player_) {
    player_->Resume();
  }
}

/* luagmp (method)
 *
 * This method will stop music playback.
 *
 * @name     stop
 *
 */
void LuaMusic::stop() {
  if (player_) {
    player_->Stop();
  }
}

/* luagmp (method)
 *
 * This method will return whether the music is currently playing.
 *
 * @name     isPlaying
 * @return   (boolean) True if music is playing.
 *
 */
bool LuaMusic::isPlaying() const {
  return player_ && player_->IsPlaying();
}

/* luagmp (method)
 *
 * This method will return whether the music is currently paused.
 *
 * @name     isPaused
 * @return   (boolean) True if music is paused.
 *
 */
bool LuaMusic::isPaused() const {
  return player_ && player_->IsPaused();
}

/* luagmp (property)
 *
 * Represents the music file path.
 *
 * @name     file
 * @return   (string) Music file path.
 *
 */
std::string LuaMusic::getFile() const {
  return filepath_;
}

/* luagmp (property)
 *
 * Represents the playback volume before options volume is applied.
 *
 * @name     volume
 * @return   (number) Volume level (0.0 - 1.0).
 *
 */
float LuaMusic::getVolume() const {
  return player_ ? player_->GetVolume() : 0.0f;
}

/* luagmp (property)
 *
 * Represents whether playback should loop.
 *
 * @name     looping
 * @return   (boolean)
 *
 */
bool LuaMusic::getLooping() const {
  return looping_;
}

/* luagmp (property)
 *
 * Represents the current playback position in seconds.
 *
 * @name     position
 * @return   (number) Playback position in seconds.
 *
 */
float LuaMusic::getPosition() const {
  return player_ ? player_->GetPosition() : 0.0f;
}

/* luagmp (property)
 *
 * Represents the music duration in seconds.
 *
 * @name     duration
 * @readonly
 * @return   (number) Duration in seconds.
 *
 */
float LuaMusic::getDuration() const {
  return player_ ? player_->GetDuration() : 0.0f;
}

/* luagmp (property)
 *
 * Represents whether Gothic's original music should be muted while this music plays.
 *
 * @name     muteGothicMusic
 * @return   (boolean)
 *
 */
bool LuaMusic::getMuteGothic() const {
  return player_ ? player_->GetMuteGothicMusic() : true;
}

/* luagmp (property)
 *
 * Represents the current Gothic music options volume multiplier.
 *
 * @name     optionsVolume
 * @readonly
 * @return   (number) Options volume multiplier.
 *
 */
float LuaMusic::getOptionsVolume() const {
  return player_ ? player_->GetOptionsVolume() : 1.0f;
}

/* luagmp (property)
 *
 * Represents whether the Gothic music options volume should affect this music.
 *
 * @name     useOptionsVolume
 * @return   (boolean)
 *
 */
bool LuaMusic::getUseOptionsVolume() const {
  return player_ ? player_->GetUseOptionsVolume() : true;
}

/// Assigns the file property using the same value format as its getter.
void LuaMusic::setFile(const std::string& filepath) {
  if (filepath == filepath_) {
    return;
  }

  filepath_ = filepath;
  if (player_) {
    player_->Stop();
    if (!player_->Load(filepath)) {
      SPDLOG_ERROR("LuaMusic: Failed to load '{}'", filepath);
    }
  }
}

/// Assigns the volume property using the same value format as its getter.
void LuaMusic::setVolume(float volume) {
  if (player_) {
    player_->SetVolume(volume);
  }
}

/// Assigns the looping property using the same value format as its getter.
void LuaMusic::setLooping(bool looping) {
  looping_ = looping;
}

/// Assigns the position property using the same value format as its getter.
void LuaMusic::setPosition(float position) {
  if (player_) {
    player_->Seek(position);
  }
}

/// Assigns the muteGothicMusic property using the same value format as its getter.
void LuaMusic::setMuteGothic(bool mute) {
  if (player_) {
    player_->SetMuteGothicMusic(mute);
  }
}

/// Assigns the useOptionsVolume property using the same value format as its getter.
void LuaMusic::setUseOptionsVolume(bool use) {
  if (player_) {
    player_->SetUseOptionsVolume(use);
  }
}

void BindMusic(sol::state& lua) {
  sol::usertype<LuaMusic> music_type = lua.new_usertype<LuaMusic>("Music", sol::constructors<LuaMusic(const std::string&)>());

  // Methods
  music_type["play"] = &LuaMusic::play;
  music_type["playLooped"] = &LuaMusic::playLooped;
  music_type["pause"] = &LuaMusic::pause;
  music_type["resume"] = &LuaMusic::resume;
  music_type["stop"] = &LuaMusic::stop;
  music_type["isPlaying"] = &LuaMusic::isPlaying;
  music_type["isPaused"] = &LuaMusic::isPaused;

  // Properties
  music_type["file"] = sol::property(&LuaMusic::getFile, &LuaMusic::setFile);
  music_type["volume"] = sol::property(&LuaMusic::getVolume, &LuaMusic::setVolume);
  music_type["looping"] = sol::property(&LuaMusic::getLooping, &LuaMusic::setLooping);
  music_type["position"] = sol::property(&LuaMusic::getPosition, &LuaMusic::setPosition);
  music_type["duration"] = sol::property(&LuaMusic::getDuration);
  music_type["muteGothicMusic"] = sol::property(&LuaMusic::getMuteGothic, &LuaMusic::setMuteGothic);
  music_type["optionsVolume"] = sol::property(&LuaMusic::getOptionsVolume);
  music_type["useOptionsVolume"] = sol::property(&LuaMusic::getUseOptionsVolume, &LuaMusic::setUseOptionsVolume);

  SPDLOG_DEBUG("Music Lua bindings registered");
}

}  // namespace gmp::lua
