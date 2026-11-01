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

#include "lua_sound3d.h"

#include <spdlog/spdlog.h>

#include <algorithm>

#include "lua_helpers.h"
#include "net_game.h"

using namespace Gothic_II_Addon;

namespace gmp::gothic {

std::unordered_set<LuaSound3d*> LuaSound3d::active_sounds_;

/* luagmp (class)
 *
 * This class represents a positional 3D sound attached to an engine Vob.
 *
 * @version  0.3.0
 * @name     Sound3d
 * @side     client
 * @category Game
 *
 */

/* luagmp (constructor)
 *
 * Creates a new 3D sound from a file.
 *
 * @param    (string) file    Sound file path.
 *
 */
LuaSound3d::LuaSound3d(const std::string& filename)
    : file_(filename),
      volume_(1.0f),
      looping_(false),
      balance_(0.0f),
      handle_(-1),
      target_kind_(TargetKind::None),
      target_vob_(nullptr),
      sound_fx_(nullptr) {
  active_sounds_.insert(this);
  params_.SetDefaults();
  ApplyParams();
  ReloadSound();
}

LuaSound3d::~LuaSound3d() {
  active_sounds_.erase(this);
  StopIfNeeded();

  if (sound_fx_) {
    sound_fx_->Release();
    sound_fx_ = nullptr;
  }
}

void LuaSound3d::ReloadSound() {
  StopIfNeeded();

  if (sound_fx_) {
    sound_fx_->Release();
    sound_fx_ = nullptr;
  }

  if (!zsound) {
    SPDLOG_ERROR("Sound system is unavailable; cannot load 3D sound '{}'.", file_);
    return;
  }

  zSTRING sound_name(file_.c_str());
  sound_fx_ = zsound->LoadSoundFX(sound_name);
  if (!sound_fx_) {
    SPDLOG_ERROR("Failed to load 3D sound FX from '{}'.", file_);
    return;
  }

  ApplyParams();
}

void LuaSound3d::StopIfNeeded() {
  if (handle_ >= 0 && zsound) {
    zsound->StopSound(handle_);
  }
  handle_ = -1;
}

void LuaSound3d::ApplyParams() {
  params_.volume = volume_;
  params_.loopType = looping_ ? zCSoundSystem::zSND_LOOPING_ENABLED : zCSoundSystem::zSND_LOOPING_DISABLED;
  if (sound_fx_) {
    sound_fx_->SetVolume(volume_);
    sound_fx_->SetLooping(looping_ ? 1 : 0);
    sound_fx_->SetPan(balance_);
  }
  update();
}

void LuaSound3d::ReplayIfNeeded(bool was_playing) {
  if (was_playing) {
    play();
  }
}

void LuaSound3d::ClearTarget() {
  target_kind_ = TargetKind::None;
  target_vob_ref_.reset();
  target_player_id_.reset();
  target_vob_ = nullptr;
}

zCVob* LuaSound3d::ResolveTargetVob() const {
  if (target_kind_ == TargetKind::Vob) {
    return target_vob_ref_ ? target_vob_ref_->handle() : nullptr;
  }

  if (target_kind_ == TargetKind::Player && target_player_id_.has_value()) {
    for (auto* player : NetGame::Instance().players) {
      if (player && player->base_player().id() == *target_player_id_) {
        return player->GetNpc();
      }
    }
  }

  return nullptr;
}

/* luagmp (method)
 *
 * Starts 3D sound playback at the current target Vob.
 *
 * @name     play
 *
 */
void LuaSound3d::play() {
  if (!sound_fx_) {
    ReloadSound();
  }

  target_vob_ = ResolveTargetVob();
  if (!zsound || !sound_fx_ || !target_vob_) {
    SPDLOG_WARN("Cannot play 3D sound '{}': zsound={}, sound_fx_={}, target_vob={}.", file_, (void*)zsound, (void*)sound_fx_, (void*)target_vob_);
    StopIfNeeded();
    return;
  }

  StopIfNeeded();
  ApplyParams();
  handle_ = zsound->PlaySound3D(sound_fx_, target_vob_, 0, &params_);
}

/* luagmp (method)
 *
 * Stops 3D sound playback.
 *
 * @name     stop
 *
 */
void LuaSound3d::stop() {
  StopIfNeeded();
}

/* luagmp (method)
 *
 * Returns whether the 3D sound is currently playing.
 *
 * @name     isPlaying
 * @return   (boolean) True if the sound is playing.
 *
 */
bool LuaSound3d::isPlaying() const {
  return handle_ >= 0 && zsound && zsound->IsSoundActive(handle_) != 0;
}

/* luagmp (method)
 *
 * Updates the active 3D sound parameters in the engine.
 *
 * @name     update
 *
 */
void LuaSound3d::update() {
  target_vob_ = ResolveTargetVob();
  if (!target_vob_) {
    StopIfNeeded();
    return;
  }

  if (handle_ >= 0 && zsound) {
    zsound->UpdateSound3D(handle_, &params_);
  }
}

void LuaSound3d::UpdateActiveSounds() {
  for (auto* sound : active_sounds_) {
    if (sound) {
      sound->update();
    }
  }
}

/* luagmp (method)
 *
 * Sets the Vob used as the 3D sound source.
 *
 * @name     setTargetVob
 * @param    (Vob) vob    Target Vob.
 *
 */
void LuaSound3d::setTargetVob(const LuaVob& vob) {
  const bool was_playing = isPlaying();
  target_kind_ = TargetKind::Vob;
  target_vob_ref_ = vob;
  target_player_id_.reset();
  target_vob_ = ResolveTargetVob();
  if (!target_vob_) {
    StopIfNeeded();
    return;
  }

  ReplayIfNeeded(was_playing);
}

/* luagmp (method)
 *
 * Sets a streamed player or server NPC as the 3D sound source.
 *
 * @name     setTargetPlayer
 * @param    (number) playerId    Streamed player or server NPC id; client-local NPC ids are not supported.
 * @return   (boolean)            True if the target exists in the game world.
 *
 */
bool LuaSound3d::setTargetPlayer(std::uint64_t player_id) {
  Gothic2APlayer* target_player = nullptr;
  for (auto* player : NetGame::Instance().players) {
    if (player && player->base_player().id() == player_id) {
      target_player = player;
      break;
    }
  }

  if (!target_player || !target_player->GetNpc()) {
    return false;
  }

  const bool was_playing = isPlaying();
  target_kind_ = TargetKind::Player;
  target_vob_ref_.reset();
  target_player_id_ = player_id;
  target_vob_ = target_player->GetNpc();
  ReplayIfNeeded(was_playing);
  return true;
}

/* luagmp (property)
 *
 * Represents the sound file path.
 *
 * @name     file
 * @return   (string)
 *
 */
std::string LuaSound3d::getFile() const {
  return file_;
}

/// Assigns the file property using the same value format as its getter.
void LuaSound3d::setFile(const std::string& filename) {
  if (filename == file_) {
    return;
  }

  const bool was_playing = isPlaying();
  file_ = filename;
  ReloadSound();
  ReplayIfNeeded(was_playing);
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
float LuaSound3d::getDuration() const {
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
float LuaSound3d::getVolume() const {
  return volume_;
}

/// Assigns the volume property using the same value format as its getter.
void LuaSound3d::setVolume(float volume) {
  volume_ = std::clamp(volume, lua_helpers::kSoundMinVolume, lua_helpers::kSoundMaxVolume);
  ApplyParams();
}

/* luagmp (property)
 *
 * Represents whether the sound should loop.
 *
 * @name     looping
 * @return   (boolean)
 *
 */
bool LuaSound3d::getLooping() const {
  return looping_;
}

/// Assigns the looping property using the same value format as its getter.
void LuaSound3d::setLooping(bool looping) {
  looping_ = looping;
  ApplyParams();
}

/* luagmp (property)
 * Stereo balance from -1 (left) through 0 (center) to 1 (right).
 *
 * @name     balance
 * @return   (number)
 *
 */
float LuaSound3d::getBalance() const {
  return balance_;
}

/// Assigns the balance property using the same value format as its getter.
void LuaSound3d::setBalance(float balance) {
  balance_ = std::clamp(balance, lua_helpers::kSoundMinBalance, lua_helpers::kSoundMaxBalance);
  ApplyParams();
}

/* luagmp (property)
 *
 * Represents 3D sound obstruction.
 *
 * @name     obstruction
 * @return   (number)
 *
 */
float LuaSound3d::getObstruction() const {
  return params_.obstruction;
}

/// Assigns the obstruction property using the same value format as its getter.
void LuaSound3d::setObstruction(float obstruction) {
  params_.obstruction = obstruction;
  update();
}

/* luagmp (property)
 * 3D sound radius in world units.
 *
 * @name     radius
 * @return   (number)
 *
 */
float LuaSound3d::getRadius() const {
  return params_.radius;
}

/// Assigns the radius property using the same value format as its getter.
void LuaSound3d::setRadius(float radius) {
  params_.radius = std::max(0.0f, radius);
  update();
}

/* luagmp (property)
 *
 * Represents 3D sound cone angle in degrees.
 *
 * @name     coneAngle
 * @return   (number)
 *
 */
float LuaSound3d::getConeAngle() const {
  return params_.coneAngleDeg;
}

/// Assigns the coneAngle property using the same value format as its getter.
void LuaSound3d::setConeAngle(float angle) {
  params_.coneAngleDeg = angle;
  update();
}

/* luagmp (property)
 *
 * Represents 3D sound reverb level.
 *
 * @name     reverbLevel
 * @return   (number)
 *
 */
float LuaSound3d::getReverbLevel() const {
  return params_.reverbLevel;
}

/// Assigns the reverbLevel property using the same value format as its getter.
void LuaSound3d::setReverbLevel(float level) {
  params_.reverbLevel = level;
  update();
}

/* luagmp (property)
 *
 * Represents whether the sound uses ambient 3D behavior.
 *
 * @name     ambient
 * @return   (boolean)
 *
 */
bool LuaSound3d::getAmbient() const {
  return params_.isAmbient3D != 0;
}

/// Assigns the ambient property using the same value format as its getter.
void LuaSound3d::setAmbient(bool ambient) {
  params_.isAmbient3D = ambient ? 1 : 0;
  update();
}

/* luagmp (property)
 *
 * Represents 3D sound pitch offset.
 *
 * @name     pitchOffset
 * @return   (number)
 *
 */
float LuaSound3d::getPitchOffset() const {
  return params_.pitchOffset;
}

/// Assigns the pitchOffset property using the same value format as its getter.
void LuaSound3d::setPitchOffset(float pitch_offset) {
  params_.pitchOffset = pitch_offset;
  update();
}

/* luagmp (property)
 * Attached Vob. Set to nil to detach the sound from its target.
 * @name targetVob
 * @return (Vob|nil)
 */
sol::object LuaSound3d::getTargetVob(sol::this_state ts) const {
  sol::state_view lua(ts);
  zCVob* target_vob = ResolveTargetVob();
  if (!target_vob) {
    return sol::make_object(lua, sol::lua_nil);
  }

  return sol::make_object(lua, LuaVob::FromExisting(target_vob));
}

/// Assigns the targetVob property using the same value format as its getter.
void LuaSound3d::setTargetVobValue(sol::object value) {
  if (value.get_type() == sol::type::nil) {
    ClearTarget();
    StopIfNeeded();
    return;
  }

  if (value.is<LuaVob>()) {
    setTargetVob(value.as<LuaVob>());
  }
}

void BindSound3d(sol::state& lua) {
  sol::usertype<LuaSound3d> sound_type = lua.new_usertype<LuaSound3d>("Sound3d", sol::constructors<LuaSound3d(const std::string&)>());

  sound_type["play"] = &LuaSound3d::play;
  sound_type["stop"] = &LuaSound3d::stop;
  sound_type["isPlaying"] = &LuaSound3d::isPlaying;
  sound_type["update"] = &LuaSound3d::update;
  sound_type["setTargetVob"] = &LuaSound3d::setTargetVob;
  sound_type["setTargetPlayer"] = &LuaSound3d::setTargetPlayer;

  sound_type["file"] = sol::property(&LuaSound3d::getFile, &LuaSound3d::setFile);
  sound_type["duration"] = sol::property(&LuaSound3d::getDuration);
  sound_type["volume"] = sol::property(&LuaSound3d::getVolume, &LuaSound3d::setVolume);
  sound_type["looping"] = sol::property(&LuaSound3d::getLooping, &LuaSound3d::setLooping);
  sound_type["balance"] = sol::property(&LuaSound3d::getBalance, &LuaSound3d::setBalance);
  sound_type["obstruction"] = sol::property(&LuaSound3d::getObstruction, &LuaSound3d::setObstruction);
  sound_type["radius"] = sol::property(&LuaSound3d::getRadius, &LuaSound3d::setRadius);
  sound_type["coneAngle"] = sol::property(&LuaSound3d::getConeAngle, &LuaSound3d::setConeAngle);
  sound_type["reverbLevel"] = sol::property(&LuaSound3d::getReverbLevel, &LuaSound3d::setReverbLevel);
  sound_type["ambient"] = sol::property(&LuaSound3d::getAmbient, &LuaSound3d::setAmbient);
  sound_type["pitchOffset"] = sol::property(&LuaSound3d::getPitchOffset, &LuaSound3d::setPitchOffset);
  sound_type["targetVob"] = sol::property(&LuaSound3d::getTargetVob, &LuaSound3d::setTargetVobValue);
}

}  // namespace gmp::gothic
