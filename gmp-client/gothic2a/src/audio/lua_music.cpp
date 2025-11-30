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

LuaMusic::LuaMusic(const std::string& filepath)
    : player_(std::make_unique<audio::MusicPlayer>()), filepath_(filepath) {
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

void LuaMusic::play() {
  if (player_) {
    player_->Play(looping_);
  }
}

void LuaMusic::playLooped() {
  looping_ = true;
  if (player_) {
    player_->Play(true);
  }
}

void LuaMusic::pause() {
  if (player_) {
    player_->Pause();
  }
}

void LuaMusic::resume() {
  if (player_) {
    player_->Resume();
  }
}

void LuaMusic::stop() {
  if (player_) {
    player_->Stop();
  }
}

bool LuaMusic::isPlaying() const {
  return player_ && player_->IsPlaying();
}

bool LuaMusic::isPaused() const {
  return player_ && player_->IsPaused();
}

std::string LuaMusic::getFile() const {
  return filepath_;
}

float LuaMusic::getVolume() const {
  return player_ ? player_->GetVolume() : 0.0f;
}

bool LuaMusic::getLooping() const {
  return looping_;
}

float LuaMusic::getPosition() const {
  return player_ ? player_->GetPosition() : 0.0f;
}

float LuaMusic::getDuration() const {
  return player_ ? player_->GetDuration() : 0.0f;
}

bool LuaMusic::getMuteGothic() const {
  return player_ ? player_->GetMuteGothicMusic() : true;
}

float LuaMusic::getOptionsVolume() const {
  return player_ ? player_->GetOptionsVolume() : 1.0f;
}

bool LuaMusic::getUseOptionsVolume() const {
  return player_ ? player_->GetUseOptionsVolume() : true;
}

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

void LuaMusic::setVolume(float volume) {
  if (player_) {
    player_->SetVolume(volume);
  }
}

void LuaMusic::setLooping(bool looping) {
  looping_ = looping;
}

void LuaMusic::setPosition(float position) {
  if (player_) {
    player_->Seek(position);
  }
}

void LuaMusic::setMuteGothic(bool mute) {
  if (player_) {
    player_->SetMuteGothicMusic(mute);
  }
}

void LuaMusic::setUseOptionsVolume(bool use) {
  if (player_) {
    player_->SetUseOptionsVolume(use);
  }
}

void BindMusic(sol::state& lua) {
  sol::usertype<LuaMusic> music_type = lua.new_usertype<LuaMusic>(
      "Music",
      sol::constructors<LuaMusic(const std::string&)>());

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
