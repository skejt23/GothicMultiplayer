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

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "menu/menu_scene.h"
#include "menu/scene/menu_camera.h"

namespace menu {

class SceneManager {
public:
  using SceneFactory = std::function<std::unique_ptr<MenuScene>()>;

  explicit SceneManager(oCGame* game);
  ~SceneManager();
  SceneManager(const SceneManager&) = delete;
  SceneManager& operator=(const SceneManager&) = delete;

  // Registers only the chosen group. Extended mode includes a basic fallback,
  // but that fallback does not participate in random selection or cycling.
  void Configure(bool extended);
  void RegisterScene(std::string name, SceneFactory factory, bool include_in_cycle = true);
  bool ActivateScene(const std::string& name);
  bool ActivateNextScene();
  bool ActivateRandomScene();
  void Update(float delta_time);
  void Render();
  void ResetActiveScene();
  void StopScene();
  void ShowWeapon();
  void HideWeapon();
  void Cleanup();
  bool HasActiveScene() const {
    return active_scene_ != nullptr;
  }
  MenuCamera& GetCamera() {
    return camera_;
  }
  oCGame* GetGame() const {
    return game_;
  }

private:
  struct Registration {
    std::string name;
    SceneFactory create;
    bool include_in_cycle;
    bool failed = false;
  };
  bool TryActivate(size_t index);
  bool IsReady() const;
  void MarkActiveSceneFailed();

  oCGame* game_ = nullptr;
  MenuCamera camera_;
  zCVob* active_weapon_ = nullptr;  // One owning ZenGin reference.
  bool weapon_requested_ = false;
  bool pending_start_ = false;
  float saved_remove_range_ = 0.0f;
  bool remove_range_saved_ = false;
  std::vector<Registration> scenes_;
  std::unique_ptr<MenuScene> active_scene_;
  std::string active_scene_name_;
};

}  // namespace menu
