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

#include "lua_sound.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <utility>

#include "lua_helpers.h"

namespace gmp::gothic {

/* luagmp (class)
 *
 * This class controls playback of a non-positional sound.
 *
 * @version  0.3.0
 * @name     Sound
 * @side     client
 * @category Game
 *
 */
/* luagmp (constructor)
 *
 * Creates a new Sound from a file.
 *
 * @param    (string) file     Sound file path.
 *
 */

LuaSound::LuaSound(const std::string& filename) : file_(filename), volume_(1.0f), looping_(false), balance_(0.0f), handle_(-1), sound_fx_(nullptr) {
  ReloadSound();
}

LuaSound::LuaSound(LuaSound&& other) noexcept
    : file_(std::move(other.file_)),
      volume_(other.volume_),
      looping_(other.looping_),
      balance_(other.balance_),
      handle_(other.handle_),
      sound_fx_(other.sound_fx_) {
  other.handle_ = -1;
  other.sound_fx_ = nullptr;
}

LuaSound& LuaSound::operator=(LuaSound&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  StopIfNeeded();
  if (sound_fx_) {
    sound_fx_->Release();
  }

  file_ = std::move(other.file_);
  volume_ = other.volume_;
  looping_ = other.looping_;
  balance_ = other.balance_;
  handle_ = other.handle_;
  sound_fx_ = other.sound_fx_;

  other.handle_ = -1;
  other.sound_fx_ = nullptr;
  return *this;
}

LuaSound::~LuaSound() {
  StopIfNeeded();

  if (sound_fx_) {
    sound_fx_->Release();
    sound_fx_ = nullptr;
  }
}

void LuaSound::ReloadSound() {
  StopIfNeeded();

  if (sound_fx_) {
    sound_fx_->Release();
    sound_fx_ = nullptr;
  }

  if (!zsound) {
    SPDLOG_ERROR("Sound system is unavailable; cannot load sound '{}'.", file_);
    sound_fx_ = nullptr;
    return;
  }

  Gothic_II_Addon::zSTRING soundName = file_.c_str();
  sound_fx_ = zsound->LoadSoundFX(soundName);
  if (!sound_fx_) {
    SPDLOG_ERROR("Failed to load sound FX from '{}'.", file_);
    return;
  }

  ApplyProperties();
}

void LuaSound::ApplyProperties() {
  if (!sound_fx_) {
    return;
  }

  sound_fx_->SetVolume(volume_);
  sound_fx_->SetPan(balance_);
  sound_fx_->SetLooping(looping_ ? 1 : 0);

  if (handle_ >= 0 && zsound) {
    zsound->UpdateSoundProps(handle_, Gothic_II_Addon::zSND_FREQ_DEFAULT, volume_, balance_);
  }
}

void LuaSound::StopIfNeeded() {
  if (handle_ >= 0 && zsound) {
    zsound->StopSound(handle_);
  }
  handle_ = -1;
}

/* luagmp (method)
 *
 * This method will start Sound playback.
 *
 * @name     play
 *
 */
void LuaSound::play() {
  if (!sound_fx_) {
    ReloadSound();
  }

  if (!zsound || !sound_fx_) {
    SPDLOG_ERROR("Cannot play sound: zsound={}, sound_fx_={}", (void*)zsound, (void*)sound_fx_);
    return;
  }

  StopIfNeeded();

  // Set looping before playback
  sound_fx_->SetLooping(looping_ ? 1 : 0);
  sound_fx_->SetVolume(volume_);
  sound_fx_->SetPan(balance_);

  handle_ = zsound->PlaySound(sound_fx_, 0, Gothic_II_Addon::zSND_FREQ_DEFAULT, volume_, balance_);
}

/* luagmp (method)
 *
 * This method will stop Sound playback.
 *
 * @name     stop
 *
 */
void LuaSound::stop() {
  StopIfNeeded();
}

/* luagmp (method)
 *
 * This method will return whether the Sound is currently playing.
 *
 * @name     isPlaying
 * @return   (boolean) True if the sound is playing.
 *
 */
bool LuaSound::isPlaying() {
  if (handle_ < 0 || !zsound) {
    return false;
  }

  if (zsound->IsSoundActive(handle_) != 0) {
    return true;
  }

  handle_ = -1;
  return false;
}


/* luagmp (property)
 *
 * Represents the sound file path.
 *
 * @name     file
 * @return   (string)
 *
 */
std::string LuaSound::getFile() const {
  return file_;
}

/// Assigns the file property using the same value format as its getter.
void LuaSound::setFile(const std::string& filename) {
  if (filename == file_) {
    return;
  }

  file_ = filename;
  ReloadSound();
}

/* luagmp (property)
 *
 * Represents the total sound duration in seconds.
 *
 * @name     duration
 * @readonly
 * @return   (number)
 *
 */
float LuaSound::getDuration() const {
  if (sound_fx_) {
    return sound_fx_->GetPlayingTimeMSEC() / 1000.0f;
  }

  if (zsound) {
    return zsound->GetPlayingTimeMSEC(file_.c_str()) / 1000.0f;
  }

  return 0.0f;
}

/* luagmp (property)
 * Playback volume from 0.0 to 1.0.
 *
 * @name     volume
 * @return   (number)
 *
 */
float LuaSound::getVolume() const {
  return volume_;
}

/// Assigns the volume property using the same value format as its getter.
void LuaSound::setVolume(float volume) {
  volume_ = std::clamp(volume, lua_helpers::kSoundMinVolume, lua_helpers::kSoundMaxVolume);
  ApplyProperties();
}

/* luagmp (property)
 *
 * Represents whether the sound should loop.
 *
 * @name     looping
 * @return   (boolean)
 *
 */
bool LuaSound::getLooping() const {
  return looping_;
}

/// Assigns the looping property using the same value format as its getter.
void LuaSound::setLooping(bool looping) {
  if (looping_ == looping) {
    return;
  }

  const bool was_playing = isPlaying();
  looping_ = looping;
  ApplyProperties();

  if (was_playing) {
    StopIfNeeded();
    play();
  }
}

/* luagmp (property)
 * Stereo balance from -1 (left) through 0 (center) to 1 (right).
 *
 * @name balance
 * @return (number)
 */
float LuaSound::getBalance() const {
  return balance_;
}

/// Assigns the balance property using the same value format as its getter.
void LuaSound::setBalance(float balance) {
  balance_ = std::clamp(balance, lua_helpers::kSoundMinBalance, lua_helpers::kSoundMaxBalance);
  ApplyProperties();
}

void BindSound(sol::state& lua) {
  sol::usertype<LuaSound> sound_type = lua.new_usertype<LuaSound>("Sound", sol::constructors<LuaSound(const std::string&)>());

  sound_type["play"] = &LuaSound::play;
  sound_type["stop"] = &LuaSound::stop;
  sound_type["isPlaying"] = &LuaSound::isPlaying;

  sound_type["file"] = sol::property(&LuaSound::getFile, &LuaSound::setFile);
  sound_type["duration"] = sol::property(&LuaSound::getDuration);
  sound_type["volume"] = sol::property(&LuaSound::getVolume, &LuaSound::setVolume);
  sound_type["looping"] = sol::property(&LuaSound::getLooping, &LuaSound::setLooping);
  sound_type["balance"] = sol::property(&LuaSound::getBalance, &LuaSound::setBalance);
}

}  // namespace gmp::gothic
