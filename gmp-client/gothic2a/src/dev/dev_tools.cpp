/*
MIT License

Copyright (c) 2025 skejt23

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

#include "dev/dev_tools.h"

#include <algorithm>
#include <ctime>
#include <string>
#include <vector>

#include "CChat.h"
#include "keyboard.h"

namespace debug {
namespace {
constexpr float kMinSpeed = 100.0f;
constexpr float kMaxSpeed = 7000.0f;
constexpr float kSpeedStep = 100.0f;

bool HasModifierForToggle() {
  return zinput->KeyPressed(KEY_LCONTROL) || zinput->KeyPressed(KEY_RCONTROL) || zinput->KeyPressed(KEY_LALT) || zinput->KeyPressed(KEY_RALT);
}

void EnablePhysics() {
  player->SetCollDet(1);
  player->SetPhysicsEnabled(1);
  player->GetAnictrl()->SetPhysicsEnabled(1);
}

void DisablePhysics() {
  player->SetCollDet(0);
  player->SetPhysicsEnabled(0);
  player->GetAnictrl()->SetPhysicsEnabled(0);
}

void ZeroVelocity() {
  auto* anictrl = player->GetAnictrl();
  anictrl->velocity[VX] = 0.0f;
  anictrl->velocity[VY] = 0.0f;
  anictrl->velocity[VZ] = 0.0f;
  anictrl->state = zCAIPlayer::zMV_STATE_FLY;
}

}  // namespace

DevTools::DevTools()
    : noclip_allowed_(false),
      noclip_enabled_(false),
      noclip_speed_(1000.0f),
      last_noclip_update_(0) {
}

DevTools& DevTools::Instance() {
  static DevTools instance;
  return instance;
}

void DevTools::HandleInput(bool writingOnChat) {
  HandleNoclip(writingOnChat);
}

void DevTools::Render() {
#ifndef NDEBUG
  // Show player position in debug builds
  if (player) {
    screen->SetFont("FONT_OLD_10_WHITE.TGA");
    screen->SetFontColor(zCOLOR(200, 200, 200));

    zVEC3 pos = player->GetPositionWorld();
    std::string posText = "Pos: (" + std::to_string(static_cast<int>(pos[VX])) + ", " + std::to_string(static_cast<int>(pos[VY])) + ", " +
                          std::to_string(static_cast<int>(pos[VZ])) + ")";

    constexpr int marginX = 100;
    constexpr int marginY = 100;
    const int textW = screen->FontSize(posText.c_str());
    screen->Print(8192 - marginX - textW, marginY, posText.c_str());
  }
#endif

  RenderNoclipOverlay();
}

void DevTools::SetNoclipAllowed(bool allowed) {
  noclip_allowed_ = allowed;
  if (!noclip_allowed_) {
    DisableNoclip();
  }
}

void DevTools::HandleNoclip(bool writingOnChat) {
  if (!noclip_allowed_) {
    if (noclip_enabled_) {
      DisableNoclip();
    }
    return;
  }

  if (zinput->KeyToggled(KEY_F8) && !writingOnChat) {
    if (!HasModifierForToggle()) {
      if (noclip_enabled_) {
        DisableNoclip();
      } else {
        noclip_enabled_ = true;
        last_noclip_update_ = clock();
      }
    }
  }

  if (!noclip_enabled_ || writingOnChat || !player) {
    return;
  }

  DisablePhysics();
  ZeroVelocity();

  const clock_t now = clock();
  const float deltaTime = (now - last_noclip_update_) / 1000.0f;
  if (deltaTime <= 0.0f) {
    return;
  }

  last_noclip_update_ = now;

  // Speed adjustment
  if (zinput->KeyPressed(KEY_ADD)) {
    noclip_speed_ = std::min(noclip_speed_ + kSpeedStep, kMaxSpeed);
  }
  if (zinput->KeyPressed(KEY_SUBTRACT)) {
    noclip_speed_ = std::max(noclip_speed_ - kSpeedStep, kMinSpeed);
  }

  // Apply movement
  zVEC3 movement(0.0f, 0.0f, 0.0f);
  const float moveDistance = noclip_speed_ * deltaTime;

  zVEC3 forward = player->GetAtVectorWorld();
  zVEC3 right = player->trafoObjToWorld.GetRightVector();
  forward.Normalize();
  right.Normalize();

  if (zinput->KeyPressed(KEY_W) || zinput->KeyPressed(KEY_UP)) {
    movement[VX] += forward[VX] * moveDistance;
    movement[VZ] += forward[VZ] * moveDistance;
  }
  if (zinput->KeyPressed(KEY_S) || zinput->KeyPressed(KEY_DOWN)) {
    movement[VX] -= forward[VX] * moveDistance;
    movement[VZ] -= forward[VZ] * moveDistance;
  }
  if (zinput->KeyPressed(KEY_A) || zinput->KeyPressed(KEY_LEFT)) {
    movement[VX] -= right[VX] * moveDistance;
    movement[VZ] -= right[VZ] * moveDistance;
  }
  if (zinput->KeyPressed(KEY_D) || zinput->KeyPressed(KEY_RIGHT)) {
    movement[VX] += right[VX] * moveDistance;
    movement[VZ] += right[VZ] * moveDistance;
  }
  if (zinput->KeyPressed(KEY_SPACE)) {
    movement[VY] += moveDistance;
  }
  if (zinput->KeyPressed(KEY_LCONTROL) || zinput->KeyPressed(KEY_RCONTROL)) {
    movement[VY] -= moveDistance;
  }

  zVEC3 newPos = player->GetPositionWorld();
  newPos[VX] += movement[VX];
  newPos[VY] += movement[VY];
  newPos[VZ] += movement[VZ];
  player->SetPositionWorld(newPos);
}

void DevTools::DisableNoclip() {
  if (!noclip_enabled_) {
    return;
  }

  noclip_enabled_ = false;
  if (player) {
    EnablePhysics();
  }
}

void DevTools::RenderNoclipOverlay() {
  if (!noclip_enabled_) {
    return;
  }

  screen->SetFont("FONT_OLD_10_WHITE.TGA");
  screen->SetFontColor(zCOLOR(0, 255, 0));

  constexpr int marginX = 100;
  constexpr int marginY = 100;

  std::vector<std::string> lines;
  lines.push_back("=== NOCLIP MODE ACTIVE ===");
  lines.push_back("Speed: " + std::to_string(static_cast<int>(noclip_speed_)) + " units/s");
  lines.push_back("WASD/Arrows: Move horizontally");
  lines.push_back("Space: Move up");
  lines.push_back("Ctrl: Move down");
  lines.push_back("+/-: Adjust speed");
  lines.push_back("F8: Toggle noclip off");

  const int fontH = screen->FontY();
  const int spacing = fontH;
  const int totalHeight = spacing * static_cast<int>(lines.size());
  const int yStart = 8192 - marginY - totalHeight;

  for (size_t i = 0; i < lines.size(); ++i) {
    const std::string& line = lines[i];
    const int textW = screen->FontSize(line.c_str());
    const int x = 8192 - marginX - textW;
    const int y = yStart + static_cast<int>(i) * spacing;

    if (i == 0 || i == 1) {
      screen->SetFontColor(zCOLOR(0, 255, 0));
      screen->Print(x, y, line.c_str());
      screen->SetFontColor(zCOLOR(200, 200, 200));
    } else {
      screen->Print(x, y, line.c_str());
    }
  }
}

}  // namespace debug
