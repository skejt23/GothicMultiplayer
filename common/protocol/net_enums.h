
/*
MIT License

Copyright (c) 2022 Gothic Multiplayer Team.

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

#include <cstdint>

namespace Net {
constexpr std::int16_t EQUIP_SLOT_GENERIC = -1;
constexpr std::int16_t EQUIP_SLOT_ARMOR = -2;
constexpr std::int16_t EQUIP_SLOT_MELEE_WEAPON = -3;
constexpr std::int16_t EQUIP_SLOT_RANGED_WEAPON = -4;
constexpr std::int16_t EQUIP_SLOT_HELMET = -5;
constexpr std::int16_t EQUIP_SLOT_SHIELD = -6;
constexpr std::int16_t EQUIP_SLOT_AMULET = -7;
constexpr std::int16_t EQUIP_SLOT_BELT = -8;
constexpr std::int16_t EQUIP_SLOT_RING_LEFT = -9;
constexpr std::int16_t EQUIP_SLOT_RING_RIGHT = -10;

enum PacketID {
  WL_PREPARE_TO_JOIN,
  WL_JOIN_TO_GAME,
  PT_MSG,
  PT_REQUEST_FILE_LENGTH,
  PT_REQUEST_FILE_PART,
  PT_INITIAL_INFO,
  PT_JOIN_GAME,
  PT_PLAYER_SPAWN,
  PT_PLAYER_NAME_UPDATE,
  PT_PLAYER_INSTANCE_UPDATE,
  PT_PLAYER_COLOR_UPDATE,
  PT_PLAYER_VISUAL_UPDATE,
  PT_PLAYER_FATNESS_UPDATE,
  PT_PLAYER_SCALE_UPDATE,
  PT_PLAYER_OVERLAY_UPDATE,
  PT_PLAYER_ANI_PLAY,
  PT_PLAYER_ANI_STOP,
  PT_PLAYER_FACE_ANI_PLAY,
  PT_PLAYER_FACE_ANI_STOP,
  PT_PLAYER_GESTICULATION,
  PT_PLAYER_SKILL_WEAPON_UPDATE,
  PT_PLAYER_TALENT_UPDATE,
  PT_PLAYER_ATTRIBUTE_UPDATE,
  PT_PLAYER_ATTRIBUTE_SNAPSHOT,
  PT_PLAYER_WORLD_UPDATE,
  PT_ACTUAL_STATISTICS,  // <- chyba tutaj będe musiał dodac optymalizacje gdyż nie potrzeba nam wszystkich informacji o
                         // graczu który jest od nas dalej niż 5000.0f
  PT_EXISTING_PLAYERS,   // Packet contains information about all other players. Send to the new joining player.
  PT_MAP_ONLY,
  PT_EXTENDED_4_SCRIPTS,  // jak juz kiedys wdrozymy skrypty
  PT_LUA_EVENT,
  PT_SRVMSG,
  PT_LEFT_GAME,
  PT_GAME_INFO,
  PT_SKY_SETTINGS,
  PT_DODIE,
  PT_RESPAWN,
  PT_PLAYER_HIT,
  PT_PLAYER_UNCONSCIOUS,
  PT_PLAYER_STANDUP,
  PT_PLAYER_DIED,
  PT_DROPITEM,
  PT_TAKEITEM,
  PT_GIVEITEM,
  PT_EQUIPITEM,
  PT_UNEQUIPITEM,
  PT_REMOVEITEM,
  PT_CASTSPELL,
  PT_CASTSPELLONTARGET,
  PT_VOICE,
  PT_ITEM_GROUND_CREATE,
  PT_ITEM_GROUND_DESTROY,
  PT_ITEM_GROUND_CLEAR,
  PT_PLAYER_WORLD_ENTER,
  PT_PLAYER_PRESENCE,
  PT_ADMIN_AUTH,
  PT_VOICE_CONFIG,
  PT_VOICE_CHANNEL,
  PT_NPC_SPAWN,
  PT_NPC_CONTROL,
  PT_NPC_ACTION_RESULT,
  PT_ITEM_TRANSACTION_RESULT,
  PT_EQUIPMENT_RESULT,
  PT_ACTOR_ACTION,
  PT_NPC_ANIMATION,
  PT_ACTOR_DAMAGE,
  PT_PLAYER_SUMMON,
  PT_PLAYER_TELEPORT,
  PT_PLAYER_TRANSFORMATION,
  PT_PLAYER_INSTANCE_READY,
};

inline const char* PacketIDToString(PacketID id) {
  switch (id) {
    case WL_PREPARE_TO_JOIN:
      return "WL_PREPARE_TO_JOIN";
    case WL_JOIN_TO_GAME:
      return "WL_JOIN_TO_GAME";
    case PT_MSG:
      return "PT_MSG";
    case PT_REQUEST_FILE_LENGTH:
      return "PT_REQUEST_FILE_LENGTH";
    case PT_REQUEST_FILE_PART:
      return "PT_REQUEST_FILE_PART";
    case PT_INITIAL_INFO:
      return "PT_INITIAL_INFO";
    case PT_JOIN_GAME:
      return "PT_JOIN_GAME";
    case PT_PLAYER_SPAWN:
      return "PT_PLAYER_SPAWN";
    case PT_PLAYER_NAME_UPDATE:
      return "PT_PLAYER_NAME_UPDATE";
    case PT_PLAYER_INSTANCE_UPDATE:
      return "PT_PLAYER_INSTANCE_UPDATE";
    case PT_PLAYER_INSTANCE_READY:
      return "PT_PLAYER_INSTANCE_READY";
    case PT_PLAYER_COLOR_UPDATE:
      return "PT_PLAYER_COLOR_UPDATE";
    case PT_PLAYER_VISUAL_UPDATE:
      return "PT_PLAYER_VISUAL_UPDATE";
    case PT_PLAYER_FATNESS_UPDATE:
      return "PT_PLAYER_FATNESS_UPDATE";
    case PT_PLAYER_SCALE_UPDATE:
      return "PT_PLAYER_SCALE_UPDATE";
    case PT_PLAYER_OVERLAY_UPDATE:
      return "PT_PLAYER_OVERLAY_UPDATE";
    case PT_PLAYER_ANI_PLAY:
      return "PT_PLAYER_ANI_PLAY";
    case PT_PLAYER_ANI_STOP:
      return "PT_PLAYER_ANI_STOP";
    case PT_PLAYER_FACE_ANI_PLAY:
      return "PT_PLAYER_FACE_ANI_PLAY";
    case PT_PLAYER_FACE_ANI_STOP:
      return "PT_PLAYER_FACE_ANI_STOP";
    case PT_PLAYER_GESTICULATION:
      return "PT_PLAYER_GESTICULATION";
    case PT_PLAYER_SKILL_WEAPON_UPDATE:
      return "PT_PLAYER_SKILL_WEAPON_UPDATE";
    case PT_PLAYER_TALENT_UPDATE:
      return "PT_PLAYER_TALENT_UPDATE";
    case PT_PLAYER_ATTRIBUTE_UPDATE:
      return "PT_PLAYER_ATTRIBUTE_UPDATE";
    case PT_PLAYER_ATTRIBUTE_SNAPSHOT:
      return "PT_PLAYER_ATTRIBUTE_SNAPSHOT";
    case PT_PLAYER_WORLD_UPDATE:
      return "PT_PLAYER_WORLD_UPDATE";
    case PT_ACTUAL_STATISTICS:
      return "PT_ACTUAL_STATISTICS";
    case PT_EXISTING_PLAYERS:
      return "PT_EXISTING_PLAYERS";
    case PT_MAP_ONLY:
      return "PT_MAP_ONLY";
    case PT_EXTENDED_4_SCRIPTS:
      return "PT_EXTENDED_4_SCRIPTS";
    case PT_LUA_EVENT:
      return "PT_LUA_EVENT";
    case PT_SRVMSG:
      return "PT_SRVMSG";
    case PT_LEFT_GAME:
      return "PT_LEFT_GAME";
    case PT_GAME_INFO:
      return "PT_GAME_INFO";
    case PT_SKY_SETTINGS:
      return "PT_SKY_SETTINGS";
    case PT_DODIE:
      return "PT_DODIE";
    case PT_RESPAWN:
      return "PT_RESPAWN";
    case PT_PLAYER_HIT:
      return "PT_PLAYER_HIT";
    case PT_PLAYER_UNCONSCIOUS:
      return "PT_PLAYER_UNCONSCIOUS";
    case PT_PLAYER_STANDUP:
      return "PT_PLAYER_STANDUP";
    case PT_PLAYER_DIED:
      return "PT_PLAYER_DIED";
    case PT_DROPITEM:
      return "PT_DROPITEM";
    case PT_TAKEITEM:
      return "PT_TAKEITEM";
    case PT_GIVEITEM:
      return "PT_GIVEITEM";
    case PT_EQUIPITEM:
      return "PT_EQUIPITEM";
    case PT_UNEQUIPITEM:
      return "PT_UNEQUIPITEM";
    case PT_REMOVEITEM:
      return "PT_REMOVEITEM";
    case PT_CASTSPELL:
      return "PT_CASTSPELL";
    case PT_CASTSPELLONTARGET:
      return "PT_CASTSPELLONTARGET";
    case PT_VOICE:
      return "PT_VOICE";
    case PT_ITEM_GROUND_CREATE:
      return "PT_ITEM_GROUND_CREATE";
    case PT_ITEM_GROUND_DESTROY:
      return "PT_ITEM_GROUND_DESTROY";
    case PT_ITEM_GROUND_CLEAR:
      return "PT_ITEM_GROUND_CLEAR";
    case PT_PLAYER_WORLD_ENTER:
      return "PT_PLAYER_WORLD_ENTER";
    case PT_PLAYER_PRESENCE:
      return "PT_PLAYER_PRESENCE";
    case PT_ADMIN_AUTH:
      return "PT_ADMIN_AUTH";
    case PT_VOICE_CONFIG:
      return "PT_VOICE_CONFIG";
    case PT_VOICE_CHANNEL:
      return "PT_VOICE_CHANNEL";
    case PT_NPC_SPAWN:
      return "PT_NPC_SPAWN";
    case PT_NPC_CONTROL:
      return "PT_NPC_CONTROL";
    case PT_NPC_ACTION_RESULT:
      return "PT_NPC_ACTION_RESULT";
    case PT_NPC_ANIMATION:
      return "PT_NPC_ANIMATION";
    case PT_ACTOR_ACTION:
      return "PT_ACTOR_ACTION";
    case PT_ACTOR_DAMAGE:
      return "PT_ACTOR_DAMAGE";
    case PT_PLAYER_SUMMON:
      return "PT_PLAYER_SUMMON";
    case PT_PLAYER_TELEPORT:
      return "PT_PLAYER_TELEPORT";
    case PT_PLAYER_TRANSFORMATION:
      return "PT_PLAYER_TRANSFORMATION";
    case PT_ITEM_TRANSACTION_RESULT:
      return "PT_ITEM_TRANSACTION_RESULT";
    case PT_EQUIPMENT_RESULT:
      return "PT_EQUIPMENT_RESULT";
  }
  return "UNKNOWN";
}

}  // namespace Net
