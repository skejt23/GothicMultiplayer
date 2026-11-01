#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>

namespace gmp {

// Capture native health deductions without changing the victim's attributes.
// Inflating HP to measure a hit is invalid: ZenGin clamps ChangeAttribute to
// maximum HP, and spell scripts can depend on the victim's actual health.
class NativeDamageObservation {
public:
  explicit NativeDamageObservation(const void* victim) : victim_(victim), previous_(current_) {
    current_ = this;
  }
  ~NativeDamageObservation() {
    current_ = previous_;
  }

  NativeDamageObservation(const NativeDamageObservation&) = delete;
  NativeDamageObservation& operator=(const NativeDamageObservation&) = delete;

  std::int32_t damage() const {
    return damage_;
  }

  static bool IsActive(const void* victim) {
    return Find(victim) != nullptr;
  }

  // Return true when the caller must suppress the native HP mutation. Healing
  // during a damage calculation is not another damage contribution.
  static bool RecordHealthChange(const void* victim, std::int32_t change) {
    auto* observation = Find(victim);
    if (!observation)
      return false;
    if (change < 0) {
      observation->damage_ = static_cast<std::int32_t>(
          std::min<std::int64_t>(static_cast<std::int64_t>(observation->damage_) - change, std::numeric_limits<std::int32_t>::max()));
    }
    return true;
  }

private:
  static NativeDamageObservation* Find(const void* victim) {
    if (!victim)
      return nullptr;
    for (auto* observation = current_; observation; observation = observation->previous_) {
      if (observation->victim_ == victim)
        return observation;
    }
    return nullptr;
  }

  inline static thread_local NativeDamageObservation* current_{nullptr};
  const void* victim_;
  NativeDamageObservation* previous_;
  std::int32_t damage_{0};
};

}  // namespace gmp
