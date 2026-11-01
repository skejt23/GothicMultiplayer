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

#include <vector>

#include "menu/menu_scene.h"
#include "menu/scene/menu_camera.h"
#include "menu/scene/menu_nameplate_renderer.h"
#include "menu/scene/menu_npc.h"

namespace menu::scenes {

class CreditsWalkScene final : public MenuScene {
public:
  CreditsWalkScene(oCGame* game, MenuCamera& camera) : game_(game), camera_(camera) {
  }
  ~CreditsWalkScene() override {
    Stop();
  }
  MenuSceneSettings GetSettings() const override;
  bool Start() override;
  void Update(float delta_time) override;
  void Render() override;
  void Stop() override;
  bool IsHealthy() const override;

private:
  bool ResetWalkers();
  oCGame* game_;
  MenuCamera& camera_;
  MenuNameplateRenderer nameplates_;
  std::vector<std::unique_ptr<MenuNpc>> walkers_;
  float reset_delay_ = 0.0f;
  bool healthy_ = false;
};

}  // namespace menu::scenes
