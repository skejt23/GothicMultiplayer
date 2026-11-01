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

#include "lua_vob.h"

#include <algorithm>
#include <utility>

#include "Shared/lua_runtime/bind_helpers.h"
#include "ZenGin/zGothicAPI.h"
#include "lua_helpers.h"

using namespace Gothic_II_Addon;

namespace gmp::gothic {

LuaVob::VobInstance::~VobInstance() {
  if (owned && vob) {
    if (vob->GetHomeWorld()) {
      vob->RemoveVobSubtreeFromWorld();
    }
    vob->Release();
  }
  vob = nullptr;
}

/* luagmp (class)
 *
 * This class represents a 3d object in the world.
 *
 * @version  0.3.0
 * @name     Vob
 * @side     client
 * @category Game
 *
 */

/* luagmp (constructor)
 *
 * Creates a new Vob using the provided visual model.
 *
 * @param    (string) model  Visual model name (e.g. "SPHERE.3DS").
 *
 */
LuaVob::LuaVob(const std::string& model) : instance_(std::make_shared<VobInstance>()) {
  instance_->vob = new oCVob();
  instance_->owned = true;
  instance_->pending_visual = model;

  ApplyPendingVisual();
}

LuaVob::LuaVob(std::shared_ptr<VobInstance> instance) : instance_(std::move(instance)) {
}

LuaVob LuaVob::FromExisting(zCVob* vob) {
  auto instance = std::make_shared<VobInstance>();
  instance->vob = vob;
  instance->owned = false;
  return LuaVob(std::move(instance));
}

zCVob* LuaVob::vob() const {
  return instance_ ? instance_->vob : nullptr;
}

zCVob* LuaVob::handle() const {
  return vob();
}

/// Assigns the objectName property using the same value format as its getter.
void LuaVob::setObjectName(const std::string& name) {
  if (auto* handle = vob()) {
    handle->SetObjectName(zSTRING(name.c_str()));
  }
}

/* luagmp (property)
 *
 * Represents the internal engine object name of the Vob.
 *
 * @name     objectName
 * @return   (string)
 */
std::string LuaVob::getObjectName() const {
  if (auto* handle = vob()) {
    return std::string(handle->GetObjectName().ToChar());
  }
  return {};
}

/* luagmp (method)
 * Applies a world-transform snapshot to this Vob.
 * @name setWorldTransform
 * @param (Mat4) matrix New world transform.
 */
void LuaVob::setWorldTransform(const ::lua::types::Mat4& matrix) {
  if (auto* handle = vob()) {
    handle->SetTrafoObjToWorld(lua_helpers::ToZenMat4(matrix));
  }
}

/* luagmp (property)
 *
 * A copy of the Vob world transform. Assign the changed matrix back to update the Vob.
 *
 * @name     worldTransform
 * @return   (Mat4)
 */
/* luagmp (method)
 * Returns a copy of the world transform. Modify the copy and assign it back using `setWorldTransform` to apply changes.
 * @name getWorldTransform
 * @return (Mat4) World-transform snapshot.
 */
::lua::types::Mat4 LuaVob::getWorldTransform() const {
  if (auto* handle = vob()) {
    return lua_helpers::ToLuaMat4(handle->GetNewTrafoObjToWorld());
  }
  return {};
}

/// Assigns the parent property using the same value format as its getter.
void LuaVob::setParent(sol::optional<LuaVob> parent) {
  auto* handle = vob();
  if (!handle) {
    return;
  }

  zCWorld* world = handle->GetHomeWorld();
  if (!world) {
    return;
  }

  if (parent) {
    auto* parent_handle = parent->vob();
    if (parent_handle && parent_handle->GetHomeWorld() == world) {
      world->MoveVobSubtreeTo_novt(handle, parent_handle);
    }
    return;
  }

  world->MoveVobSubtreeToWorldSpace(handle);
}

/* luagmp (property)
 *
 * Represents the reference to the parent Vob.
 *
 * @note     The Vob hierarchy will be lost after changing the world.
 * @name     parent
 * @return   (Vob|nil)
 */
sol::object LuaVob::getParent(sol::this_state ts) const {
  sol::state_view lua(ts);
  auto* handle = vob();
  if (!handle || !handle->globalVobTreeNode) {
    return sol::make_object(lua, sol::lua_nil);
  }

  auto* parent_node = handle->globalVobTreeNode->parent;
  if (!parent_node || !parent_node->data) {
    return sol::make_object(lua, sol::lua_nil);
  }

  return sol::make_object(lua, LuaVob::FromExisting(parent_node->data));
}

/// Assigns the cdDynamic property using the same value format as its getter.
void LuaVob::setCdDynamic(bool enabled) {
  if (auto* handle = vob()) {
    handle->SetCollDetDyn(enabled ? 1 : 0);
  }
}

/* luagmp (property)
 *
 * Represents the state of dynamic collision of the Vob. Enabling this option will prevent other dynamic objects from passing through it.
 *
 * @name     cdDynamic
 * @return   (boolean)
 */
bool LuaVob::getCdDynamic() const {
  if (auto* handle = vob()) {
    return handle->collDetectionDynamic != 0;
  }
  return false;
}

/// Assigns the cdStatic property using the same value format as its getter.
void LuaVob::setCdStatic(bool enabled) {
  if (auto* handle = vob()) {
    handle->SetCollDetStat(enabled ? 1 : 0);
  }
}

/* luagmp (property)
 *
 * Represents the state of static collision of the Vob. Enabling this option will prevent static objects (i.e. world mesh) from passing
 * through it.
 *
 * @name     cdStatic
 * @return   (boolean)
 */
bool LuaVob::getCdStatic() const {
  if (auto* handle = vob()) {
    return handle->collDetectionStatic != 0;
  }
  return false;
}

/// Assigns the farClipZScale property using the same value format as its getter.
void LuaVob::setFarClipZScale(float scale) {
  if (auto* handle = vob()) {
    handle->m_fVobFarClipZScale = scale;
  }
}

/* luagmp (property)
 *
 * Represents the max distance at which the Vob will still be rendered.
 *
 * @name     farClipZScale
 * @return   (number)
 */
float LuaVob::getFarClipZScale() const {
  if (auto* handle = vob()) {
    return handle->m_fVobFarClipZScale;
  }
  return 0.0f;
}

/// Assigns the visual property using the same value format as its getter.
void LuaVob::setVisual(const std::string& visual) {
  if (instance_) {
    instance_->pending_visual = visual;
    ApplyPendingVisual();
  }
}

/* luagmp (property)
 *
 * Represents the model file name used as Vob visual, e.g. "SPHERE.3DS".
 *
 * @name     visual
 * @return   (string)
 */
std::string LuaVob::getVisual() const {
  if (auto* handle = vob()) {
    if (auto* visual = handle->GetVisual()) {
      return std::string(visual->GetObjectName().ToChar());
    }
  }
  return {};
}

/// Assigns the visualAlpha property using the same value format as its getter.
void LuaVob::setVisualAlpha(float alpha) {
  if (auto* handle = vob()) {
    float clamped = std::clamp(alpha, 0.0f, 1.0f);
    handle->visualAlpha = clamped;
    handle->visualAlphaEnabled = clamped < 1.0f ? 1 : 0;
  }
}

/* luagmp (property)
 * Visual opacity from 0.0 (transparent) to 1.0 (opaque).
 *
 * @name     visualAlpha
 * @return   (number)
 *
 */
float LuaVob::getVisualAlpha() const {
  if (auto* handle = vob()) {
    return handle->visualAlpha;
  }
  return 0.0f;
}

/* luagmp (method)
 *
 * This method will set the position of the Vob in the world.
 *
 * @name     setPosition
 * @param    (number) x    Position on X axis.
 * @param    (number) y    Position on Y axis.
 * @param    (number) z    Position on Z axis.
 *
 */
void LuaVob::setPosition(float x, float y, float z) {
  if (auto* handle = vob()) {
    handle->SetPositionWorld(zVEC3(x, y, z));
  }
}

/* luagmp (method)
 *
 * This method will return the position of the Vob in the world.
 *
 * @name     getPosition
 * @return   ({x, y, z})   Table containing x,y,z position.
 *
 */
sol::table LuaVob::getPosition(sol::this_state s) const {
  sol::state_view lua(s);

  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  if (auto* handle = vob()) {
    handle->GetPositionWorld(x, y, z);
  }

  return ::lua::bind_helpers::MakeVec3Table(lua, x, y, z);
}

/* luagmp (method)
 *
 * This method will set the euler rotation of the Vob in the world, in degrees.
 *
 * @name     setRotationDegrees
 * @param    (number) x    Rotation on X axis in degrees.
 * @param    (number) y    Rotation on Y axis in degrees.
 * @param    (number) z    Rotation on Z axis in degrees.
 *
 */
void LuaVob::setRotationDegrees(float x, float y, float z) {
  if (auto* handle = vob()) {
    zMAT4 matrix = handle->GetNewTrafoObjToWorld();
    zVEC3 translation = matrix.GetTranslation();
    matrix.SetByEulerAngles(lua_helpers::LuaRotationToGothicEuler(zVEC3(x, y, z)));
    matrix.SetTranslation(translation);
    handle->SetTrafoObjToWorld(matrix);
  }
}

/* luagmp (method)
 *
 * This method will return the euler rotation of the vob in the world, in degrees.
 *
 * @name     getRotationDegrees
 * @return   ({x, y, z})   Table containing x,y,z rotation.
 *
 */
sol::table LuaVob::getRotationDegrees(sol::this_state s) const {
  sol::state_view lua(s);

  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  if (auto* handle = vob()) {
    zVEC3 euler = lua_helpers::GothicEulerToLuaRotation(handle->GetNewTrafoObjToWorld().GetEulerAngles());
    x = euler.n[VX];
    y = euler.n[VY];
    z = euler.n[VZ];
  }

  return ::lua::bind_helpers::MakeVec3Table(lua, x, y, z);
}

/* luagmp (method)
 *
 * This method will add the vob to the currently loaded world. If the vob is not added, it won't show up.
 *
 * @name     addToWorld
 * @param    (Vob|nil) parent  Optional parent vob to attach to.
 *
 */
void LuaVob::addToWorld(sol::optional<LuaVob> parent) {
  auto* handle = vob();
  if (!handle || handle->GetHomeWorld() != nullptr) {
    return;
  }

  zCWorld* world = nullptr;
  zCVob* parent_handle = nullptr;
  if (parent) {
    parent_handle = parent->vob();
    if (!parent_handle) {
      return;
    }
    world = parent_handle->GetHomeWorld();
  } else if (ogame) {
    world = ogame->GetGameWorld();
  }

  if (!world) {
    return;
  }

  ApplyPendingVisual();
  if (parent_handle) {
    world->AddVobAsChild_novt(handle, parent_handle);
  } else {
    world->AddVob(handle);
  }
}

/* luagmp (method)
 *
 * This method will remove the vob from the currently loaded world.
 *
 * @name     removeFromWorld
 *
 */
void LuaVob::removeFromWorld() {
  if (auto* handle = vob()) {
    if (handle->GetHomeWorld()) {
      handle->RemoveVobSubtreeFromWorld();
    }
  }
}

/* luagmp (method)
 *
 * This method will try to put the vob on the floor. If the difference between vob position and the floor y position is <= 1000, the method succeeds.
 *
 * @name     floor
 *
 */
void LuaVob::floor() {
  auto* handle = vob();
  if (!handle) {
    return;
  }

  if (!handle->GetHomeWorld()) {
    return;
  }

  auto* ocvob = handle->CastTo<oCVob>();
  if (!ocvob) {
    return;
  }

  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  handle->GetPositionWorld(x, y, z);

  zVEC3 position(x, y, z);
  if (ocvob->GetFloorPosition(position)) {
    if (std::abs(position.n[VY] - y) <= 1000.0f) {
      handle->SetPositionWorld(position);
    }
    return;
  }

  zVEC3 original(x, y, z);
  ocvob->SetOnFloor(position);
  handle->GetPositionWorld(x, y, z);
  if (std::abs(y - original.n[VY]) > 1000.0f) {
    handle->SetPositionWorld(original);
  }
}

void LuaVob::ApplyPendingVisual() {
  auto* handle = vob();
  if (!handle || !instance_ || instance_->pending_visual.empty() || !lua_helpers::CanApplyVisual()) {
    return;
  }

  handle->SetVisual(zSTRING(instance_->pending_visual.c_str()));
  instance_->pending_visual.clear();
}

void BindVob(sol::state& lua) {
  sol::usertype<LuaVob> vob_type = lua.new_usertype<LuaVob>("Vob", sol::constructors<LuaVob(const std::string&)>());

  vob_type["setPosition"] = &LuaVob::setPosition;
  vob_type["getPosition"] = &LuaVob::getPosition;
  vob_type["setRotationDegrees"] = &LuaVob::setRotationDegrees;
  vob_type["getRotationDegrees"] = &LuaVob::getRotationDegrees;

  vob_type["addToWorld"] = &LuaVob::addToWorld;
  vob_type["removeFromWorld"] = &LuaVob::removeFromWorld;
  vob_type["floor"] = &LuaVob::floor;

  vob_type["objectName"] = sol::property(&LuaVob::getObjectName, &LuaVob::setObjectName);
  vob_type["worldTransform"] = sol::property(&LuaVob::getWorldTransform, &LuaVob::setWorldTransform);
  vob_type["getWorldTransform"] = &LuaVob::getWorldTransform;
  vob_type["setWorldTransform"] = &LuaVob::setWorldTransform;
  vob_type["parent"] = sol::property(&LuaVob::getParent, &LuaVob::setParent);
  vob_type["cdDynamic"] = sol::property(&LuaVob::getCdDynamic, &LuaVob::setCdDynamic);
  vob_type["cdStatic"] = sol::property(&LuaVob::getCdStatic, &LuaVob::setCdStatic);
  vob_type["farClipZScale"] = sol::property(&LuaVob::getFarClipZScale, &LuaVob::setFarClipZScale);
  vob_type["visual"] = sol::property(&LuaVob::getVisual, &LuaVob::setVisual);
  vob_type["visualAlpha"] = sol::property(&LuaVob::getVisualAlpha, &LuaVob::setVisualAlpha);
}

}  // namespace gmp::gothic
