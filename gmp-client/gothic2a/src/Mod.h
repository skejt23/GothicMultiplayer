
/*
MIT License

Copyright (c) 2022 Gothic Multiplayer Team (pampi, skejt23, mecio)

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

#include "common.h"

namespace Gothic_II_Addon {
class oCNpc;
}

void Initialize(void);
void SetSuppressLocalEquipEvents(bool suppress);
bool ShouldSuppressLocalEquipEvents();

// Prevent server presentation from being recaptured as another local action.
class ScopedNetworkObservationSuppression {
public:
  ScopedNetworkObservationSuppression() : previous_(ShouldSuppressLocalEquipEvents()) { SetSuppressLocalEquipEvents(true); }
  ~ScopedNetworkObservationSuppression() { SetSuppressLocalEquipEvents(previous_); }
  ScopedNetworkObservationSuppression(const ScopedNetworkObservationSuppression&) = delete;
  ScopedNetworkObservationSuppression& operator=(const ScopedNetworkObservationSuppression&) = delete;
private:
  bool previous_;
};
void SetSuppressLocalLifecycleEvents(bool suppress);
bool ShouldSuppressLocalLifecycleEvents();
void SetDamageAnimationsEnabled(bool enabled);
bool AreDamageAnimationsEnabled();
void PlayNetworkDamageAnimation(Gothic_II_Addon::oCNpc* victim, Gothic_II_Addon::oCNpc* attacker, int damage, unsigned long damage_type);
void SetMunitionTrailEnabled(bool enabled);
bool IsMunitionTrailEnabled();
void ResetLocalActionObservation();
void RefreshLocalActionObservation();
