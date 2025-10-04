
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

#include "CAnimMenu.h"

#include <stdio.h>

#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <array>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "CChat.h"
#include "language.h"
#include "patch.h"

using namespace std;
extern zCOLOR Highlighted;
extern zCOLOR Normal;
zCOLOR FColors;

CAnimMenu::CAnimMenu() {
  MenuPos = 0, PrintFrom = 0, PrintTo = 0;
  Opened = false;
  std::ifstream anim_file(".\\Multiplayer\\AnimMenu.json");

  if (!anim_file.good()) {
    CChat::GetInstance()->WriteMessage(NORMAL, false, "Couldn't find AnimMenu.json in Multiplayer folder.");
    return;
  }

  nlohmann::json json_data;
  try {
    anim_file >> json_data;
  } catch (const std::exception& ex) {
    SPDLOG_ERROR("Failed to parse AnimMenu.json: {}", ex.what());
    CChat::GetInstance()->WriteMessage(NORMAL, false, "AnimMenu.json is invalid.");
    return;
  }

  if (!json_data.contains("animations") || !json_data["animations"].is_array()) {
    SPDLOG_WARN("AnimMenu.json does not contain an 'animations' array.");
    return;
  }

  const auto should_skip =
      [](const std::string& value) {
        if (value.empty()) {
          return false;
    }
    static const std::array<const char*, 5> kBlockedSubstrings = {"DIVE", "TOUCHPLATE", "VWHEEL", "RELOAD", "LADDER"};
    for (const auto* substring : kBlockedSubstrings) {
      if (value.find(substring) != std::string::npos) {
        return true;
      }
    }
    return false;
  };

  for (const auto& entry : json_data["animations"]) {
    if (!entry.is_object()) {
      continue;
    }

    const auto name = entry.value("name", std::string{});
    const auto loop = entry.value("loop", std::string{});
    const auto start = entry.value("start", std::string{});
    const auto end = entry.value("end", std::string{});

    if (name.empty() || loop.empty()) {
      continue;
    }

    if (should_skip(loop) || should_skip(start) || should_skip(end)) {
      continue;
    }

    Anim anim;
    anim.AniName = name.c_str();
    anim.AniLoop = loop.c_str();
    if (!start.empty()) {
      anim.AniStart = start.c_str();
    }
    if (!end.empty()) {
      anim.AniEnd = end.c_str();
    }
    AnimVector.push_back(std::move(anim));
  }
}

void CAnimMenu::Open() {
  if (!AnimVector[MenuPos].AniEnd.IsEmpty()) {
    if (player->GetModel()->IsAnimationActive(AnimVector[MenuPos].AniLoop)) {
      player->GetModel()->StartAnimation(AnimVector[MenuPos].AniEnd);
      return;
    }
  } else if (player->GetModel()->IsAnimationActive(AnimVector[MenuPos].AniLoop)) {
    player->GetModel()->StopAnimation(AnimVector[MenuPos].AniLoop);
    return;
  }
  player->GetAnictrl()->StopTurnAnis();
  Patch::PlayerInterfaceEnabled(false);
  Opened = true;
}

void CAnimMenu::Close() {
  Patch::PlayerInterfaceEnabled(true);
  player->SetMovLock(0);
  Opened = false;
}

void CAnimMenu::RunMenuItem() {
  Patch::PlayerInterfaceEnabled(true);
  if (!AnimVector[MenuPos].AniStart.IsEmpty()) {
    player->GetModel()->StartAnimation(AnimVector[MenuPos].AniStart);
  } else
    player->GetModel()->StartAnimation(AnimVector[MenuPos].AniLoop);
  Close();
}

void CAnimMenu::PrintMenu() {
  if (player->IsDead() || player->GetBodyState() == 22)
    Close();
  if (!player->IsMovLock())
    player->SetMovLock(1);

  // INPUT
  if (zinput->KeyToggled(KEY_UP)) {
    if (MenuPos > 0)
      MenuPos--;
    if (PrintFrom > 0) {
      if (MenuPos > PrintFrom)
        PrintFrom--;
    }
  }
  if (zinput->KeyToggled(KEY_DOWN)) {
    if (MenuPos < (int)AnimVector.size() - 1) {
      MenuPos++;
      if (MenuPos > 9)
        PrintFrom++;
    }
  }
  if (zinput->KeyPressed(KEY_RETURN)) {
    RunMenuItem();
  }

  // PRINT
  screen->SetFontColor(Normal);
  screen->Print(6500, 3200, Language::Instance()[Language::ANIMS_MENU]);
  if ((int)AnimVector.size() > 9)
    PrintTo = 10;
  else
    PrintTo = (int)AnimVector.size() - 1;
  int Size = 3400;
  for (int i = PrintFrom; i < PrintFrom + PrintTo; i++) {
    FColors = (MenuPos == i) ? Highlighted : Normal;
    screen->SetFontColor(FColors);
    screen->Print(6500, Size, AnimVector[i].AniName);
    Size += 200;
  }
}