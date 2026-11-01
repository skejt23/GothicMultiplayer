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

#include "menu/scene/menu_npc.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <numbers>

#include "world_utils.hpp"

namespace menu {
namespace {
bool IsFinite(const zVEC3& position) {
  return std::isfinite(position[VX]) && std::isfinite(position[VY]) && std::isfinite(position[VZ]);
}
float HorizontalDistance(const zVEC3& a, const zVEC3& b) {
  return std::hypot(a[VX] - b[VX], a[VZ] - b[VZ]);
}

// Native instance constructors change parser bindings, including PC_HERO's
// instance address. A temporary actor must not replace those gameplay bindings.
class NativeInstanceScope {
public:
  NativeInstanceScope(zCParser& parser, int instance)
      : bindings_{Save(parser.GetSymbol("SELF")), Save(parser.GetSymbol("OTHER")), Save(parser.GetSymbol("ITEM")), Save(parser.GetSymbol(instance))},
        instance_symbol_(zCPar_Symbol::instance_sym),
        instance_address_(zCPar_Symbol::instance_adr),
        ai_disabled_(oCNpc::ai_disabled) {
  }
  ~NativeInstanceScope() {
    for (const auto& binding : bindings_) {
      if (binding.symbol) {
        binding.symbol->SetOffset(binding.offset);
      }
    }
    zCPar_Symbol::instance_sym = instance_symbol_;
    zCPar_Symbol::instance_adr = instance_address_;
    oCNpc::ai_disabled = ai_disabled_;
  }

private:
  struct Binding {
    zCPar_Symbol* symbol;
    int offset;
  };
  static Binding Save(zCPar_Symbol* symbol) {
    return symbol && symbol->type == zPAR_TYPE_INSTANCE ? Binding{symbol, symbol->GetOffset()} : Binding{nullptr, 0};
  }
  std::array<Binding, 4> bindings_;
  zCPar_Symbol* instance_symbol_;
  void* instance_address_;
  int ai_disabled_;
};
}  // namespace

bool MenuNpc::CanCreate() {
  return ogame && ogame->GetGameWorld() && zfactory && Gothic_II_Addon::parser && Gothic_II_Addon::player;
}

bool MenuNpc::FindGround(oCWorld& world, const zVEC3& hint, zVEC3& ground) {
  if (!IsFinite(hint)) {
    return false;
  }
  const zVEC3 origin = hint + zVEC3(0.0f, 300.0f, 0.0f);
  if (!world.TraceRayNearestHit(origin, zVEC3(0.0f, -2500.0f, 0.0f), static_cast<zCVob*>(nullptr), zTRACERAY_VOB_IGNORE | zTRACERAY_STAT_POLY)) {
    return false;
  }
  ground = world.traceRayReport.foundIntersection;
  return IsFinite(ground);
}

std::unique_ptr<MenuNpc> MenuNpc::Create(oCWorld& world, const MenuNpcDefinition& definition) {
  if (!CanCreate() || ogame->GetGameWorld() != &world) {
    SPDLOG_WARN("Menu NPC creation: engine/world unavailable");
    return nullptr;
  }
  auto* waypoint = world.wayNet && !definition.spawn_wp.empty() ? world.wayNet->GetWaypoint(zSTRING(definition.spawn_wp.c_str())) : nullptr;
  if (!waypoint) {
    SPDLOG_WARN("Menu NPC spawn waypoint not found: {}", definition.spawn_wp);
    return nullptr;
  }
  auto actor = std::unique_ptr<MenuNpc>(new MenuNpc(world, definition));
  auto& config = actor->definition_;
  if (config.instance.empty()) {
    config.instance = MenuNpcDefinition::kDefaultInstance;
  }
  if (config.name_font.empty()) {
    config.name_font = MenuNpcDefinition::kDefaultFont;
  }
  zSTRING instance_name(config.instance.c_str());
  instance_name.Upper();
  actor->allow_visual_overrides_ = instance_name == zSTRING(MenuNpcDefinition::kDefaultInstance);
  auto* parser = Gothic_II_Addon::parser;
  const int instance = parser->GetIndex(instance_name);
  auto* symbol = instance >= 0 ? parser->GetSymbol(instance) : nullptr;
  const int npc_class = parser->GetIndex("C_NPC");
  if (!symbol || symbol->type != zPAR_TYPE_INSTANCE || npc_class < 0 || parser->GetBaseClass(symbol) != npc_class) {
    SPDLOG_WARN("Menu NPC instance is not a C_NPC: {}", config.instance);
    return nullptr;
  }
  // Acquire the factory reference before running the instance constructor.
  // InitByScript also inserts into logical world lists; our destructor removes
  // those entries even if initialization fails before physical insertion.
  actor->npc_ = zfactory->CreateNpc(-1);
  if (!actor->npc_) {
    SPDLOG_WARN("Menu NPC creation: factory failed");
    return nullptr;
  }
  auto* npc = actor->npc_;
  // Keep this scope through lazy model/AI initialization too: UnShrink can
  // initialize the human controller and change the global AI switch.
  NativeInstanceScope scope(*parser, instance);
  npc->InitByScript(instance, 0);
  // Keep the instance's model, visuals, inventory and overlays, but remove any
  // registered story routine before the engine gets a chance to update it.
  npc->startAIState = 0;
  npc->daily_routine = 0;
  npc->state.SetRoutine(nullptr, nullptr);
  if (rtnMan) {
    rtnMan->RemoveRoutine(npc);
  }
  npc->state.hasRoutine = false;
  npc->state.ClearAIState();
  npc->respawnOn = false;
  npc->ClearPerception();
  npc->ClearPerceptionLists();
  npc->noFocus = true;
  npc->name[0].Clear();
  npc->SetMovLock(true);
  auto* model = npc->GetModel();  // Loads the instance's model if still deferred.
  if (!model) {
    SPDLOG_WARN("Menu NPC instance has no model: {}", config.instance);
    return nullptr;
  }
  if (!actor->SetAdditionalVisuals(config.visual)) {
    return nullptr;
  }
  model->SetRandAnisEnabled(false);
  model->doVobRot = false;
  actor->feet_offset_ = -model->bbox3DLocalFixed.mins[VY];
  zVEC3 ground;
  if (!std::isfinite(actor->feet_offset_) || !FindGround(world, waypoint->GetPositionWorld(), ground)) {
    SPDLOG_WARN("Menu NPC creation: no ground at waypoint {}", config.spawn_wp);
    return nullptr;
  }
  actor->position_ = ground + zVEC3(0.0f, actor->feet_offset_, 0.0f);
  // AddVob invokes oCWorld::InsertInLists and the physical world insertion.
  // Unlike Enable, it does not run Interrupt, equip inventory or find a routine.
  npc->SetPositionWorld(actor->position_);
  world.AddVob(npc);
  if (npc->GetHomeWorld() != &world) {
    SPDLOG_WARN("Menu NPC creation: AddVob did not attach the actor to the menu world");
    return nullptr;
  }
  npc->InitHumanAI();
  if (!npc->human_ai) {
    SPDLOG_WARN("Menu NPC creation: human controller unavailable");
    return nullptr;
  }
  // Scene behavior decides when to draw weapons, including for instances that
  // start with a weapon in hand. Sheathe before replacing equipment slots.
  npc->SetWeaponMode2(NPC_WEAPON_NONE);

  // Keep the human controller alive (oCNpc retains pointers to it), but don't
  // schedule its DoAI: no routines, perception, physics or root-motion navigation.
  actor->detached_ai_ = npc->callback_ai;
  if (actor->detached_ai_) {
    actor->detached_ai_->AddRef();
    npc->SetAI(nullptr);
  }
  npc->SetPhysicsEnabled(false);
  npc->SetCollDetStat(false);
  npc->SetCollDetDyn(false);
  npc->GetEM()->Clear();
  npc->SetPositionWorld(actor->position_);
  // Sleeping suppresses zCVob::DoFrameActivity and OnTick. Update advances the
  // visual and applies animation movement only when freeze_on_wp permits it.
  npc->SetSleeping(true);
  if (!actor->ApplyOverlay(config.overlay) || (!config.equip_armor.empty() && !actor->EquipArmor(config.equip_armor.c_str())) ||
      (!config.equip_melee.empty() && !actor->EquipMelee(config.equip_melee.c_str())) ||
      (!config.equip_ranged.empty() && !actor->EquipRanged(config.equip_ranged.c_str())) || !actor->ResetToSpawn()) {
    return nullptr;
  }
  if (config.draw_melee && !actor->DrawMeleeWeapon()) {
    SPDLOG_WARN("Menu NPC '{}' could not draw its melee weapon", config.name);
    return nullptr;
  }
  if (!config.animation.empty() && !actor->PlayAnimation(config.animation.c_str(), config.animation_loop)) {
    SPDLOG_WARN("Menu NPC '{}' could not start its initial animation: {}", config.name, config.animation);
    return nullptr;
  }
  SPDLOG_DEBUG("Created menu NPC: {:p}", static_cast<void*>(npc));
  return actor;
}

bool MenuNpc::ResetToSpawn() {
  return PlaceAtWaypoint(definition_.spawn_wp.c_str());
}

bool MenuNpc::WalkToEndWaypoint(float speed) {
  return WalkToWaypoint(definition_.end_wp.c_str(), speed);
}

MenuNpc::~MenuNpc() {
  if (!npc_) {
    world_.Release();
    return;
  }
  definition_.name.clear();
  StopMovement();
  StopAnimation();
  npc_->GetEM()->Clear();
  ClearNpcFocusReferences(npc_);
  // Remove through the virtual world API, which also updates oCWorld's NPC
  // lists and removes event/world dependencies. Never delete a refcounted vob.
  // Remove even if physically disabled: oCWorld's logical lists can still own
  // references when GetHomeWorld() is null. The retained world is still valid.
  world_.RemoveVob(npc_);
  if (detached_ai_) {
    // RemoveFromLists calls oCNpc::CleanUp/DeleteHumanAI. Release our extra
    // controller reference only after that cleanup; never reattach a dead AI.
    detached_ai_->Release();
    detached_ai_ = nullptr;
  }
  SPDLOG_DEBUG("Destroyed menu NPC: {:p}", static_cast<void*>(npc_));
  npc_->Release();
  world_.Release();
}

bool MenuNpc::SetAdditionalVisuals(const MenuNpcVisual& visual) {
  if (!allow_visual_overrides_) {
    return true;
  }
  if (!npc_ || visual.body.empty() || visual.head.empty()) {
    return false;
  }
  StopMovement();
  StopAnimation();
  zSTRING body(visual.body.c_str());
  zSTRING head(visual.head.c_str());
  npc_->SetAdditionalVisuals(body, visual.body_texture, 0, head, visual.head_texture, 0, 0);
  auto* model = npc_->GetModel();
  if (!model || model->meshSoftSkinList.GetNumInList() == 0) {
    SPDLOG_WARN("Menu NPC visual failed: body={}, head={}", visual.body, visual.head);
    return false;
  }
  model->SetRandAnisEnabled(false);
  model->doVobRot = false;
  feet_offset_ = -model->bbox3DLocalFixed.mins[VY];
  logged_missing_head_ = false;
  return std::isfinite(feet_offset_);
}

bool MenuNpc::ApplyOverlay(const std::string& overlay) {
  if (!allow_visual_overrides_ || overlay.empty()) {
    return true;
  }
  auto* model = npc_ ? npc_->GetModel() : nullptr;
  zSTRING overlay_name(overlay.c_str());
  overlay_name.Upper();
  if (npc_ && npc_->activeOverlays.IsInList(overlay_name))
    return true;
  // Validate before the native NPC API, which asserts for a missing prototype.
  auto* prototype = model && model->modelProtoList.GetNumInList() > 0 ? zCModelPrototype::Load(overlay_name, model->modelProtoList[0]) : nullptr;
  if (!prototype) {
    SPDLOG_WARN("Menu NPC overlay failed to load: {}", overlay);
    return false;
  }
  if (model->HasAppliedModelProtoOverlay(prototype))
    model->RemoveModelProtoOverlay(prototype);
  // The engine must allocate its own zCArray<zSTRING>: its delete[] ABI differs
  // from the dependency's container helpers used by GMP.
  const bool applied = npc_->ApplyOverlay(overlay_name) != 0;
  prototype->Release();
  if (!applied)
    return false;
  if (npc_->human_ai) {
    npc_->human_ai->InitAnimations();
  }
  return true;
}

bool MenuNpc::EquipArmor(const char* instance) {
  return Equip(instance, Equipment::Armor);
}
bool MenuNpc::EquipMelee(const char* instance) {
  return Equip(instance, Equipment::Melee);
}
bool MenuNpc::EquipRanged(const char* instance) {
  return Equip(instance, Equipment::Ranged);
}

bool MenuNpc::Equip(const char* instance, Equipment kind) {
  auto* parser = Gothic_II_Addon::parser;
  if (!IsValid() || !parser || !zfactory || !instance || !*instance) {
    return false;
  }
  const int index = parser->GetIndex(zSTRING(instance));
  auto* symbol = index >= 0 ? parser->GetSymbol(index) : nullptr;
  const int item_class = parser->GetIndex("C_ITEM");
  if (!symbol || symbol->type != zPAR_TYPE_INSTANCE || item_class < 0 || parser->GetBaseClass(symbol) != item_class) {
    SPDLOG_WARN("Menu NPC equipment is not an item instance: {}", instance);
    return false;
  }
  NativeInstanceScope scope(*parser, index);
  const bool armor = kind == Equipment::Armor;
  const bool ranged = kind == Equipment::Ranged;
  auto* previous = armor ? npc_->GetEquippedArmor() : ranged ? npc_->GetEquippedRangedWeapon() : npc_->GetEquippedMeleeWeapon();
  if (previous && previous->GetInstance() == index) {
    // Native Equip methods toggle an already active item off.
    previous->on_equip = 0;
    previous->on_unequip = 0;
    return true;
  }
  const auto release = [](oCItem* item) { item->Release(); };
  std::unique_ptr<oCItem, decltype(release)> item(zfactory->CreateItem(index), release);
  const char* category = armor ? "torso armor" : ranged ? "bow/crossbow" : "melee weapon";
  const bool valid = item && (armor    ? item->HasFlag(ITM_CAT_ARMOR) && item->wear == ITM_WEAR_TORSO
                              : ranged ? item->HasFlag(ITM_CAT_FF) && (item->HasFlag(ITM_FLAG_BOW) || item->HasFlag(ITM_FLAG_CROSSBOW))
                                       : item->HasFlag(ITM_CAT_NF) && (item->IsOneHandWeapon() || item->TwoHanded()));
  if (!valid) {
    SPDLOG_WARN("Menu NPC equipment has the wrong category: {} (expected {})", instance, category);
    return false;
  }
  if (armor ? item->visual_change.IsEmpty() : item->file.IsEmpty()) {
    SPDLOG_WARN("Menu NPC equipment has no visual definition: {}", instance);
    return false;
  }
  // These are local costume props. Equipping them must not execute gameplay
  // callbacks or leave effect vobs behind when the menu scene is destroyed.
  item->on_equip = 0;
  item->on_unequip = 0;
  item->effectName.Clear();
  auto* inventory_item = npc_->PutInInv(item.get());
  if (!inventory_item) {
    return false;
  }
  // Insert can merge into an existing inventory stack from the NPC instance.
  inventory_item->on_equip = 0;
  inventory_item->on_unequip = 0;
  inventory_item->effectName.Clear();
  if (previous) {
    previous->on_unequip = 0;
  }
  if (armor) {
    npc_->EquipArmor(inventory_item);
  } else {
    // CreateItem initializes the script fields, but the mesh is loaded lazily.
    // Load after inventory insertion (which can discard an item's visual),
    // before EquipWeapon attaches that visual to the model's weapon node.
    inventory_item->CreateVisual();
    if (!inventory_item->GetVisual()) {
      SPDLOG_WARN("Menu NPC weapon visual failed to load: {} ({})", instance, inventory_item->file.ToChar());
      return false;
    }
    if (ranged) {
      npc_->EquipFarWeapon(inventory_item);
    } else {
      npc_->EquipWeapon(inventory_item);
    }
  }
  // Inventory and equipment slots own their references after the factory ref
  // is released here. oCNpc cleanup removes slots and destroys the inventory.
  npc_->SetSleeping(true);
  const bool equipped = (armor    ? npc_->GetEquippedArmor()
                         : ranged ? npc_->GetEquippedRangedWeapon()
                                  : npc_->GetEquippedMeleeWeapon()) == inventory_item;
  if (!equipped) {
    SPDLOG_WARN("Menu NPC could not equip {}", instance);
  }
  return equipped;
}

bool MenuNpc::DrawMeleeWeapon() {
  if (!IsValid() || !npc_->human_ai || !npc_->GetEquippedMeleeWeapon()) {
    return false;
  }
  const int mode = npc_->GetEquippedMeleeWeapon()->TwoHanded() ? NPC_WEAPON_2HS : NPC_WEAPON_1HS;
  npc_->SetWeaponMode2(mode);
  npc_->SetSleeping(true);
  return npc_->GetWeaponMode() == mode;
}

bool MenuNpc::SetPosition(const zVEC3& position) {
  if (!npc_ || !IsFinite(position)) {
    return false;
  }
  StopMovement();
  current_waypoint_.clear();
  position_ = position;
  npc_->SetPositionWorld(position_);
  return true;
}

bool MenuNpc::PlaceAtWaypoint(const char* name) {
  auto* waypoint = world_.wayNet && name && *name ? world_.wayNet->GetWaypoint(zSTRING(name)) : nullptr;
  zVEC3 ground;
  if (!IsValid() || !waypoint || waypoint->waterDepth > 0 || waypoint->underWater || !FindGround(world_, waypoint->GetPositionWorld(), ground) ||
      !SetPosition(ground + zVEC3(0.0f, feet_offset_, 0.0f))) {
    SPDLOG_WARN("Menu NPC cannot use spawn waypoint: {}", name ? name : "<empty>");
    return false;
  }
  current_waypoint_ = name;
  zVEC3 direction = waypoint->dir;
  direction[VY] = 0.0f;
  if (IsFinite(direction) && direction.Length_Sqr() > 0.001f) {
    npc_->SetHeadingAtWorld(direction / direction.Length());
  }
  npc_->SetSleeping(true);
  return true;
}

void MenuNpc::SetRotation(float yaw) {
  if (npc_ && std::isfinite(yaw)) {
    npc_->ResetRotationsWorld();
    npc_->RotateWorldY(std::remainder(yaw, 360.0f));
  }
}

zVEC3 MenuNpc::GetPosition() const {
  return position_;
}

bool MenuNpc::PlayAnimation(const char* name, bool loop) {
  if (!npc_ || !name) {
    return false;
  }
  auto* model = npc_->GetModel();
  const int id = model ? model->GetAniIDFromAniName(zSTRING(name)) : -1;
  if (id < 0) {
    SPDLOG_WARN("Failed to play menu NPC animation: {}", name);
    return false;
  }
  StopMovement();
  StopAnimation();
  // Clear automatic follow-up animations left by the previous explicit clip.
  model->StopAnisLayerRange(0, 100);
  animation_id_ = id;
  loop_animation_ = loop;
  animation_failed_ = false;
  model->StartAni(id, 0);
  return model->GetActiveAni(id) != nullptr;
}

void MenuNpc::StopAnimation() {
  if (npc_ && animation_id_ >= 0) {
    if (auto* model = npc_->GetModel()) {
      model->StopAnisLayerRange(0, 100);
    }
  }
  animation_id_ = -1;
  loop_animation_ = false;
}

bool MenuNpc::HasAnimationFinished() const {
  auto* model = npc_ ? npc_->GetModel() : nullptr;
  return !model || animation_id_ < 0 || !model->GetActiveAni(animation_id_);
}

bool MenuNpc::WalkTo(const zVEC3& destination, float speed) {
  return BeginWalk({destination}, speed);
}

bool MenuNpc::WalkToWaypoint(const char* destination, float speed) {
  const std::string source = current_waypoint_;
  StopMovement();
  auto* waynet = world_.wayNet;
  auto* from = waynet && !source.empty() ? waynet->GetWaypoint(zSTRING(source.c_str())) : nullptr;
  auto* to = waynet && destination && *destination ? waynet->GetWaypoint(zSTRING(destination)) : nullptr;
  if (!IsValid() || !from || !to) {
    SPDLOG_WARN("Menu NPC waynet endpoints missing: {} -> {}", source, destination ? destination : "<empty>");
    FailMovement();
    return false;
  }
  std::unique_ptr<zCRoute> native_route(waynet->FindRoute(from, to, npc_));
  if (!native_route) {
    SPDLOG_WARN("Menu NPC waynet route not found: {} -> {}", source, destination);
    FailMovement();
    return false;
  }
  std::vector<zVEC3> positions;
  zCWaypoint* last = nullptr;
  while (auto* waypoint = native_route->GetNextWP()) {
    const auto* way = native_route->GetCurrentWay();
    if (positions.size() >= 128 || waypoint->waterDepth > 0 || waypoint->underWater || (way && (way->jump || way->chasm))) {
      SPDLOG_WARN("Menu NPC waynet route needs unsupported traversal: {} -> {}", source, destination);
      FailMovement();
      return false;
    }
    positions.push_back(waypoint->GetPositionWorld());
    last = waypoint;
  }
  if (last != to || !BeginWalk(std::move(positions), speed)) {
    FailMovement();
    return false;
  }
  destination_waypoint_ = destination;
  SPDLOG_DEBUG("Menu NPC follows waynet: {} -> {} ({} points)", source, destination, route_.size());
  return true;
}

bool MenuNpc::BeginWalk(std::vector<zVEC3> route, float speed) {
  StopMovement();
  if (!IsValid() || route.empty() || !std::isfinite(speed) || speed <= 0.0f || speed > 1000.0f) {
    FailMovement();
    return false;
  }
  float distance = 0.0f;
  auto previous = position_;
  for (const auto& point : route) {
    if (!IsFinite(point)) {
      FailMovement();
      return false;
    }
    distance += HorizontalDistance(previous, point);
    previous = point;
  }
  if (!std::isfinite(distance) || distance > 100000.0f) {
    FailMovement();
    return false;
  }
  if (!PlayAnimation("S_WALKL", true)) {
    FailMovement();
    return false;
  }
  route_ = std::move(route);
  route_index_ = 0;
  current_waypoint_.clear();
  speed_ = speed;
  movement_time_left_ = distance / speed + 5.0f;
  movement_ = Movement::Walking;
  return true;
}

void MenuNpc::StopMovement() {
  if (movement_ == Movement::Walking) {
    StopAnimation();
  }
  movement_ = Movement::Idle;
  movement_time_left_ = 0.0f;
  route_.clear();
  route_index_ = 0;
  destination_waypoint_.clear();
}

void MenuNpc::FailMovement() {
  StopMovement();
  movement_ = Movement::Failed;
  SPDLOG_WARN("Menu NPC movement failed: invalid destination/speed, obstructed path or missing ground/animation");
}

bool MenuNpc::IsValid() const {
  return npc_ && npc_->GetHomeWorld() == &world_ && npc_->visual && !animation_failed_;
}

void MenuNpc::SetVisible(bool visible) {
  if (npc_) {
    npc_->showVisual = visible;
  }
}

bool MenuNpc::ApplyAnimationMovement(const zCModel& model) {
  // These fields back the native GetLastPosDelta/GetLastRotDelta accessors.
  // AdvanceAnis already accounts for frame time, model scale and animation
  // loops. Transform its local displacement once; do not multiply by dt again.
  const zVEC3 translation = model.vobTrans;
  if (!IsFinite(translation)) {
    return false;
  }
  zMAT4 transform = npc_->trafoObjToWorld;
  const zVEC3 next = transform * translation;
  if (!IsFinite(next)) {
    return false;
  }
  transform.SetTranslation(next);
  if (model.doVobRot) {
    // Match zCVob::DoFrameActivity: retain only root rotation around local Y.
    const float y = model.vobRot.q[VY];
    const float w = model.vobRot.q[VW];
    const float length = std::hypot(y, w);
    if (!std::isfinite(length)) {
      return false;
    }
    if (length > 0.000001f) {
      const zCQuat yaw(0.0f, y / length, 0.0f, w / length);
      zMAT4 rotation = zMAT4::GetIdentity();
      yaw.QuatToMatrix4(rotation);
      transform = transform * rotation;
    }
  }
  npc_->SetTrafoObjToWorld(transform);
  position_ = next;
  if (translation.Length_Sqr() > 0.0f) {
    // A subsequent waynet walk must start from an explicitly placed waypoint.
    current_waypoint_.clear();
  }
  return true;
}

void MenuNpc::Update(float delta_time) {
  if (!IsValid() || !std::isfinite(delta_time) || delta_time <= 0.0f) {
    return;
  }
  if (loop_animation_ && HasAnimationFinished()) {
    auto* model = npc_->GetModel();
    model->StopAnisLayerRange(0, 100);
    model->StartAni(animation_id_, 0);
    if (!model->GetActiveAni(animation_id_)) {
      loop_animation_ = false;
      animation_failed_ = true;
      SPDLOG_WARN("Menu NPC could not restart animation {}", animation_id_);
      return;
    }
  }
  auto* model = npc_->GetModel();
  // AdvanceAnis uses the engine timer. Scale only this model for a clamped
  // frame; do not alter global timing or shared animation prototypes.
  if (model && ztimer && ztimer->frameTimeFloatSecs > 0.0f) {
    const float time_scale = model->timeScale;
    model->timeScale = time_scale * delta_time / ztimer->frameTimeFloatSecs;
    model->AdvanceAnis();
    model->timeScale = time_scale;
    // Waynet/direct walks own their translation and heading, regardless of
    // freeze_on_wp. Applying the walk clip's root motion would move them twice.
    if (!definition_.freeze_on_wp && movement_ != Movement::Walking && animation_id_ >= 0 && !ApplyAnimationMovement(*model)) {
      animation_failed_ = true;
      SPDLOG_WARN("Menu NPC animation {} produced invalid movement", animation_id_);
      return;
    }
    npc_->SetBBox3DLocal(model->GetBBox3D());
  }
  if (movement_ == Movement::Walking) {
    movement_time_left_ -= delta_time;
    if (movement_time_left_ <= 0.0f) {
      FailMovement();
      return;
    }
    float remaining = speed_ * delta_time;
    // Tie facing to travel distance: 120 world units/second gives 180 degrees/
    // second, and changing walking speed scales both movement and turning.
    constexpr float kTurnRadiansPerUnit = std::numbers::pi_v<float> / 120.0f;
    float turn_budget = remaining * kTurnRadiansPerUnit;
    while (route_index_ < route_.size() && remaining > 0.0f) {
      const auto& destination = route_[route_index_];
      const float distance = HorizontalDistance(position_, destination);
      if (distance <= 1.0f) {
        ++route_index_;
        continue;
      }
      const zVEC3 direction(destination[VX] - position_[VX], 0.0f, destination[VZ] - position_[VZ]);
      const zVEC3 heading = npc_->GetAtVectorWorld();
      if (!IsFinite(heading)) {
        FailMovement();
        return;
      }
      const float yaw = std::atan2(heading[VX], heading[VZ]);
      const float target_yaw = std::atan2(direction[VX], direction[VZ]);
      const float difference = std::remainder(target_yaw - yaw, 2.0f * std::numbers::pi_v<float>);
      const float turn = std::clamp(difference, -turn_budget, turn_budget);
      const float next_yaw = yaw + turn;
      npc_->SetHeadingAtWorld(zVEC3(std::sin(next_yaw), 0.0f, std::cos(next_yaw)));
      // All ground samples and waypoint crossings share one frame's turn budget.
      turn_budget = std::max(0.0f, turn_budget - std::abs(turn));
      // Bound each ground/collision sample even if an author chooses a high speed.
      const float step = std::min({remaining, distance, 25.0f});
      zVEC3 next = position_ + direction * (step / distance);
      zVEC3 ground;
      const auto ray = next - position_;
      if (world_.TraceRayNearestHit(position_, ray, static_cast<zCVob*>(nullptr), zTRACERAY_VOB_IGNORE | zTRACERAY_STAT_POLY) ||
          !FindGround(world_, next - zVEC3(0.0f, feet_offset_, 0.0f), ground) || std::abs(ground[VY] + feet_offset_ - position_[VY]) > 50.0f) {
        FailMovement();
        return;
      }
      next[VY] = ground[VY] + feet_offset_;
      position_ = next;
      remaining -= step;
      if (step >= distance) {
        ++route_index_;
      }
    }
    if (route_index_ == route_.size()) {
      const auto reached = destination_waypoint_;
      StopMovement();
      current_waypoint_ = reached;
      movement_ = Movement::Reached;
    }
  }
  npc_->SetPositionWorld(position_);
  npc_->SetSleeping(true);
}

bool MenuNpc::HasVisibleNameplate() const {
  return IsValid() && definition_.name_show && !definition_.name.empty() && npc_->showVisual;
}

std::uint8_t MenuNpc::GetNameplateAlpha(const zVEC3& camera_position) const {
  const float start = definition_.name_fade_start;
  const float end = definition_.name_fade_end;
  if (!HasVisibleNameplate() || !zCCamera::activeCam || !IsFinite(camera_position) || !std::isfinite(start) || !std::isfinite(end) || start < 0.0f ||
      end <= start) {
    return 0;
  }
  const float distance = (npc_->GetPositionWorld() - camera_position).Length();
  if (!std::isfinite(distance) || distance >= end) {
    return 0;
  }
  const float distance_alpha = std::clamp((end - distance) / (end - start), 0.0f, 1.0f);
  // Respect the actor's native distance fade, world draw distance and visual
  // alpha too, so a label cannot outlast a model that has already faded away.
  zCVisual* visual = npc_->visual;  // Borrowed; CalcRenderAlpha does not add a reference.
  float visual_alpha = 0.0f;
  npc_->CalcRenderAlpha(distance, visual, visual_alpha);
  if (!visual || !std::isfinite(visual_alpha)) {
    return 0;
  }
  return static_cast<std::uint8_t>(255.0f * distance_alpha * std::clamp(visual_alpha, 0.0f, 1.0f));
}

zVEC3 MenuNpc::GetNameplatePosition() const {
  if (auto* model = npc_ ? npc_->GetModel() : nullptr) {
    if (auto* head = model->SearchNode(zSTRING("ZS_HEAD"))) {
      return npc_->GetTrafoModelNodeToWorld(head).GetTranslation() + zVEC3(0.0f, nameplate_offset_, 0.0f);
    }
    if (!logged_missing_head_) {
      SPDLOG_DEBUG("Menu NPC has no ZS_HEAD node; using model height for its nameplate");
      logged_missing_head_ = true;
    }
    return position_ + zVEC3(0.0f, model->bbox3DLocalFixed.maxs[VY] + nameplate_offset_, 0.0f);
  }
  return position_ + zVEC3(0.0f, 120.0f + nameplate_offset_, 0.0f);
}

}  // namespace menu
