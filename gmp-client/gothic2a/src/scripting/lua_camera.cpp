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

#include "lua_camera.h"

#include <cstddef>
#include <optional>

#include "Shared/lua_runtime/bind_helpers.h"
#include "ZenGin/zGothicAPI.h"
#include "lua_helpers.h"
#include "net_game.h"

using namespace Gothic_II_Addon;

namespace gmp::gothic {
namespace {
zCAICamera* AiCamera() {
  return zCAICamera::GetCurrent();
}

zCCamera* RenderCamera() {
  return zCCamera::activeCam;
}

zCVob* CameraVob() {
  if (auto* ai = AiCamera()) {
    if (ai->camVob) {
      return ai->camVob;
    }
  }

  if (auto* camera = RenderCamera()) {
    return camera->connectedVob;
  }

  return nullptr;
}

void SetMatrixPosition(zMAT4& matrix, const zVEC3& position) {
  matrix.SetTranslation(position);
}

void SetMatrixRotation(zMAT4& matrix, const zVEC3& rotation) {
  const zVEC3 translation = matrix.GetTranslation();
  matrix.SetByEulerAngles(lua_helpers::LuaRotationToGothicEuler(rotation));
  matrix.SetTranslation(translation);
}

struct MovementLockState {
  bool initialized = false;
  bool enabled = true;
  int translate = 1;
  int rotate = 1;
  int collision = 1;
  zMAT4 frozen_matrix;
};

MovementLockState& MovementLock() {
  static MovementLockState state;
  return state;
}

zMAT4 CurrentCameraTransform() {
  if (auto* vob = CameraVob()) {
    return vob->GetNewTrafoObjToWorld();
  }

  if (auto* camera = RenderCamera()) {
    return camera->camMatrixInv;
  }

  zMAT4 matrix;
  matrix.MakeIdentity();
  return matrix;
}

void ApplyCameraTransform(const zMAT4& matrix) {
  zCVob* vob = CameraVob();
  if (vob) {
    vob->SetTrafoObjToWorld(matrix);
  }

  if (auto* camera = RenderCamera()) {
    if (camera->connectedVob && camera->connectedVob != vob) {
      camera->connectedVob->SetTrafoObjToWorld(matrix);
    }
    if (camera->connectedVob) {
      camera->Activate();
    } else {
      camera->SetTransform(zCAM_TRAFO_WORLDVIEW, matrix.InverseLinTrafo());
    }
  }
}

bool HasCameraTransformTarget() {
  return CameraVob() || RenderCamera();
}

zMAT4 EditableCameraTransform() {
  auto& state = MovementLock();
  if (!state.enabled && state.initialized) {
    return state.frozen_matrix;
  }

  return CurrentCameraTransform();
}

void StoreMovementStateForLock() {
  auto& state = MovementLock();
  if (!state.enabled) {
    return;
  }

  if (auto* ai = AiCamera()) {
    state.translate = ai->translate;
    state.rotate = ai->rotate;
    state.collision = ai->collision;
  }
}

void DisableCameraAiForLock() {
  if (auto* ai = AiCamera()) {
    if (ai->camVob && ai->camVob->callback_ai == static_cast<zCAIBase*>(ai)) {
      ai->camVob->SetAI(nullptr);
    }
  }
}

void LockCameraTransform(const zMAT4& matrix) {
  StoreMovementStateForLock();

  auto& state = MovementLock();
  state.frozen_matrix = matrix;
  state.initialized = true;
  state.enabled = false;

  DisableCameraAiForLock();
  ApplyCameraTransform(state.frozen_matrix);
}

void SetTarget(zCVob* vob) {
  if (auto* ai = AiCamera()) {
    if (!vob) {
      // Gothic's camera AI dereferences target unconditionally in
      // CheckUnderWaterFX. Clearing the scripted target therefore means
      // returning to the current hero, not leaving active AI targetless.
      if (player && ogame && player->GetHomeWorld() == ogame->GetWorld()) {
        ai->SetTarget(player);
      } else {
        if (ai->camVob && ai->camVob->callback_ai == static_cast<zCAIBase*>(ai)) {
          ai->camVob->SetAI(nullptr);
        }
        ai->ClearTargetList();
      }
      return;
    }
    ai->SetTarget(vob);
  }
}

void SetModeChangeEnabledRaw(bool enabled) {
  zCAICamera::bCamChanges = enabled ? 1 : 0;
}

bool GetModeChangeEnabledRaw() {
  return zCAICamera::bCamChanges != 0;
}

bool GetMovementEnabledRaw() {
  auto* ai = AiCamera();
  return ai && ai->camVob && ai->camVob->callback_ai == static_cast<zCAIBase*>(ai);
}

void DisableCameraMovement() {
  if (!HasCameraTransformTarget()) {
    return;
  }

  LockCameraTransform(CurrentCameraTransform());
}

void EnableCameraMovement() {
  auto& state = MovementLock();
  auto* ai = AiCamera();
  if (!ai || !ai->camVob) {
    state.enabled = true;
    state.initialized = false;
    return;
  }

  if (!ai->target) {
    SetTarget(nullptr);
    if (!ai->target) {
      // There is no safe target for Gothic's camera AI yet. Keep it detached
      // until a hero or explicit scripted target becomes available.
      state.enabled = false;
      return;
    }
  }

  if (state.initialized) {
    ai->translate = state.translate;
    ai->rotate = state.rotate;
    ai->collision = state.collision;
  }

  ai->camVob->SetAI(static_cast<zCAIBase*>(ai));
  state.enabled = true;
  state.initialized = false;
}

zCVob* NetworkCharacterVob(std::uint64_t player_id) {
  for (auto* player : NetGame::Instance().players) {
    if (player && player->base_player().id() == player_id) {
      return player->GetNpc();
    }
  }

  return nullptr;
}

enum class CameraTargetKind { None, Vob, Player };

struct CameraTargetBinding {
  CameraTargetKind kind = CameraTargetKind::None;
  std::optional<LuaVob> vob;
  std::optional<std::uint64_t> player_id;
};

CameraTargetBinding& TargetBinding() {
  static CameraTargetBinding binding;
  return binding;
}

void ClearTargetBinding() {
  auto& binding = TargetBinding();
  binding.kind = CameraTargetKind::None;
  binding.vob.reset();
  binding.player_id.reset();
}

zCVob* ResolveBoundTarget() {
  auto& binding = TargetBinding();
  if (binding.kind == CameraTargetKind::Vob) {
    return binding.vob ? binding.vob->handle() : nullptr;
  }

  if (binding.kind == CameraTargetKind::Player && binding.player_id.has_value()) {
    return NetworkCharacterVob(*binding.player_id);
  }

  return nullptr;
}

void RefreshBoundTarget() {
  auto& binding = TargetBinding();
  if (binding.kind == CameraTargetKind::None) {
    return;
  }

  zCVob* target = ResolveBoundTarget();
  if (!target) {
    SetTarget(nullptr);
    ClearTargetBinding();
    return;
  }

  auto* ai = AiCamera();
  if (ai && ai->target != target) {
    SetTarget(target);
  }
}

}  // namespace

/* luagmp (class)
 *
 * Static access to the active engine camera.
 *
 * @version  0.3.0
 * @name     Camera
 * @side     client
 * @category Game
 *
 */

/* luagmp (method)
 *
 * Sets the active camera mode.
 *
 * @name     setMode
 * @param    (string) mode    Camera mode name.
 * @return   (boolean)        True if an AI camera is available.
 *
 */
bool LuaCamera::setMode(const std::string& mode) {
  auto* ai = AiCamera();
  if (!ai) {
    return false;
  }

  zCArray<zCVob*> target_list;
  if (ai->target) {
    target_list.InsertEnd(ai->target);
  }

  zSTRING camera_mode(mode.c_str());
  ai->SetMode(camera_mode, target_list);
  return true;
}

/* luagmp (method)
 *
 * Returns the active camera mode.
 *
 * @name     getMode
 * @return   (string) Camera mode name.
 *
 */
std::string LuaCamera::getMode() {
  auto* ai = AiCamera();
  if (!ai) {
    return {};
  }

  return std::string(ai->GetMode().ToChar());
}

/* luagmp (method)
 *
 * Sets the active camera world position.
 * This puts the camera into manual movement mode until movementEnabled is set to true.
 *
 * @name     setPosition
 * @param    (number) x    X world position.
 * @param    (number) y    Y world position.
 * @param    (number) z    Z world position.
 *
 */
void LuaCamera::setPosition(float x, float y, float z) {
  if (!HasCameraTransformTarget()) {
    return;
  }

  zMAT4 matrix = EditableCameraTransform();
  SetMatrixPosition(matrix, zVEC3(x, y, z));
  LockCameraTransform(matrix);
}

/* luagmp (method)
 *
 * Returns the active camera world position.
 *
 * @name     getPosition
 * @return   ({x, y, z})   Table containing x,y,z world position.
 *
 */
sol::table LuaCamera::getPosition(sol::this_state ts) {
  sol::state_view lua(ts);
  zVEC3 position(0.0f, 0.0f, 0.0f);
  if (auto* vob = CameraVob()) {
    position = vob->GetPositionWorld();
  } else if (auto* camera = RenderCamera()) {
    position = camera->camMatrixInv.GetTranslation();
  }

  return ::lua::bind_helpers::MakeVec3Table(lua, position[VX], position[VY], position[VZ]);
}

/* luagmp (method)
 *
 * Sets the active camera Euler rotation in degrees.
 * This puts the camera into manual movement mode until movementEnabled is set to true.
 *
 * @name     setRotationDegrees
 * @param    (number) x    X rotation in degrees.
 * @param    (number) y    Y rotation in degrees.
 * @param    (number) z    Z rotation in degrees.
 *
 */
void LuaCamera::setRotationDegrees(float x, float y, float z) {
  if (!HasCameraTransformTarget()) {
    return;
  }

  zMAT4 matrix = EditableCameraTransform();
  SetMatrixRotation(matrix, zVEC3(x, y, z));
  LockCameraTransform(matrix);
}

/* luagmp (method)
 *
 * Returns the active camera Euler rotation in degrees.
 *
 * @name     getRotationDegrees
 * @return   ({x, y, z})   Table containing x,y,z rotation.
 *
 */
sol::table LuaCamera::getRotationDegrees(sol::this_state ts) {
  sol::state_view lua(ts);
  zVEC3 rotation(0.0f, 0.0f, 0.0f);
  if (auto* vob = CameraVob()) {
    rotation = vob->GetNewTrafoObjToWorld().GetEulerAngles();
  } else if (auto* camera = RenderCamera()) {
    rotation = camera->camMatrixInv.GetEulerAngles();
  }

  const zVEC3 lua_rotation = lua_helpers::GothicEulerToLuaRotation(rotation);
  return ::lua::bind_helpers::MakeVec3Table(lua, lua_rotation[VX], lua_rotation[VY], lua_rotation[VZ]);
}

/* luagmp (method)
 *
 * Sets a Vob as the active camera target.
 *
 * @name     setTargetVob
 * @param    (Vob) vob    Target Vob.
 * @return   (boolean)    True if the target was applied.
 *
 */
bool LuaCamera::setTargetVob(const LuaVob& vob) {
  zCVob* handle = vob.handle();
  if (!AiCamera() || !handle) {
    return false;
  }

  auto& binding = TargetBinding();
  binding.kind = CameraTargetKind::Vob;
  binding.vob = vob;
  binding.player_id.reset();

  SetTarget(handle);
  return true;
}

/* luagmp (method)
 *
 * Sets a streamed player or server NPC as the active camera target. Local NPC ids are not supported.
 *
 * @name     setTargetPlayer
 * @param    (number) playerId    Streamed player or server NPC id; client-local NPC ids are not supported.
 * @return   (boolean)            True if the player or NPC exists.
 *
 */
bool LuaCamera::setTargetPlayer(std::int64_t player_id) {
  zCVob* handle = NetworkCharacterVob(player_id);
  if (!AiCamera() || !handle) {
    return false;
  }

  auto& binding = TargetBinding();
  binding.kind = CameraTargetKind::Player;
  binding.vob.reset();
  binding.player_id = static_cast<std::uint64_t>(player_id);

  SetTarget(handle);
  return true;
}

/* luagmp (method)
 *
 * Sets the active render camera field of view.
 *
 * @name     setFOV
 * @param    (number) fov    Field of view.
 *
 */
void LuaCamera::setFOV(float fov) {
  if (auto* camera = RenderCamera()) {
    camera->SetFOV(fov);
  }
}

/* luagmp (method)
 *
 * Returns the active render camera field of view.
 *
 * @name     getFOV
 * @return   (number) Field of view.
 *
 */
float LuaCamera::getFOV() {
  if (auto* camera = RenderCamera()) {
    return camera->GetFOV();
  }
  return 0.0f;
}

/* luagmp (property)
 *
 * Represents whether scripted camera mode changes are enabled.
 *
 * @name     modeChangeEnabled
 * @return   (boolean)
 *
 */
bool LuaCamera::getModeChangeEnabled() {
  return GetModeChangeEnabledRaw();
}

/// Assigns the modeChangeEnabled property using the same value format as its getter.
void LuaCamera::setModeChangeEnabled(bool enabled) {
  SetModeChangeEnabledRaw(enabled);
}

/* luagmp (property)
 *
 * Represents whether the camera is allowed to move.
 * Setting this to true restores engine camera AI after manual position or rotation changes.
 *
 * @name     movementEnabled
 * @return   (boolean)
 *
 */
bool LuaCamera::getMovementEnabled() {
  auto& state = MovementLock();
  return state.enabled && GetMovementEnabledRaw();
}

/// Assigns the movementEnabled property using the same value format as its getter.
void LuaCamera::setMovementEnabled(bool enabled) {
  if (enabled) {
    EnableCameraMovement();
  } else {
    DisableCameraMovement();
  }
}

void LuaCamera::ApplyMovementLock() {
  RefreshBoundTarget();

  auto& state = MovementLock();
  if (state.enabled || !state.initialized) {
    return;
  }

  DisableCameraAiForLock();
  ApplyCameraTransform(state.frozen_matrix);
}

/* luagmp (property)
 *
 * Represents the active camera target Vob.
 *
 * @name     targetVob
 * @return   (Vob|nil)
 *
 */
sol::object LuaCamera::getTargetVob(sol::this_state ts) {
  sol::state_view lua(ts);
  RefreshBoundTarget();

  auto* ai = AiCamera();
  if (!ai || !ai->target) {
    return sol::make_object(lua, sol::lua_nil);
  }

  return sol::make_object(lua, LuaVob::FromExisting(ai->target));
}

/// Assigns the targetVob property using the same value format as its getter.
void LuaCamera::setTargetVobValue(sol::object value) {
  if (value.get_type() == sol::type::nil) {
    ClearTargetBinding();
    SetTarget(nullptr);
    return;
  }

  if (value.is<LuaVob>()) {
    setTargetVob(value.as<LuaVob>());
  }
}

void ResetCamera() {
  ClearTargetBinding();
  SetTarget(nullptr);
  EnableCameraMovement();
  SetModeChangeEnabledRaw(true);
}

void BindCamera(sol::state& lua) {
  sol::usertype<LuaCamera> camera_type = lua.new_usertype<LuaCamera>("Camera", sol::no_constructor);

  camera_type["setMode"] = &LuaCamera::setMode;
  camera_type["getMode"] = &LuaCamera::getMode;
  camera_type["setPosition"] = &LuaCamera::setPosition;
  camera_type["getPosition"] = &LuaCamera::getPosition;
  camera_type["setRotationDegrees"] = &LuaCamera::setRotationDegrees;
  camera_type["getRotationDegrees"] = &LuaCamera::getRotationDegrees;
  camera_type["setTargetVob"] = &LuaCamera::setTargetVob;
  camera_type["setTargetPlayer"] = &LuaCamera::setTargetPlayer;
  camera_type["setFOV"] = &LuaCamera::setFOV;
  camera_type["getFOV"] = &LuaCamera::getFOV;

  camera_type["modeChangeEnabled"] = sol::property(&LuaCamera::getModeChangeEnabled, &LuaCamera::setModeChangeEnabled);
  camera_type["movementEnabled"] = sol::property(&LuaCamera::getMovementEnabled, &LuaCamera::setMovementEnabled);
  camera_type["targetVob"] = sol::property(&LuaCamera::getTargetVob, &LuaCamera::setTargetVobValue);

  lua["Camera"] = LuaCamera();
}

}  // namespace gmp::gothic
