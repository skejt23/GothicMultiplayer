// SPDX-License-Identifier: MIT
#include "npc_animation_utils.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <spdlog/spdlog.h>
#include <string>

#include "animation_sync.h"
#include "hooking/MemoryPatch.h"
#include "weapon_skills.h"

namespace {
constexpr DWORD kStopTurnAnisAddress = 0x006AE530;
bool turn_animation_fix_installed = false;

void __fastcall StopOnlyTurnAnimations(oCAniCtrl_Human* controller, void*) {
  auto* model = controller ? controller->GetModel() : nullptr;
  if (!model)
    return;

  // Gothic II puts dialog gestures on the turn layer too. The native
  // StopTurnAnis fades the entire layer every idle frame, cancelling them.
  const int count = std::clamp(model->numActiveAnis, 0, zMDL_MAX_ANIS_PARALLEL);
  for (int index = count - 1; index >= 0; --index) {
    auto* active = model->aniChannels[index];
    auto* animation = active ? active->protoAni : nullptr;
    if (animation && animation->layer == ANI_TURN_LAYER && gmp::IsPlayerTurningAnimation(animation->GetAniName().ToChar()))
      model->FadeOutAni(active);
  }
}

bool HasOverlay(oCNpc* npc, const std::string& name) {
  for (int index = 0; index < npc->activeOverlays.GetNumInList(); ++index)
    if (_stricmp(npc->activeOverlays[index].ToChar(), name.c_str()) == 0)
      return true;
  return false;
}

bool OverlayExists(const std::string& name) {
  zFILE_VDFS file;
  if (file.Exists(zSTRING(name.c_str())))
    return true;
  const auto compiled = name.substr(0, name.size() - 4) + ".MSB";
  if (file.Exists(zSTRING(compiled.c_str())))
    return true;
  if (!zoptions)
    return false;
  if (file.Exists(zoptions->GetDirString(DIR_COMPILED_ANIMS) + zSTRING(compiled.c_str())))
    return true;
  return file.SearchFile(zSTRING(name.c_str()), zoptions->GetDirString(DIR_ANIMS), TRUE) == 0 && file.Exists();
}
}  // namespace

void InstallNpcAnimationFixes() {
  if (turn_animation_fix_installed)
    return;
  if (CreateHook(kStopTurnAnisAddress, reinterpret_cast<DWORD>(&StopOnlyTurnAnimations)))
    turn_animation_fix_installed = true;
  else
    SPDLOG_ERROR("Failed to fix native StopTurnAnis at 0x{:08X}", kStopTurnAnisAddress);
}

void ApplyNpcTalent(oCNpc* npc, int talent, int value) {
  if (!npc || talent < 1 || talent >= oCNpcTalent::NPC_TAL_MAX)
    return;
  if (!gmp::IsWeaponSkill(talent)) {
    if (npc->GetTalentSkill(talent) != value)
      npc->SetTalentSkill(talent, value);
    return;
  }

  const int tier = std::clamp(value, 0, 2);
  auto* model = npc->GetModel();
  auto* base = model && model->modelProtoList.GetNumInList() ? model->modelProtoList[0] : nullptr;
  if (!base)
    return;
  constexpr std::array<const char*, 5> prefixes{"", "1HS", "2HS", "BOW", "CBOW"};
  const std::string prefix = std::string(base->modelProtoName.ToChar()) + "_" + prefixes[talent] + "T";
  bool removed = false;
  for (int previous = 1; previous <= 2; ++previous) {
    const auto overlay = prefix + std::to_string(previous) + ".MDS";
    if (previous != tier && HasOverlay(npc, overlay)) {
      npc->RemoveOverlay(zSTRING(overlay.c_str()));
      removed = true;
    }
  }
  const auto desired = prefix + std::to_string(tier) + ".MDS";
  const bool applied = tier == 0 || HasOverlay(npc, desired);
  constexpr std::array<const char*, 5> run_animations{"", "S_1HRUN", "S_2HRUN", "S_BOWRUN", "S_CBOWRUN"};
  const bool has_weapon_animations = model->GetAniIDFromAniName(zSTRING(run_animations[talent])) >= 0;
  const bool available = tier == 0 || applied || (has_weapon_animations && OverlayExists(desired));
  if (npc->GetTalentSkill(talent) == tier && (applied || !available) && !removed)
    return;

  // Native SetTalentSkill only applies weapon overlays when learning. Reset
  // first so downgrades also select their new tier instead of retaining T2.
  npc->SetTalentSkill(talent, 0);
  if (tier && available) {
    npc->SetTalentSkill(talent, tier);
  } else if (talent < npc->talents.GetNumInList() && npc->talents[talent]) {
    // Creature models may have no weapon-training overlays. Preserve the stat
    // without asking the engine to load a missing MDS and terminate the game.
    npc->talents[talent]->m_skill = tier;
  }
  if (auto* controller = npc->GetAnictrl()) {
    constexpr std::array<int, 5> modes{NPC_WEAPON_NONE, NPC_WEAPON_1HS, NPC_WEAPON_2HS, NPC_WEAPON_BOW, NPC_WEAPON_CBOW};
    controller->InitFightAnis(modes[talent]);
    controller->SetFightAnis(npc->GetWeaponMode());
  }
}

bool ApplyNpcWeaponSkill(oCNpc* npc, int skill, int percentage) {
  if (!npc || !gmp::IsWeaponSkill(skill))
    return false;
  const int clamped = std::clamp(percentage, 0, 100);
  npc->SetHitChance(skill, clamped);
  ApplyNpcTalent(npc, skill, gmp::WeaponSkillTier(clamped));
  return true;
}
