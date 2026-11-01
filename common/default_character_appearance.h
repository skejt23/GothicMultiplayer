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

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace gmp {

// Stock Gothic II choices shared by the menu, config and join validation.
inline constexpr std::array<std::string_view, 2> kCharacterBodies{"HUM_BODY_NAKED0", "HUM_BODY_BABE0"};
inline constexpr std::array<std::string_view, 6> kCharacterHeads{
    "HUM_HEAD_FIGHTER", "HUM_HEAD_BALD", "HUM_HEAD_FATBALD", "HUM_HEAD_PONY", "HUM_HEAD_PSIONIC", "HUM_HEAD_THIEF"};
inline constexpr std::array<std::string_view, 7> kCharacterWalkingOverlays{
    "", "HUMANS_MILITIA.MDS", "HUMANS_MAGE.MDS", "HUMANS_RELAXED.MDS", "HUMANS_ARROGANCE.MDS", "HUMANS_TIRED.MDS", "HUMANS_BABE.MDS"};
inline constexpr int kMaxCharacterBodyTexture = 12;
inline constexpr int kMaxCharacterHeadTexture = 162;

struct CharacterAppearance {
  std::string body_model{"HUM_BODY_NAKED0"};
  int body_texture{9};
  std::string head_model{"HUM_HEAD_PONY"};
  int head_texture{18};
  std::string walking_overlay;
};

inline bool IsCharacterWalkingOverlay(std::string_view overlay) {
  return std::find(kCharacterWalkingOverlays.begin(), kCharacterWalkingOverlays.end(), overlay) != kCharacterWalkingOverlays.end();
}

// Native Gothic can report overlay names without the .MDS suffix.
inline std::optional<std::string_view> NormalizeCharacterWalkingOverlay(std::string overlay) {
  for (auto& ch : overlay) {
    if (ch >= 'a' && ch <= 'z') {
      ch -= 'a' - 'A';
    }
  }
  if (overlay.ends_with(".MDS")) {
    overlay.resize(overlay.size() - 4);
  }
  for (const auto name : kCharacterWalkingOverlays) {
    if (name.substr(0, name.empty() ? 0 : name.size() - 4) == overlay) {
      return name;
    }
  }
  return std::nullopt;
}

inline void NormalizeCharacterAppearance(CharacterAppearance& appearance) {
  const CharacterAppearance defaults;
  if (std::find(kCharacterBodies.begin(), kCharacterBodies.end(), appearance.body_model) == kCharacterBodies.end())
    appearance.body_model = defaults.body_model;
  if (std::find(kCharacterHeads.begin(), kCharacterHeads.end(), appearance.head_model) == kCharacterHeads.end())
    appearance.head_model = defaults.head_model;
  appearance.body_texture = std::clamp(appearance.body_texture, 0, kMaxCharacterBodyTexture);
  appearance.head_texture = std::clamp(appearance.head_texture, 0, kMaxCharacterHeadTexture);
  appearance.walking_overlay = NormalizeCharacterWalkingOverlay(appearance.walking_overlay).value_or("");
}

}  // namespace gmp
