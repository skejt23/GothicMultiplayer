#pragma once

#include <cstdint>
#include <optional>

namespace gmp::gothic {

constexpr std::int32_t kHudAll = 0;
constexpr std::int32_t kHudHealthBar = 1;
constexpr std::int32_t kHudManaBar = 2;
constexpr std::int32_t kHudSwimBar = 3;
constexpr std::int32_t kHudFocusBar = 4;
constexpr std::int32_t kHudFocusName = 5;

constexpr std::int32_t kHudModeHidden = 0;
constexpr std::int32_t kHudModeDefault = 1;
constexpr std::int32_t kHudModeAlwaysVisible = 2;

struct HudVector {
  std::int32_t x;
  std::int32_t y;
};

bool SetHudEnabled(std::int32_t hud_type, bool enabled);
std::optional<bool> GetHudEnabled(std::int32_t hud_type);
bool IsHudTypeEnabled(std::int32_t hud_type);
bool SetHudMode(std::int32_t hud_type, std::int32_t mode);
std::optional<std::int32_t> GetHudMode(std::int32_t hud_type);
bool IsHudVisible(std::int32_t hud_type, bool default_visible);
bool SetHudBarPosition(std::int32_t hud_type, std::int32_t x, std::int32_t y);
std::optional<HudVector> GetHudBarPosition(std::int32_t hud_type);
bool SetHudBarSize(std::int32_t hud_type, std::int32_t width, std::int32_t height);
std::optional<HudVector> GetHudBarSize(std::int32_t hud_type);

}  // namespace gmp::gothic
