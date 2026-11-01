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
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>

// A command only changes the named components. The real hero must never replay
// an unrelated proxy snapshot as a side effect of a server correction.
enum ActorSyncComponent : std::uint16_t {
  kSyncPosition = 1u << 0,
  kSyncOrientation = 1u << 1,
  kSyncHealth = 1u << 2,
  kSyncMana = 1u << 3,
  kSyncEquipment = 1u << 4,
  kSyncCombat = 1u << 5,
  kSyncPose = 1u << 6,
  kSyncLife = 1u << 7,
  kSyncAll = 255,
};

constexpr std::uint8_t kMaxSpellInvestLevel = 20;
constexpr std::size_t kMaxInventoryEntries = 4096;
// Leave room for counts, string lengths and the rest of an actor snapshot in
// GameNetworkingSockets' maximum reliable message.
constexpr std::size_t kMaxInventoryNameBytes = 384 * 1024;

inline std::uint64_t ActorClockMilliseconds() {
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count());
}

inline bool IsNewActorSequence(std::uint32_t incoming, std::uint32_t current) {
  const auto delta = incoming - current;
  return delta != 0 && delta < 0x80000000u;
}

inline bool IsFiniteActorVector(const glm::vec3& vector) {
  return std::isfinite(vector.x) && std::isfinite(vector.y) && std::isfinite(vector.z);
}

// Component freshness is separate from lifecycle event delivery. A position
// update cannot consume a health update or a death notification.
class ActorComponentSequences {
public:
  void Reset(std::uint32_t sequence = 0) {
    sequences_.fill(sequence);
    initialized_.fill(sequence != 0);
  }

  std::uint16_t Accept(std::uint16_t components, std::uint32_t sequence, bool allow_equal = false) {
    std::uint16_t accepted = 0;
    for (std::size_t index = 0; index < sequences_.size(); ++index) {
      const auto bit = static_cast<std::uint16_t>(1u << index);
      if ((components & bit) && (!initialized_[index] || IsNewActorSequence(sequence, sequences_[index]) ||
                                (allow_equal && sequence == sequences_[index]))) {
        sequences_[index] = sequence;
        initialized_[index] = true;
        accepted |= bit;
      }
    }
    return accepted;
  }

  std::uint16_t CurrentComponents(std::uint16_t components, const ActorComponentSequences& captured) const {
    std::uint16_t current = 0;
    for (std::size_t index = 0; index < sequences_.size(); ++index) {
      const auto bit = static_cast<std::uint16_t>(1u << index);
      if ((components & bit) && initialized_[index] && captured.initialized_[index] &&
          sequences_[index] == captured.sequences_[index]) current |= bit;
    }
    return current;
  }

private:
  std::array<std::uint32_t, 8> sequences_{};
  std::array<bool, 8> initialized_{};
};
