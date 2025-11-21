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

#include <algorithm>

#include <spdlog/spdlog.h>

namespace gmp::gothic {
namespace {
constexpr float kMinVolume = 0.0f;
constexpr float kMaxVolume = 1.0f;
constexpr float kMinBalance = -1.0f;
constexpr float kMaxBalance = 1.0f;
}

LuaSound::LuaSound(const std::string& filename)
    : file_(filename), volume_(1.0f), looping_(false), balance_(0.0f), handle_(-1), sound_fx_(nullptr) {
  ReloadSound();
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

  SPDLOG_INFO("Attempting to load sound: '{}'", file_);
  
  Gothic_II_Addon::zSTRING soundName = file_.c_str();
  sound_fx_ = zsound->LoadSoundFX(soundName);
  if (!sound_fx_) {
    SPDLOG_ERROR("Failed to load sound FX from '{}'.", file_);
    return;
  }
  
  SPDLOG_INFO("Successfully loaded sound: '{}'", file_);
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

void LuaSound::play() {
  if (!sound_fx_) {
    ReloadSound();
  }

  if (!zsound || !sound_fx_) {
    SPDLOG_ERROR("Cannot play sound: zsound={}, sound_fx_={}", 
                 (void*)zsound, (void*)sound_fx_);
    return;
  }

  StopIfNeeded();
  
  // Set looping before playback
  sound_fx_->SetLooping(looping_ ? 1 : 0);
  sound_fx_->SetVolume(volume_);
  sound_fx_->SetPan(balance_);
  
  SPDLOG_INFO("Attempting to play sound: '{}' (vol={}, pan={}, loop={})",
              file_, volume_, balance_, looping_);
  
  handle_ = zsound->PlaySound(sound_fx_, 0, Gothic_II_Addon::zSND_FREQ_DEFAULT, volume_, balance_);
  
  SPDLOG_INFO("PlaySound returned handle: {}", handle_);
}

void LuaSound::stop() {
  StopIfNeeded();
}

bool LuaSound::isPlaying() const {
  return handle_ >= 0 && zsound && zsound->IsSoundActive(handle_) != 0;
}

std::string LuaSound::getFile() const {
  return file_;
}

void LuaSound::setFile(const std::string& filename) {
  if (filename == file_) {
    return;
  }

  file_ = filename;
  ReloadSound();
}

float LuaSound::getPlayingTime() const {
  if (sound_fx_) {
    return sound_fx_->GetPlayingTimeMSEC() / 1000.0f;
  }

  if (zsound) {
    return zsound->GetPlayingTimeMSEC(file_.c_str()) / 1000.0f;
  }

  return 0.0f;
}

float LuaSound::getVolume() const {
  return volume_;
}

void LuaSound::setVolume(float volume) {
  volume_ = std::clamp(volume, kMinVolume, kMaxVolume);
  ApplyProperties();
}

bool LuaSound::getLooping() const {
  return looping_;
}

void LuaSound::setLooping(bool looping) {
  looping_ = looping;
  ApplyProperties();
}

float LuaSound::getBalance() const {
  return balance_;
}

void LuaSound::setBalance(float balance) {
  balance_ = std::clamp(balance, kMinBalance, kMaxBalance);
  ApplyProperties();
}

}  // namespace gmp::gothic