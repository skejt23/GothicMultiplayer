/*
MIT License

Copyright (c) 2025 Gothic Multiplayer Team.

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

#include <memory>

#include "menu/menu_scene_manager.h"
#include "menu/scenes/scene_registry.h"
#include "menu/scenes/basic/scene_default.h"
#include "menu/scenes/basic/newworld_scenes.h"
#include "menu/scenes/extended/credits_walk_scene.h"
#include "menu/scenes/extended/training_scene.h"

namespace menu::scenes {

// Basic scenes. Extended mode registers only the default as a fallback.
void RegisterBasicMenuScenes(SceneManager& manager, bool include_in_cycle) {
  manager.RegisterScene(
      kDefaultSceneName, [game = manager.GetGame()] { return std::make_unique<DefaultMenuScene>(game, nullptr, kDefaultSceneSettings); },
      include_in_cycle);
  if (!include_in_cycle) {
    return;  // Extended mode needs only this fallback, not the basic scene pool.
  }
  manager.RegisterScene(
      "newworld_01", [game = manager.GetGame()] { return std::make_unique<DefaultMenuScene>(game, nullptr, kNewWorldScene01); }, include_in_cycle);
  manager.RegisterScene(
      "newworld_02", [game = manager.GetGame()] { return std::make_unique<DefaultMenuScene>(game, nullptr, kNewWorldScene02); }, include_in_cycle);
  manager.RegisterScene(
      "newworld_03", [game = manager.GetGame()] { return std::make_unique<DefaultMenuScene>(game, nullptr, kNewWorldScene03); }, include_in_cycle);
  manager.RegisterScene(
      "newworld_04", [game = manager.GetGame()] { return std::make_unique<DefaultMenuScene>(game, nullptr, kNewWorldScene04); }, include_in_cycle);
  manager.RegisterScene(
      "newworld_05", [game = manager.GetGame()] { return std::make_unique<DefaultMenuScene>(game, nullptr, kNewWorldScene05); }, include_in_cycle);
  manager.RegisterScene(
      "newworld_06", [game = manager.GetGame()] { return std::make_unique<DefaultMenuScene>(game, nullptr, kNewWorldScene06); }, include_in_cycle);
  manager.RegisterScene(
      "newworld_07", [game = manager.GetGame()] { return std::make_unique<DefaultMenuScene>(game, nullptr, kNewWorldScene07); }, include_in_cycle);
  manager.RegisterScene(
      "newworld_08", [game = manager.GetGame()] { return std::make_unique<DefaultMenuScene>(game, nullptr, kNewWorldScene08); }, include_in_cycle);
  manager.RegisterScene(
      "newworld_09", [game = manager.GetGame()] { return std::make_unique<DefaultMenuScene>(game, nullptr, kNewWorldScene09); }, include_in_cycle);
  manager.RegisterScene(
      "newworld_10", [game = manager.GetGame()] { return std::make_unique<DefaultMenuScene>(game, nullptr, kNewWorldScene10); }, include_in_cycle);
  manager.RegisterScene(
      "newworld_11", [game = manager.GetGame()] { return std::make_unique<DefaultMenuScene>(game, nullptr, kNewWorldScene11); }, include_in_cycle);
  manager.RegisterScene(
      "newworld_12", [game = manager.GetGame()] { return std::make_unique<DefaultMenuScene>(game, nullptr, kNewWorldScene12); }, include_in_cycle);
  manager.RegisterScene(
      "newworld_13", [game = manager.GetGame()] { return std::make_unique<DefaultMenuScene>(game, nullptr, kNewWorldScene13); }, include_in_cycle);
}

// Extended scenes. Settings and actor behavior live in each scene's .cpp file.
void RegisterExtendedMenuScenes(SceneManager& manager) {
  auto* game = manager.GetGame();
  auto* camera = &manager.GetCamera();
  manager.RegisterScene("TrainingScene", [game, camera] { return std::make_unique<TrainingScene>(game, *camera); });
  manager.RegisterScene("CreditsWalkScene", [game, camera] { return std::make_unique<CreditsWalkScene>(game, *camera); });
}

}  // namespace menu::scenes
