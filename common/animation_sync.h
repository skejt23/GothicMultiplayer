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

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace gmp {

// Receiving a newer sample does not invalidate an older sample waiting for its
// render time. Immediate server commands invalidate only the affected fields.
class ActorPresentationEpochs {
 public:
  void Reset() { epochs_.fill(0); }

  void Invalidate(std::uint16_t components) {
    for (std::size_t index = 0; index < epochs_.size(); ++index) {
      if (components & (1u << index)) ++epochs_[index];
    }
  }

  std::uint16_t CurrentComponents(std::uint16_t components, const ActorPresentationEpochs& captured) const {
    std::uint16_t current = 0;
    for (std::size_t index = 0; index < epochs_.size(); ++index) {
      const auto bit = static_cast<std::uint16_t>(1u << index);
      if ((components & bit) && epochs_[index] == captured.epochs_[index]) current |= bit;
    }
    return current;
  }

 private:
  std::array<std::uint32_t, 8> epochs_{};
};

inline bool IsPlayerTurningAnimation(std::string_view name) {
  return name.ends_with("TURNL") || name.ends_with("TURNR");
}

inline bool IsPlayerStrafeAnimation(std::string_view name) {
  return name.starts_with("T_") &&
         (name.ends_with("STRAFEL") || name.ends_with("STRAFER"));
}

inline bool IsPlayerLifecycleAnimation(std::string_view name) {
  return name == "S_DEAD" || name == "S_DEADB" || name == "T_DEAD" || name == "T_DEADB" ||
         name == "S_WOUNDED" || name == "S_WOUNDEDB" ||
         name == "T_STAND_2_WOUNDED" || name == "T_STAND_2_WOUNDEDB" ||
         name == "T_WOUNDED_2_DEAD" || name == "T_WOUNDEDB_2_DEADB" ||
         name == "T_WOUNDED_TRY" || name == "T_WOUNDEDB_TRY" ||
         name == "T_WOUNDED_2_STAND" || name == "T_WOUNDEDB_2_STAND";
}

inline bool IsDamageReactionAnimation(std::string_view name) {
  return name == "T_GOTHIT" || (name.starts_with("T_") && (name.ends_with("STUMBLE") || name.ends_with("STUMBLEB")));
}

inline bool IsPlayerGetUpAnimation(std::string_view name) {
  return name == "T_WOUNDED_2_STAND" || name == "T_WOUNDEDB_2_STAND" ||
         name == "T_FALLEN_2_STAND" || name == "T_FALLENB_2_STAND" || name == "T_FALLDN_2_STAND";
}

// Gothic's strafe cycles use T_* names. Finite attacks and stand-up transitions
// still belong to the reliable action stream, not repeatedly sampled poses.
inline bool IsSampledPlayerAnimation(std::string_view name, bool looping) {
  return !name.empty() && !IsPlayerTurningAnimation(name) && !IsPlayerLifecycleAnimation(name) &&
         (looping || name.starts_with("S_") || IsPlayerStrafeAnimation(name));
}

inline bool IsPlayerLocomotionAnimation(std::string_view name) {
  return IsPlayerStrafeAnimation(name) || name.find("RUN") != std::string_view::npos ||
         name.find("WALK") != std::string_view::npos || name.find("SNEAK") != std::string_view::npos ||
         name.find("SWIM") != std::string_view::npos || name.find("DIVE") != std::string_view::npos;
}

struct PlayerAnimationSample {
  int id = -1;
  std::string_view name;
  int layer = 0;
  bool looping = false;
  bool fading_out = false;
  bool skeletal = true;
  bool casting = false;
};

inline int SelectPlayerPoseAnimation(std::span<const PlayerAnimationSample> animations) {
  int selected_id = -1;
  int selected_layer = -1;
  bool selected_locomotion = false;
  bool selected_casting = false;
  for (const auto& animation : animations) {
    if (animation.id < 0 || animation.fading_out || !animation.skeletal ||
        !IsSampledPlayerAnimation(animation.name, animation.looping)) {
      continue;
    }
    const bool locomotion = IsPlayerLocomotionAnimation(animation.name);
    if (selected_id < 0 || (animation.casting && !selected_casting) ||
        (animation.casting == selected_casting &&
         ((locomotion && !selected_locomotion) || (locomotion == selected_locomotion && animation.layer >= selected_layer)))) {
      selected_id = animation.id;
      selected_layer = animation.layer;
      selected_locomotion = locomotion;
      selected_casting = animation.casting;
    }
  }
  return selected_id;
}

}  // namespace gmp
