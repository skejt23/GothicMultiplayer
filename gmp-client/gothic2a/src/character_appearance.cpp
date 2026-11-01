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

#include "character_appearance.h"

namespace gmp::gothic {

void ApplyCharacterAppearance(oCNpc& npc, const CharacterAppearance& appearance) {
  // Remove using the actual engine name, including any suffix it retained.
  for (int i = npc.activeOverlays.GetNumInList() - 1; i >= 0; --i) {
    const zSTRING name = npc.activeOverlays[i];
    const auto walking = NormalizeCharacterWalkingOverlay(name.ToChar());
    if (walking && !walking->empty()) {
      npc.RemoveOverlay(name);
    }
  }
  zSTRING body(appearance.body_model.c_str());
  zSTRING head(appearance.head_model.c_str());
  npc.SetAdditionalVisuals(body, appearance.body_texture, 0, head, appearance.head_texture, 0, -1);
  if (!appearance.walking_overlay.empty()) {
    zSTRING overlay(appearance.walking_overlay.c_str());
    npc.ApplyOverlay(overlay);
  }
  if (npc.human_ai) {
    npc.human_ai->InitAnimations();
  }
}

std::string GetCharacterWalkingOverlay(const oCNpc& npc) {
  for (int i = npc.activeOverlays.GetNumInList() - 1; i >= 0; --i) {
    const auto overlay = NormalizeCharacterWalkingOverlay(npc.activeOverlays[i].ToChar());
    if (overlay && !overlay->empty()) {
      return std::string(*overlay);
    }
  }
  return {};
}

}  // namespace gmp::gothic
