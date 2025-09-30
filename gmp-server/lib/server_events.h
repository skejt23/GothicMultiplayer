
/*
MIT License

Copyright (c) 2023 Gothic Multiplayer Team.

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
#include <optional>
#include <string>

#include <glm/glm.hpp>

inline const std::string kEventOnInitName = "onInit";
inline const std::string kEventOnExitName = "onExit";
inline const std::string kEventOnGameTimeName = "onGameTime";
inline const std::string kEventOnPlayerConnectName = "onPlayerConnect";
inline const std::string kEventOnPlayerDisconnectName = "onPlayerDisconnect";
inline const std::string kEventOnPlayerMessageName = "onPlayerMessage";
inline const std::string kEventOnPlayerCommandName = "onPlayerCommand";
inline const std::string kEventOnPlayerWhisperName = "onPlayerWhisper";
inline const std::string kEventOnPlayerKillName = "onPlayerKill";
inline const std::string kEventOnPlayerDeathName = "onPlayerDeath";
inline const std::string kEventOnPlayerDropItemName = "onPlayerDropItem";
inline const std::string kEventOnPlayerTakeItemName = "onPlayerTakeItem";
inline const std::string kEventOnPlayerCastSpellName = "onPlayerCastSpell";
inline const std::string kEventOnPlayerSpawnName = "onPlayerSpawn";
inline const std::string kEventOnPlayerRespawnName = "onPlayerRespawn";
inline const std::string kEventOnPlayerHitName = "onPlayerHit";

struct OnGameTimeEvent {
  std::uint16_t day;
  std::uint8_t hour;
  std::uint8_t min;
};

struct OnPlayerMessageEvent {
  std::uint64_t pid;
  std::string text;
};

struct OnPlayerCommandEvent {
  std::uint64_t pid;
  std::string command;
};

struct OnPlayerWhisperEvent {
  std::uint64_t from_id;
  std::uint64_t to_id;
  std::string text;
};

struct OnPlayerKillEvent {
  std::uint64_t killer_id;
  std::uint64_t victim_id;
};

struct OnPlayerDeathEvent {
  std::uint64_t player_id;
  std::optional<std::uint64_t> killer_id;
};

struct OnPlayerDropItemEvent {
  std::uint64_t pid;
  std::int16_t item_instance;
  std::int16_t amount;
};

struct OnPlayerTakeItemEvent {
  std::uint64_t pid;
  std::int16_t item_instance;
};

struct OnPlayerCastSpellEvent {
  std::uint64_t caster_id;
  std::uint16_t spell_id;
  std::optional<std::uint64_t> target_id;
};

struct OnPlayerSpawnEvent {
  std::uint64_t player_id;
  glm::vec3 position;
};

struct OnPlayerRespawnEvent {
  std::uint64_t player_id;
  glm::vec3 position;
};

struct OnPlayerHitEvent {
  std::optional<std::uint64_t> attacker_id;
  std::uint64_t victim_id;
  std::int16_t damage;
};