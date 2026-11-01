
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

/*****************************************************************************
*** 	Created by:		16/12/11	-	skejt23/Pampi
*** 	Description:	Player class
***
*****************************************************************************/

#include "gothic2a_player.hpp"

#include <spdlog/spdlog.h>

#include <cstdio>
#include <list>
#include <array>
#include <optional>
#include <vector>

#include "CIngame.h"
#include "CInterpolatePos.h"
#include "net_game.h"
#include "Mod.h"
#include "script_instance_scope.h"
#include "world_utils.hpp"
#include "torch_utils.h"

// NEEDED FOR SETTING NPC TYPES
#define B_SETVISUALS "B_SETVISUALS"
#define LESSER_SKELETON "LESSER_SKELETON"
#define SKELETON "SKELETON"
#define SKELETON_MAGE "SKELETON_MAGE"
#define SKELETON_LORD "SKELETON_LORD"
static int HeroInstance;

// Externs
extern CIngame* global_ingame;

namespace {
void CloseSpellBook(oCNpc* npc) {
  if (!npc) {
    return;
  }

  if (auto* spell_book = npc->GetSpellBook()) {
    spell_book->Close(1);
  }
}

void SetLocalHero(oCNpc* npc) {
  npc->SetAsPlayer();
  Gothic_II_Addon::player = npc;
  if (auto* camera = zCAICamera::GetCurrent()) {
    camera->SetTarget(npc);
  }
}

void RemoveNpcFromWorld(oCNpc* npc) {
  if (!npc || !ogame || !ogame->GetGameWorld()) {
    return;
  }

  npc->AddRef();
  ClearNpcFocusReferences(npc);
  if (ogame->GetSpawnManager()) {
    ogame->GetSpawnManager()->DeleteNpc(npc);
  } else {
    npc->Disable();
    ogame->GetGameWorld()->RemoveVob(npc);
  }
  npc->Release();
}


}  // namespace

// NPC INSTANCES
constexpr const char* PCHERO = "PC_HERO";
constexpr const char* ORCWARRIOR = "ORCWARRIOR_ROAM";
constexpr const char* ORCELITE = "ORCELITE_ROAM";
constexpr const char* ORCSHAMAN = "ORCSHAMAN_SIT";
constexpr const char* UNDEADORC = "UNDEADORCWARRIOR";
constexpr const char* SHEEP = "SHEEP";
constexpr const char* DRACONIAN = "DRACONIAN";

Gothic2APlayer::Gothic2APlayer(gmp::client::Player& base_player, bool is_local_player)
    : base_player_(base_player), is_local_player_(is_local_player) {
  this->npc = NULL;
  this->ScriptInstance = NULL;
  this->InterPos = new CInterpolatePos(this);
  this->Type = NPC_HUMAN;
}

Gothic2APlayer::~Gothic2APlayer() {
  ClearRemoteLightEffect();
  NetGame::Instance().UnregisterActor(*this);
  this->npc = NULL;
  this->ScriptInstance = NULL;
  delete this->InterPos;
  this->InterPos = NULL;
}

void Gothic2APlayer::AnalyzePosition(zVEC3& Pos) {
  if (!npc || !InterPos) {
    return;
  }

  InterPos->UpdateInterpolation(Pos[VX], Pos[VY], Pos[VZ]);
};

void Gothic2APlayer::AnalyzePose(const zVEC3& position, const zVEC3& heading, std::uint64_t source_time_ms, bool teleport) {
  if (InterPos && npc) {
    InterPos->UpdatePoseInterpolation(position[VX], position[VY], position[VZ], heading[VX], heading[VY], heading[VZ], source_time_ms, teleport);
  }
}

void Gothic2APlayer::QueuePresentation(std::uint64_t source_time_ms, std::function<void()> apply) {
  if (InterPos && npc) InterPos->QueuePresentation(source_time_ms, std::move(apply));
}

double Gothic2APlayer::PresentationTime(double local_time_ms) const {
  return InterPos ? InterPos->PresentationTime(local_time_ms) : local_time_ms;
}

void Gothic2APlayer::DeleteAllPlayers() {
  if (global_ingame && global_ingame->Shrinker) {
    global_ingame->Shrinker->UnShrinkAll();
  }
  auto& players = NetGame::Instance().players;
  while (!players.empty()) {
    Gothic2APlayer* remote_player = players.back();
    players.pop_back();
    if (!remote_player) {
      continue;
    }

    if (!remote_player->IsLocalPlayer()) {
      remote_player->LeaveGame();
    }
    delete remote_player;
  }
  NetGame::Instance().players.clear();
};

void Gothic2APlayer::DisablePlayer() {
  ClearRemoteLightEffect();
  if (base_player_.is_enabled() && npc) {
    StopPositionInterpolation();
    ClearHandledAnimation();
    CloseSpellBook(npc);
    npc->Disable();
    base_player_.set_enabled(false);
  }
}

int Gothic2APlayer::GetHealth() {
  return this->npc ? this->npc->attribute[NPC_ATR_HITPOINTS] : 0;
};

Gothic2APlayer* Gothic2APlayer::GetLocalPlayer() {
  if (NetGame::Instance().players.empty()) {
    return nullptr;
  }

  return NetGame::Instance().players[0];
};

const char* Gothic2APlayer::GetName() {
  return this->npc ? this->npc->GetName().ToChar() : "";
};

int Gothic2APlayer::GetNameLength() {
  return this->npc ? this->npc->GetName().Length() : 0;
};

bool Gothic2APlayer::IsFighting() {
  if (!npc) {
    return false;
  }

  if (npc->GetWeaponMode() > 0)
    return true;
  return false;
};

bool Gothic2APlayer::IsLocalPlayer() {
  return is_local_player_;
}

void Gothic2APlayer::LeaveGame() {
  ClearRemoteLightEffect();
  StopPositionInterpolation();
  if (!npc) {
    ClearHandledAnimation();
    base_player_.set_enabled(false);
    return;
  }

  auto* leaving_npc = npc;
  leaving_npc->AddRef();
  ClearTorchHands(leaving_npc);
  ClearHandledAnimation();
  if (global_ingame && global_ingame->Shrinker && global_ingame->Shrinker->IsShrinked(npc))
    global_ingame->Shrinker->UnShrinkNpc(npc);
  CloseSpellBook(npc);
  ClearNpcFocusReferences(npc);
  if (ogame && ogame->GetSpawnManager())
    ogame->GetSpawnManager()->DeleteNpc(npc);
  else {
    npc->Disable();
  }
  SetNpc(nullptr);
  leaving_npc->Release();
  base_player_.set_enabled(false);
};

void Gothic2APlayer::RespawnPlayer() {
  ClearRemoteLightEffect();
  if (!npc) {
    return;
  }

  StopPositionInterpolation();
  ClearHandledAnimation();
  if (global_ingame && global_ingame->Shrinker) {
    global_ingame->Shrinker->UnShrinkNpc(npc);
  }
  CloseSpellBook(npc);
  auto max_health = base_player_.max_health();
  if (!IsLocalPlayer()) {
    base_player_.set_health(max_health);
    base_player_.set_update_health_packet_counter(0);
    SetHealth(max_health);
    auto player_pos = npc->GetPositionWorld();
    npc->ResetPos(player_pos);
  } else {
    npc->RefreshNpc();
    npc->SetMovLock(0);
    npc->SetWeaponMode(NPC_WEAPON_NONE);
    max_health = base_player_.max_health();
    base_player_.set_health(max_health);
    base_player_.set_update_health_packet_counter(0);
    SetHealth(max_health);
    auto pos = npc->GetPositionWorld();
    npc->ResetPos(pos);
  }
  npc->SetAttribute(NPC_ATR_MANAMAX, base_player_.max_mana());
  npc->SetAttribute(NPC_ATR_MANA, base_player_.mana());
  npc->SetAttribute(NPC_ATR_HITPOINTSMAX, base_player_.max_health());
}

void Gothic2APlayer::SetHealth(int Value) {
  if (!npc) {
    return;
  }

  this->npc->attribute[NPC_ATR_HITPOINTS] = Value;
};

void Gothic2APlayer::SetName(zSTRING& Name) {
  if (!npc) {
    return;
  }

  this->npc->name[0].Clear();
  this->npc->name[0].Insert(0, Name);
};

void Gothic2APlayer::SetName(const char* Name) {
  if (!npc) {
    return;
  }

  this->npc->name[0].Clear();
  this->npc->name[0] = Name;
};

void Gothic2APlayer::SetNameColor(const zCOLOR& color) {
  name_color_ = color;
}

void Gothic2APlayer::StopPositionInterpolation() {
  if (InterPos) {
    InterPos->StopInterpolation();
  }
}

bool Gothic2APlayer::MarkAnimationHandled(const std::string& animation_name) {
  if (animation_name.empty() || animation_name == last_handled_animation_name_) {
    return false;
  }

  last_handled_animation_name_ = animation_name;
  return true;
}

void Gothic2APlayer::ClearHandledAnimation() {
  last_handled_animation_name_.clear();
}

void Gothic2APlayer::SetRemoteLightEffect(oCVisualFX* effect) {
  ClearRemoteLightEffect();
  remote_light_effect_ = effect;
}

void Gothic2APlayer::ClearRemoteLightEffect() {
  auto* effect = remote_light_effect_;
  remote_light_effect_ = nullptr;
  if (effect) {
    effect->Kill();
    effect->Release();
  }
}

void Gothic2APlayer::UpdateMagicPresentation() {
  if (IsLocalPlayer() || !npc)
    return;
  if (remote_light_effect_ && (npc->IsDead() || npc->IsUnconscious() || remote_light_effect_->CanBeDeleted()))
    ClearRemoteLightEffect();
  // Proxy actors do not run the local magic AI. Keep the hand symbol attached
  // to its model node without calling Invest/Spell_Cast or gameplay scripts.
  if (auto* book = npc->GetSpellBook(); book && book->show_handsymbol)
    book->ShowHandSymbol();
}

void Gothic2APlayer::SetNpc(oCNpc* Npc) {
  auto* previous = this->npc;
  if (previous != Npc)
    ClearRemoteLightEffect();
  this->npc = Npc;
  this->ScriptInstance = Npc ? Npc->GetInstance() : 0;
  if (IsLocalPlayer() && global_ingame && global_ingame->Inventory)
    global_ingame->Inventory->BindInventory(Npc ? &Npc->inventory2 : nullptr);
  NetGame::Instance().ActorNpcChanged(*this, previous);
};

void Gothic2APlayer::RememberTransformationInstance(oCNpc* original) {
  transformation_return_instance_ = original ? original->GetInstanceName().ToChar() : "";
  transformation_return_generation_ = base_player_.actor_generation();
}

std::string Gothic2APlayer::GetTransformationReturnInstance() const {
  return transformation_return_generation_ == base_player_.actor_generation() ? transformation_return_instance_ : "";
}

bool Gothic2APlayer::ReplaceNpcInstance(int instance_id) {
  if (!npc || !zfactory || !ogame || !ogame->GetGameWorld() || !IsNpcInstance(Gothic_II_Addon::parser, instance_id))
    return false;

  auto* old_npc = IsLocalPlayer() && Gothic_II_Addon::player ? Gothic_II_Addon::player : npc;
  auto position = old_npc->GetPositionWorld();
  const auto heading = old_npc->GetAtVectorWorld();
  const zSTRING name = old_npc->GetName();
  const bool movement_locked = old_npc->IsMovLock() != 0;
  ScopedNetworkObservationSuppression suppress_observation;
  SPDLOG_INFO("Replacing actor {} instance {} with {} (local={})", base_player_.id(), old_npc->GetInstance(), instance_id, IsLocalPlayer());
  if (auto logger = spdlog::default_logger())
    logger->flush();

  oCNpc* new_npc = nullptr;
  {
    auto* parser = Gothic_II_Addon::parser;
    ScriptInstanceScope scope(*parser, instance_id);
    new_npc = zfactory->CreateNpc(instance_id);
  }
  if (!new_npc)
    return false;

  // Keep the constructor's body, attributes, talents and possessions. An
  // instance change is a new NPC, not a model pasted onto the previous hero.
  new_npc->startAIState = 0;
  new_npc->daily_routine = 0;
  new_npc->state.SetRoutine(nullptr, nullptr);
  if (rtnMan)
    rtnMan->RemoveRoutine(new_npc);
  new_npc->state.hasRoutine = false;
  new_npc->state.ClearAIState();
  ResetWorldRenderState();
  old_npc->AddRef();
  old_npc->CloseInventory();
  old_npc->CloseSteal();
  old_npc->ClearEM();
  CloseSpellBook(old_npc);
  old_npc->SetInteractItem(nullptr);
  ClearTorchHands(old_npc);
  ClearRemoteLightEffect();
  if (global_ingame && global_ingame->Shrinker)
    global_ingame->Shrinker->UnShrinkNpc(old_npc);

  {
    auto* parser = Gothic_II_Addon::parser;
    ScriptInstanceScope scope(*parser, instance_id);
    auto spawn_position = position;
    new_npc->Enable(spawn_position);
  }
  new_npc->state.ClearAIState();
  new_npc->ClearEM();
  SetNpc(new_npc);
  new_npc->SetHeadingYWorld(position + heading);
  new_npc->name[0] = name;
  if (IsLocalPlayer()) {
    SetLocalHero(new_npc);
    ResetLocalActionObservation();
  }
  RemoveNpcFromWorld(old_npc);
  // Enable searches with the old body still present. Reinitialize collision
  // and animation state at the final position after that body has gone.
  if (!new_npc->GetAnictrl())
    new_npc->InitHumanAI();
  const int health = new_npc->GetAttribute(NPC_ATR_HITPOINTS);
  const int mana = new_npc->GetAttribute(NPC_ATR_MANA);
  const int weapon_mode = new_npc->GetWeaponMode();
  new_npc->ResetPos(position);
  new_npc->attribute[NPC_ATR_HITPOINTS] = health;
  new_npc->attribute[NPC_ATR_MANA] = mana;
  new_npc->SetWeaponMode(weapon_mode);
  new_npc->SetHeadingYWorld(position + heading);
  new_npc->showVisual = TRUE;
  new_npc->SetSleeping(FALSE);
  new_npc->SetMovLock(IsLocalPlayer() && movement_locked);
  if (auto* controller = new_npc->GetAnictrl()) {
    controller->InitAnimations();
    controller->SetFightAnis(new_npc->GetWeaponMode());
    controller->SetWalkMode(ANI_WALKMODE_RUN);
    controller->SearchStandAni(TRUE);
  }
  old_npc->Release();
  new_npc->Release();  // World owns the enabled replacement actor.
  ResetWorldRenderState();
  StopPositionInterpolation();
  ClearHandledAnimation();
  SPDLOG_INFO("Actor {} instance replacement completed", base_player_.id());
  if (auto logger = spdlog::default_logger())
    logger->flush();
  return true;
}

void Gothic2APlayer::SetNpcType(NpcType TYPE) {
  if (Type == TYPE)
    return;
  if (!npc || !zfactory || !ogame || !ogame->GetSpawnManager()) {
    return;
  }

  zCParser* parser = Gothic_II_Addon::parser;
  if (!parser) {
    return;
  }

  if (TYPE > NPC_DRACONIAN && Type != NPC_HUMAN) {
    SetNpcType(NPC_HUMAN);
    if (!npc) {
      return;
    }
  }

  char buffer[128];
  zSTRING TypeTemp;
  parser->SetInstance("SELF", npc);

  auto replace_with_instance = [&](const char* instance_name) {
    const int instance_id = parser->GetIndex(instance_name);
    if (instance_id < 0) {
      SPDLOG_WARN("Could not find NPC instance {}", instance_name);
      return false;
    }

    return ReplaceNpcInstance(instance_id);
  };

  bool changed = false;
  switch (TYPE) {
    case NPC_HUMAN: {
      changed = replace_with_instance(PCHERO);
    } break;
    case NPC_ORCWARRIOR: {
      changed = replace_with_instance(ORCWARRIOR);
    } break;
    case NPC_ORCELITE: {
      changed = replace_with_instance(ORCELITE);
    } break;
    case NPC_ORCSHAMAN: {
      changed = replace_with_instance(ORCSHAMAN);
    } break;
    case NPC_UNDEADORC: {
      changed = replace_with_instance(UNDEADORC);
    } break;
    case NPC_SHEEP: {
      changed = replace_with_instance(SHEEP);
    } break;
    case NPC_DRACONIAN: {
      changed = replace_with_instance(DRACONIAN);
    } break;
    case NPC_LESSERSKELETON:
      std::snprintf(buffer, sizeof(buffer), "%s_%s", B_SETVISUALS, LESSER_SKELETON);
      TypeTemp = buffer;
      parser->CallFunc(TypeTemp);
      changed = true;
      break;
    case NPC_SKELETON:
      std::snprintf(buffer, sizeof(buffer), "%s_%s", B_SETVISUALS, SKELETON);
      TypeTemp = buffer;
      parser->CallFunc(TypeTemp);
      changed = true;
      break;
    case NPC_SKELETONMAGE:
      std::snprintf(buffer, sizeof(buffer), "%s_%s", B_SETVISUALS, SKELETON_MAGE);
      TypeTemp = buffer;
      parser->CallFunc(TypeTemp);
      changed = true;
      break;
    case NPC_SKELETONLORD:
      std::snprintf(buffer, sizeof(buffer), "%s_%s", B_SETVISUALS, SKELETON_LORD);
      TypeTemp = buffer;
      parser->CallFunc(TypeTemp);
      changed = true;
      break;
    default:
      break;
  };

  if (changed) {
    Type = TYPE;
  }
};

void Gothic2APlayer::SetPosition(const zVEC3& pos) {
  if (!this->npc) {
    return;
  }

  const int coll_det_stat = this->npc->collDetectionStatic ? 1 : 0;
  const int coll_det_dyn = this->npc->collDetectionDynamic ? 1 : 0;
  this->npc->SetCollDet(0);
  this->npc->SetPositionWorld(pos);
  this->npc->SetCollDetStat(coll_det_stat);
  this->npc->SetCollDetDyn(coll_det_dyn);
};

void Gothic2APlayer::SetPosition(float x, float y, float z) {
  zVEC3 pos(x, y, z);
  SetPosition(pos);
};
