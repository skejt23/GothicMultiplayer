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

#include "game_server.h"

#include <bitsery/ext/value_range.h>
#include <bitsery/traits/vector.h>
#include <httplib.h>
#include <opus/opus.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <version.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <iterator>
#include <limits>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <stack>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "Lua/event_bind.h"
#include "Shared/lua_runtime/lua_value_codec.h"
#include "default_character_appearance.h"
#include "diagnostics_manager.h"
#include "gothic_clock.h"
#include "healing_spells.h"
#include "inventory_sync.h"
#include "script_names.h"
#include "master_server_client.h"
#include "native_event_packets.h"
#include "net_enums.h"
#include "npc_packets.h"
#include "packets.h"
#include "platform_depend.h"
#include "server_constants.h"
#include "server_events.h"
#include "shared/event.h"
#include "shared/math.h"
#include "sol/sol.hpp"
#include "transformation_spells.h"
#include "transport/transport.h"
#include "weapon_skills.h"

// Non-owning access for serialization helpers; GameServer owns the endpoint.
static Net::ServerTransport* g_transport = nullptr;

std::atomic<bool> g_is_server_running = true;

using namespace Net;

namespace {

constexpr std::size_t kMaxWorldNameLength = 32;
constexpr std::size_t kMaxPlayerNameLength = 64;
constexpr std::uint32_t kMaxVoicePacketsPerSecond = 75;
constexpr int kVoiceSampleRate = 48000;
constexpr int kVoiceFrameSamples = 960;
constexpr auto kVoiceActivityTimeout = std::chrono::milliseconds(400);
constexpr const char* kDiagnosticsFileName = "diagnostic.txt";
constexpr const char* kPublicDataPath = "data/public";
// Hard upper bound for one server's advertised client content transfer.
constexpr std::uint64_t kMaximumAdvertisedDownloadSize = 4ULL * 1024ULL * 1024ULL * 1024ULL;
constexpr std::string_view kFrame = "-========================================-";
constexpr auto kPlayerPresenceUpdateInterval = std::chrono::milliseconds(1000);
constexpr std::uint8_t kFullSkySettingsFlags = SKY_SETTING_WEATHER | SKY_SETTING_RAIN_START | SKY_SETTING_RAIN_STOP |
                                                SKY_SETTING_WIND_SCALE | SKY_SETTING_DONT_RAIN | SKY_SETTING_RAIN_WEIGHT |
                                                SKY_SETTING_LIGHTNING;

bool IsValidVoiceChannelName(std::string_view channel) {
  if (channel.empty() || channel.size() > kMaxVoiceChannelNameLength) {
    return false;
  }
  return std::all_of(channel.begin(), channel.end(), [](unsigned char character) {
    return character >= 0x20 && character != 0x7f;
  });
}

bool IsLiveCharacter(const Character& actor) {
  return actor.is_ingame && actor.tod == 0 && actor.health > 0 && actor.state.life_state == PLAYER_LIFE_ALIVE;
}

bool SameCharacterScope(const Character& a, const Character& b) {
  return a.world == b.world && a.virtual_world == b.virtual_world;
}

bool AcceptActionId(std::optional<std::uint32_t>& last, std::uint32_t incoming) {
  if (incoming == 0 || (last && !IsNewActorSequence(incoming, *last))) return false;
  last = incoming;
  return true;
}

void ReconcileWeaponModeHands(Character& character) {
  auto& state = character.state;
  for (auto* hand : {&state.left_hand_item_instance, &state.right_hand_item_instance}) {
    if (*hand == state.melee_weapon_instance || *hand == state.ranged_weapon_instance) hand->clear();
  }
  const auto& drawn = state.weapon_mode >= 5 ? state.ranged_weapon_instance : state.melee_weapon_instance;
  if (state.weapon_mode >= 2 && state.weapon_mode <= 6 && character.InventoryCount(drawn) > 0)
    (state.weapon_mode == 5 ? state.left_hand_item_instance : state.right_hand_item_instance) = drawn;
  if (state.weapon_mode != 7) {
    state.active_spell_instance.clear();
    state.active_spell_nr = state.active_spell_level = 0;
    state.spell_investing = false;
  }
}

struct CombatBounds {
  std::int32_t damage{0};
  std::uint32_t types{0};
  std::uint32_t reach{0};
  std::uint32_t interval_ms{250};
};

CombatBounds GetCombatBounds(const Character& actor) {
  if (actor.transformation) {
    const auto* form = gmp::GetTransformationForm(actor.transformation->spell_id);
    return {std::clamp(actor.strength, 5, 1000000), form ? form->damage_type : 0u, 300, 250};
  }
  if (actor.state.weapon_mode <= 1)
    return {std::clamp(actor.strength, 5, 1000000), 255u, 300, 250};
  const bool ranged = actor.state.weapon_mode == 5 || actor.state.weapon_mode == 6;
  const auto& weapon = ranged ? actor.state.ranged_weapon_instance : actor.state.melee_weapon_instance;
  if (actor.state.weapon_mode == 7 || actor.InventoryCount(weapon) == 0) return {};
  // Native damage and protections depend on the controlling client's scripts.
  // Keep ownership, action cadence and spatial bounds without a shared catalog.
  return {1000000, 255u, ranged ? 10000u : 500u, ranged ? 500u : 200u};
}

bool CanFinishCharacter(const Character& attacker, const Character& victim) {
  return !attacker.transformation && attacker.state.weapon_mode >= 2 && attacker.state.weapon_mode <= 4 && victim.health > 0 &&
         victim.state.life_state == PLAYER_LIFE_UNCONSCIOUS && (victim.flags & PlayerManager::PL_UNCONCIOUS) != 0 &&
         attacker.InventoryCount(attacker.state.melee_weapon_instance) > 0 &&
         glm::distance(attacker.state.position, victim.state.position) <= 500.0f;
}

bool SelectConfiguredResources(const ResourceManager& resource_manager, const std::vector<std::string>& resource_names,
                               std::vector<ResourceManager::DiscoveredResource>& selected_resources) {
  const auto& discovered_resources = resource_manager.GetDiscoveredResourceInfo();
  selected_resources.clear();
  selected_resources.reserve(resource_names.size());

  for (const auto& resource_name : resource_names) {
    auto it = std::find_if(discovered_resources.begin(), discovered_resources.end(),
                           [&resource_name](const ResourceManager::DiscoveredResource& resource) { return resource.name == resource_name; });
    if (it == discovered_resources.end()) {
      SPDLOG_ERROR("Configured resource '{}' was not found in resources/", resource_name);
      return false;
    }

    if (!it->metadata) {
      SPDLOG_ERROR("Configured resource '{}' has invalid metadata", resource_name);
      return false;
    }

    selected_resources.push_back(*it);
  }

  return true;
}

SkySettingsPacket MakeSkySettingsPacket(std::uint8_t flags, std::int32_t weather_type, std::int32_t rain_start_hour,
                                        std::int32_t rain_start_min, std::int32_t rain_stop_hour, std::int32_t rain_stop_min,
                                        float wind_scale, bool dont_rain, float rain_weight, bool render_lightning) {
  SkySettingsPacket packet{};
  packet.packet_type = PT_SKY_SETTINGS;
  packet.flags = flags;
  packet.weather_type = weather_type;
  packet.rain_start_hour = static_cast<std::int16_t>(rain_start_hour);
  packet.rain_start_min = static_cast<std::int16_t>(rain_start_min);
  packet.rain_stop_hour = static_cast<std::int16_t>(rain_stop_hour);
  packet.rain_stop_min = static_cast<std::int16_t>(rain_stop_min);
  packet.wind_scale = wind_scale;
  packet.dont_rain = dont_rain ? 1 : 0;
  packet.rain_weight = std::clamp(rain_weight, 0.0f, 1.0f);
  packet.render_lightning = render_lightning ? 1 : 0;
  return packet;
}

std::string FormatConnectionDetails(std::string address, const std::string& identity) {
  if (!address.empty()) {
    address.append(", ");
  }
  address.append(identity.empty() ? "<identity unavailable>" : identity);
  return address;
}

std::string FormatConnectionDetails(ConnectionHandle connection) {
  if (!g_transport) {
    return FormatConnectionDetails({}, {});
  }
  return FormatConnectionDetails(g_transport->GetPlayerIp(connection), g_transport->GetPlayerIdentity(connection));
}

std::string FormatPlayerLabel(const PlayerManager::Player& player) {
  const auto connection_details = FormatConnectionDetails(player.ip_address, player.identity);
  if (player.name.empty()) {
    return connection_details;
  }
  std::string label = player.name;
  label.append(" (").append(connection_details).append(")");
  return label;
}

bool IsCommand(std::string_view command, std::string_view expected) {
  return command.size() == expected.size() && std::equal(command.begin(), command.end(), expected.begin(), expected.end(), [](char a, char b) {
           return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
         });
}

std::string_view TrimLeft(std::string_view text) {
  const auto first = text.find_first_not_of(' ');
  if (first == std::string_view::npos) {
    return {};
  }
  text.remove_prefix(first);
  return text;
}

std::pair<std::string_view, std::string_view> SplitCommandArgs(std::string_view text) {
  text = TrimLeft(text);
  const auto space_pos = text.find(' ');
  if (space_pos == std::string_view::npos) {
    return {text, {}};
  }

  auto args = text.substr(space_pos + 1);
  return {text.substr(0, space_pos), TrimLeft(args)};
}

#ifdef MASTER_SERVER_ENDPOINT
constexpr std::string_view kMasterServerEndpoint = MASTER_SERVER_ENDPOINT;
#else
constexpr std::string_view kMasterServerEndpoint{};
#endif

std::string FormatCurrentDateTime() {
  auto now = std::time(nullptr);
  std::tm local_tm{};
#ifdef _WIN32
  localtime_s(&local_tm, &now);
#else
  localtime_r(&now, &local_tm);
#endif
  std::ostringstream oss;
  oss << std::put_time(&local_tm, "%H:%M:%S %d/%m/%Y");
  return oss.str();
}

std::uint8_t GetPlayerLifeState(const Character& player) {
  if (player.tod != 0) {
    return PLAYER_LIFE_DEAD;
  }
  if ((player.flags & PlayerManager::PL_UNCONCIOUS) != 0) {
    return PLAYER_LIFE_UNCONSCIOUS;
  }
  return PLAYER_LIFE_ALIVE;
}

void ClearTransientCombatState(Character& player) {
  player.approved_projectiles.clear();
  player.state.left_hand_item_instance.clear();
  player.state.right_hand_item_instance.clear();
  player.state.animation_name.clear();
  player.state.weapon_mode = 0;
  player.state.active_spell_nr = 0;
  player.state.active_spell_instance.clear();
  player.state.active_spell_level = 0;
  player.state.spell_investing = false;
}

void ClearRecoveredPose(Character& player) {
  ClearTransientCombatState(player);
  player.state.body_state &= ~127u; // BS_STAND, preserving body-state modifiers.
}

void PopulatePlayerSpawnSnapshot(PlayerSpawnPacket& packet, const Character& player) {
  packet.instance = player.instance;
  packet.name_color_r = player.name_color_r;
  packet.name_color_g = player.name_color_g;
  packet.name_color_b = player.name_color_b;

  packet.strength = player.strength;
  packet.dexterity = player.dexterity;
  packet.level = player.level;
  packet.exp = player.exp;
  packet.next_level_exp = player.next_level_exp;
  packet.learn_points = player.learn_points;
  packet.health = player.health;
  packet.max_health = player.max_health;
  packet.mana = player.mana;
  packet.max_mana = player.max_mana;
  packet.life_state = GetPlayerLifeState(player);

  packet.fatness = player.fatness;
  packet.scale = player.scale;

  packet.weapon_skills.clear();
  packet.weapon_skills.reserve(player.weapon_skills.size());
  for (const auto& [skill_id, percentage] : player.weapon_skills) {
    PlayerSpawnPacket::SkillEntry entry;
    entry.skill_id = skill_id;
    entry.percentage = percentage;
    packet.weapon_skills.push_back(std::move(entry));
  }

  packet.talents.clear();
  packet.talents.reserve(player.talents.size());
  for (const auto& [talent_id, value] : player.talents) {
    PlayerSpawnPacket::TalentEntry entry;
    entry.talent_id = talent_id;
    entry.value = value;
    packet.talents.push_back(std::move(entry));
  }

  packet.overlays = player.overlays;
}

std::uint32_t AdvancePlayerStateSequence(Character& player) {
  ++player.state_sequence;
  if (player.state_sequence == 0) {
    ++player.state_sequence;
  }
  return player.state_sequence;
}

std::uint32_t AdvanceActorCommand(Character& actor) {
  if (++actor.command_revision == 0) ++actor.command_revision;
  return actor.command_revision;
}

void RestoreTransformation(Character& actor) {
  if (!actor.transformation)
    return;
  const auto human = std::move(*actor.transformation);
  actor.transformation.reset();
  // Preserve the fractional HP lost by integer conversion into a small form.
  // Repeated sheep/rat transforms must neither heal nor erode the human's HP.
  const auto form_health = gmp::ScaleTransformationHealth(actor.health, actor.max_health, human.form_max_health);
  const auto delta = static_cast<std::int64_t>(form_health - human.form_health) * human.max_health;
  const auto scaled_delta = delta < 0 ? -((-delta + human.form_max_health - 1) / human.form_max_health) : delta / human.form_max_health;
  actor.health =
      actor.health <= 0
          ? 0
          : static_cast<std::int32_t>(std::clamp<std::int64_t>(static_cast<std::int64_t>(human.health) + scaled_delta, 1, human.max_health));
  actor.max_health = human.max_health;
  if (actor.state.life_state == PLAYER_LIFE_DEAD)
    actor.health = 0;
  else if (actor.state.life_state == PLAYER_LIFE_UNCONSCIOUS)
    actor.health = 1;
  actor.max_mana = human.max_mana;
  actor.mana = std::clamp(human.mana, 0, actor.max_mana);
  actor.strength = human.strength;
  actor.dexterity = human.dexterity;
  actor.instance = human.instance;
  const auto current = actor.state;
  actor.state = human.equipment;
  actor.state.position = current.position;
  actor.state.nrot = current.nrot;
  actor.state.source_time_ms = current.source_time_ms;
  actor.state.life_state = current.life_state;
  actor.state.invulnerable = current.invulnerable;
  actor.state.health_points = actor.health;
  actor.state.mana_points = actor.mana;
  actor.state.animation_name.clear();
  actor.state.body_state = 0;
  actor.state.active_spell_nr = actor.state.active_spell_level = 0;
  actor.state.active_spell_instance.clear();
  actor.state.spell_investing = false;
  actor.state.weapon_mode = 0;
  actor.approved_projectiles.clear();
}

void EnterTransformation(Character& actor, std::uint16_t spell, std::uint32_t cast_id) {
  const auto* form = gmp::GetTransformationForm(spell);
  actor.transformation = Character::Transformation{spell,          cast_id,    actor.instance,   actor.max_health,
                                                   actor.max_mana, actor.mana, actor.strength,   actor.dexterity,
                                                   actor.health,   0,          form->max_health, actor.state};
  actor.health = gmp::ScaleTransformationHealth(actor.health, actor.max_health, form->max_health);
  actor.transformation->form_health = actor.health;
  actor.max_health = form->max_health;
  actor.max_mana = actor.mana = 0;
  actor.strength = form->strength;
  actor.dexterity = form->dexterity;
  actor.instance = form->instance;
  const auto old = actor.state;
  actor.state = PlayerState{};
  actor.state.weapon_mode = gmp::kTransformationWeaponMode;
  actor.state.position = old.position;
  actor.state.nrot = old.nrot;
  actor.state.source_time_ms = ActorClockMilliseconds();
  actor.state.health_points = actor.health;
  actor.state.life_state = old.life_state;
  actor.state.invulnerable = old.invulnerable;
  actor.approved_projectiles.clear();
}

PlayerTransformationPacket MakeTransformationPacket(const Character& actor, std::uint32_t cast_id) {
  PlayerTransformationPacket packet;
  packet.player_id = actor.player_id;
  packet.actor_generation = actor.actor_generation;
  packet.command_revision = actor.command_revision;
  packet.state_sequence = actor.state_sequence;
  packet.cast_id = cast_id;
  packet.spell_id = actor.transformation ? actor.transformation->spell_id : 0;
  packet.instance = actor.instance;
  packet.max_health = actor.max_health;
  packet.max_mana = actor.max_mana;
  packet.strength = actor.strength;
  packet.dexterity = actor.dexterity;
  packet.state = actor.state;
  packet.state.health_points = actor.health;
  packet.state.mana_points = actor.mana;
  packet.state.invulnerable = actor.invulnerable;
  packet.state.correction_mask = kSyncAll;
  return packet;
}

PlayerStateUpdatePacket MakePlayerStateUpdatePacket(const Character& player) {
  PlayerStateUpdatePacket packet{};
  packet.packet_type = PT_ACTUAL_STATISTICS;
  packet.player_id = player.player_id;
  packet.state_sequence = player.state_sequence;
  packet.actor_generation = player.actor_generation;
  packet.command_revision = player.command_revision;
  packet.state = player.state;
  packet.state.correction_mask = kSyncAll;
  packet.state.health_points = player.health;
  packet.state.mana_points = player.mana;
  packet.state.life_state = GetPlayerLifeState(player);
  packet.state.invulnerable = player.invulnerable;
  return packet;
}

PlayerSpawnPacket MakePlayerSpawnPacket(const Character& player) {
  PlayerSpawnPacket packet{};
  packet.instance_change_revision = player.instance_change ? player.instance_change->request_revision : 0;
  if (player.transformation) {
    packet.transformation_spell_id = player.transformation->spell_id;
    packet.transformation_cast_id = player.transformation->cast_id;
  }
  packet.packet_type = PT_PLAYER_SPAWN;
  packet.player_id = player.player_id;
  packet.state_sequence = player.state_sequence;
  packet.actor_generation = player.actor_generation;
  packet.command_revision = player.command_revision;
  packet.source_time_ms = player.state.source_time_ms;
  packet.invulnerable = player.invulnerable;
  packet.weapon_mode = player.state.weapon_mode;
  packet.active_spell_nr = player.state.active_spell_nr;
  packet.active_spell_instance = player.state.active_spell_instance;
  packet.active_spell_level = player.state.active_spell_level;
  packet.spell_investing = player.state.spell_investing;
  packet.head_direction = player.state.head_direction;
  packet.melee_weapon_instance = player.state.melee_weapon_instance;
  packet.ranged_weapon_instance = player.state.ranged_weapon_instance;
  packet.body_state = player.state.body_state;
  packet.walk_mode = player.state.walk_mode;
  packet.look_yaw = player.state.look_yaw;
  packet.look_pitch = player.state.look_pitch;
  packet.focus_target_id = player.state.focus_target_id;
  packet.player_name = player.name;
  packet.position = player.state.position;
  packet.normal = player.state.nrot;
  packet.left_hand_item_instance = player.state.left_hand_item_instance;
  packet.right_hand_item_instance = player.state.right_hand_item_instance;
  packet.equipped_armor_instance = player.state.equipped_armor_instance;
  packet.equipped_helmet_instance = player.state.equipped_helmet_instance;
  packet.equipped_shield_instance = player.state.equipped_shield_instance;
  packet.equipped_amulet_instance = player.state.equipped_amulet_instance;
  packet.equipped_belt_instance = player.state.equipped_belt_instance;
  packet.equipped_ring_left_instance = player.state.equipped_ring_left_instance;
  packet.equipped_ring_right_instance = player.state.equipped_ring_right_instance;
  packet.animation_name = player.state.animation_name;
  packet.body_model = player.body_model;
  packet.body_texture = player.body_texture;
  packet.head_model = player.head_model;
  packet.head_texture = player.head_texture;
  packet.teeth_texture = player.teeth_texture;
  packet.skin_color = player.skin_color;
  packet.walk_style = player.walkstyle;
  PopulatePlayerSpawnSnapshot(packet, player);
  for (const auto& [instance, amount] : player.inventory) {
    if (amount > 0) packet.inventory.push_back({instance, amount});
  }
  std::sort(packet.inventory.begin(), packet.inventory.end(),
      [](const InventoryEntry& left, const InventoryEntry& right) { return left.item_instance < right.item_instance; });
  return packet;
}

bool IsSameVisibilityScope(const Character& a, const Character& b) {
  return a.world == b.world && a.virtual_world == b.virtual_world;
}

NpcControlPacket MakeNpcControlPacket(const NpcManager::Npc& npc) {
  NpcControlPacket packet;
  packet.npc_id = npc.player_id;
  packet.actor_generation = npc.actor_generation;
  packet.source_time_ms = npc.state.source_time_ms;
  packet.control_epoch = npc.control_epoch;
  packet.host_player_id = npc.host_player_id;
  packet.state_sequence = npc.state_sequence;
  packet.position = npc.state.position;
  packet.normal = npc.state.nrot;
  if (npc.host_player_id != 0 && !npc.actions.empty()) {
    const auto& action = npc.actions.front();
    const auto now = NpcManager::Clock::now();
    const auto elapsed = action.started_at
        ? std::chrono::duration_cast<std::chrono::milliseconds>(now - *action.started_at).count() : 0;
    const auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - action.queued_at).count();
    packet.action_id = action.id;
    packet.elapsed_ms = static_cast<std::uint32_t>(std::max<std::int64_t>(0, elapsed));
    packet.animation = action.animation;
    packet.timeout_ms = static_cast<std::uint32_t>(std::max<std::int64_t>(1,
        std::min<std::int64_t>(static_cast<std::int64_t>(action.timeout_ms) - elapsed,
                              static_cast<std::int64_t>(NpcManager::kMaxActionLifetimeMs) - age)));
  }
  return packet;
}

NpcAnimationPacket MakeNpcAnimationPacket(const NpcManager::Npc& npc) {
  NpcAnimationPacket packet;
  packet.npc_id = npc.player_id;
  packet.actor_generation = npc.actor_generation;
  packet.source_time_ms = npc.animation_started_at_ms;
  packet.revision = npc.animation_revision;
  packet.animation = npc.animation;
  return packet;
}

bool IsFinitePosition(const glm::vec3& value) {
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool IsValidActorPosition(const glm::vec3& value) {
  constexpr float limit = 10000000.0f;
  return IsFinitePosition(value) && std::abs(value.x) <= limit && std::abs(value.y) <= limit && std::abs(value.z) <= limit;
}

bool IsInsideStreamRange(const Character& viewer, const Character& subject, float radius, float height) {
  if (!IsSameVisibilityScope(viewer, subject)) {
    return false;
  }

  const auto delta = subject.state.position - viewer.state.position;
  if (height > 0.0f && std::abs(delta.y) > height) {
    return false;
  }

  const auto radius_squared = radius * radius;
  return delta.x * delta.x + delta.z * delta.z <= radius_squared;
}

std::string SanitizeWorldName(std::string world) {
  if (world.size() > kMaxWorldNameLength) {
    SPDLOG_WARN("World name '{}' is longer than {} characters and will be truncated", world, kMaxWorldNameLength);
    world.resize(kMaxWorldNameLength);
  }

  return world;
}

std::string SanitizeServerText(std::string text) {
  for (std::size_t i = 0; i < text.size(); ++i) {
    unsigned char ch = static_cast<unsigned char>(text[i]);
    if (ch < 0x20 && ch != 0x07) {
      text.resize(i);
      break;
    }
  }
  return text;
}

std::string SanitizePlayerName(std::string name) {
  name = SanitizeServerText(std::move(name));
  if (name.size() > kMaxPlayerNameLength) {
    SPDLOG_WARN("Player name '{}' is longer than {} characters and will be truncated", name, kMaxPlayerNameLength);
    name.resize(kMaxPlayerNameLength);
  }
  return name;
}

std::string SanitizePlayerAnimationName(std::string name) {
  return gmp::IsValidAnimationName(name) ? gmp::NormalizeScriptName(name) : std::string{};
}

MessagePacket CreateMessagePacket(std::optional<std::uint32_t> sender_id, std::optional<std::uint32_t> recipient_id, std::uint8_t r, std::uint8_t g,
                                  std::uint8_t b, std::string text, std::uint8_t packet_type = PT_MSG) {
  MessagePacket packet{};
  packet.packet_type = packet_type;
  packet.message = SanitizeServerText(std::move(text));
  packet.r = r;
  packet.g = g;
  packet.b = b;
  packet.sender = sender_id;
  packet.recipient = recipient_id;
  return packet;
}

void LogServerBanner() {
  SPDLOG_INFO(kFrame);
  SPDLOG_INFO("-= Gothic Multiplayer Dedicated Server");
  SPDLOG_INFO(kFrame);

  constexpr std::string_view git_tag_long = GIT_TAG_LONG;
  if (!git_tag_long.empty()) {
    SPDLOG_INFO("-= Version: {}", git_tag_long);
  } else {
    SPDLOG_INFO("-= Version: Development build");
  }

  constexpr std::string_view git_commit = GIT_COMMIT_LONG;
  if (!git_commit.empty()) {
    SPDLOG_INFO("-= Commit: {}", git_commit);
  }

  SPDLOG_INFO("-= Build date: {} {}", __DATE__, __TIME__);
  SPDLOG_INFO("-= GMP Team 2011-2025");
}

template <typename Packet, typename TContainer = std::vector<std::uint8_t>>
bool SerializeAndSend(const Packet& packet, Net::TrafficClass traffic, Net::ConnectionHandle connection) {
  TContainer buffer;
  const auto written_size = bitsery::quickSerialization<bitsery::OutputBufferAdapter<TContainer>>(buffer, packet);
  if (!g_transport || written_size > std::numeric_limits<std::uint32_t>::max() ||
      !g_transport->Send(buffer.data(), static_cast<std::uint32_t>(written_size), traffic, connection)) {
    DiagnosticsManager::Instance().RecordSendFailure(packet.packet_type);
    if (traffic == TrafficClass::Control) {
      SPDLOG_WARN("Failed to enqueue reliable gameplay packet {} ({}) for connection {} ({} bytes)", static_cast<int>(packet.packet_type),
                  PacketIDToString(static_cast<PacketID>(packet.packet_type)), connection, written_size);
      if constexpr (std::is_same_v<Packet, LuaEventPacket>) {
        SPDLOG_WARN("Failed Lua event '{}': source {}, payload {} bytes", packet.event_name, packet.source_element, packet.payload.size());
      }
      if (g_transport) {
        const auto stats = g_transport->GetNetworkStats(connection);
        if (stats.available) {
          SPDLOG_WARN("Connection {} send queue: reliable {} bytes, unreliable {} bytes, unacknowledged {} bytes, control delay {} us", connection,
                      stats.pending_reliable_bytes, stats.pending_unreliable_bytes, stats.unacknowledged_reliable_bytes, stats.control_queue_time_usec);
        }
        const auto diagnostics = g_transport->GetDiagnostics(connection);
        if (!diagnostics.empty()) {
          SPDLOG_WARN("Connection {} transport diagnostics: {}", connection, diagnostics);
        }
        // A missing reliable update leaves the client inconsistent. Close events
        // are deferred until Pulse so callers may safely finish player iteration.
        g_transport->CloseConnection(connection, CloseReason::Failure, "Unable to enqueue reliable gameplay data");
      }
    }
    return false;
  }
  DiagnosticsManager::Instance().RecordOutgoingPacket(packet.packet_type, static_cast<std::uint32_t>(written_size));
  return true;
}

template <typename PacketT>
bool DeserializeClientPacket(const ::Packet& source, PacketT& packet, std::uint8_t packet_identifier, const char* packet_name) {
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({source.data, source.length}, packet);
  if (!state.second) {
    DiagnosticsManager::Instance().RecordPacketRejected(packet_identifier);
    SPDLOG_WARN("Failed to deserialize {}", packet_name);
    return false;
  }

  return true;
}

template <typename Packet>
void BroadcastToRelevant(PlayerManager& player_manager, const Character& subject, const Packet& packet, Net::TrafficClass traffic) {
  const auto connection = player_manager.GetConnectionHandle(subject.player_id);
  // Character mutations and stream-in/out share reliable Control ordering. An
  // old attribute update must not overtake a snapshot after re-entering range.
  if (subject.is_ingame && connection) {
    SerializeAndSend(packet, traffic, *connection);
  }

  for (const auto& viewer_id : subject.streamed_by_players) {
    auto viewer_opt = player_manager.GetPlayer(viewer_id);
    if (!viewer_opt.has_value()) {
      continue;
    }
    const auto& viewer = viewer_opt->get();
    if (!viewer.is_ingame) {
      continue;
    }
    if (viewer.world != subject.world || viewer.virtual_world != subject.virtual_world) {
      continue;
    }
    SerializeAndSend(packet, traffic, viewer.connection);
  }
}

void BroadcastPlayerStateToViewers(PlayerManager& player_manager, const Character& subject) {
  if (!subject.is_ingame) {
    return;
  }

  const auto packet = MakePlayerStateUpdatePacket(subject);
  for (const auto viewer_id : subject.streamed_by_players) {
    auto viewer_opt = player_manager.GetPlayer(viewer_id);
    if (!viewer_opt.has_value()) {
      continue;
    }

    const auto& viewer = viewer_opt->get();
    if (!viewer.is_ingame || !IsSameVisibilityScope(viewer, subject)) {
      continue;
    }

    SerializeAndSend(packet, TrafficClass::State, viewer.connection);
  }
}

void StreamOutSubjectFromViewer(Character& subject, PlayerManager::Player& viewer) {
  if (subject.streamed_by_players.erase(viewer.player_id) == 0) {
    return;
  }

  viewer.spawned_players.erase(subject.player_id);
  viewer.sent_actor_states.erase(subject.player_id);

  DisconnectionInfoPacket packet{};
  packet.packet_type = PT_LEFT_GAME;
  packet.disconnected_id = subject.player_id;
  SerializeAndSend(packet, TrafficClass::Control, viewer.connection);
}

void StreamOutAllKnownPlayers(PlayerManager& player_manager, PlayerManager::Player& player) {
  const std::vector<PlayerManager::PlayerId> viewers(player.streamed_by_players.begin(), player.streamed_by_players.end());
  for (const auto viewer_id : viewers) {
    auto viewer_opt = player_manager.GetPlayer(viewer_id);
    if (!viewer_opt.has_value()) {
      player.streamed_by_players.erase(viewer_id);
      continue;
    }

    StreamOutSubjectFromViewer(player, viewer_opt->get());
  }

  const std::vector<PlayerManager::PlayerId> spawned_players(player.spawned_players.begin(), player.spawned_players.end());
  for (const auto spawned_id : spawned_players) {
    auto spawned_opt = player_manager.GetPlayer(spawned_id);
    if (!spawned_opt.has_value()) {
      player.spawned_players.erase(spawned_id);
      continue;
    }

    StreamOutSubjectFromViewer(spawned_opt->get(), player);
  }

  player.spawned_players.clear();
  player.streamed_by_players.clear();
}

bool CanSeeItemGround(const PlayerManager::Player& player, const ItemGroundManager::ItemGround& item_ground) {
  return player.is_ingame && player.world == item_ground.world && player.virtual_world == item_ground.virtual_world;
}

bool AddInventoryItem(Character& player, const std::string& instance, std::int32_t amount) {
  return player.AddInventory(instance, amount);
}

bool RemoveInventoryItem(Character& player, const std::string& instance, std::int32_t amount) {
  return player.RemoveInventory(instance, amount);
}

std::string MergePendingEquipmentState(std::optional<std::string>& pending, const std::string& current, const std::string& incoming) {
  if (!pending.has_value()) {
    return incoming;
  }

  if (incoming == *pending) {
    pending.reset();
    return incoming;
  }

  return current;
}

void SendPlayerAttributeUpdate(PlayerManager& player_manager, Character& subject, PlayerAttributeId attribute_id,
                               std::int32_t value) {
  if (subject.instance_change)
    subject.instance_change->attributes[attribute_id] = value;
  PlayerAttributeUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_ATTRIBUTE_UPDATE;
  packet.player_id = subject.player_id;
  packet.attribute_id = attribute_id;
  packet.value = value;
  packet.actor_generation = subject.actor_generation;
  packet.command_revision = AdvanceActorCommand(subject);
  packet.state_sequence = AdvancePlayerStateSequence(subject);

  BroadcastToRelevant(player_manager, subject, packet, TrafficClass::Control);
}

void SendPlayerAttributeSnapshot(PlayerManager& player_manager, const Character& subject, Net::ConnectionHandle connection) {
  PlayerAttributeSnapshotPacket packet{};
  packet.packet_type = PT_PLAYER_ATTRIBUTE_SNAPSHOT;
  packet.player_id = subject.player_id;
  packet.strength = subject.strength;
  packet.actor_generation = subject.actor_generation;
  packet.command_revision = subject.command_revision;
  packet.state_sequence = subject.state_sequence;
  packet.dexterity = subject.dexterity;
  packet.level = subject.level;
  packet.exp = subject.exp;
  packet.next_level_exp = subject.next_level_exp;
  packet.learn_points = subject.learn_points;
  packet.health = subject.health;
  packet.max_health = subject.max_health;
  packet.mana = subject.mana;
  packet.max_mana = subject.max_mana;
  SerializeAndSend(packet, TrafficClass::Control, connection);
}

void SendPlayerInstanceUpdate(PlayerManager& player_manager, const Character& subject, Net::ConnectionHandle connection) {
  PlayerInstanceUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_INSTANCE_UPDATE;
  packet.player_id = subject.player_id;
  packet.instance = subject.instance;
  packet.actor_generation = subject.actor_generation;
  packet.command_revision = subject.command_revision;
  SerializeAndSend(packet, TrafficClass::Control, connection);
}

void BroadcastPlayerInstanceUpdate(PlayerManager& player_manager, const Character& subject) {
  PlayerInstanceUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_INSTANCE_UPDATE;
  packet.player_id = subject.player_id;
  packet.instance = subject.instance;
  packet.actor_generation = subject.actor_generation;
  packet.command_revision = subject.command_revision;
  BroadcastToRelevant(player_manager, subject, packet, TrafficClass::Control);
}

void SendPlayerColorUpdate(PlayerManager& player_manager, const Character& subject, Net::ConnectionHandle connection) {
  PlayerColorUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_COLOR_UPDATE;
  packet.player_id = subject.player_id;
  packet.r = subject.name_color_r;
  packet.g = subject.name_color_g;
  packet.b = subject.name_color_b;
  SerializeAndSend(packet, TrafficClass::Control, connection);
}

void BroadcastPlayerColorUpdate(PlayerManager& player_manager, const Character& subject) {
  PlayerColorUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_COLOR_UPDATE;
  packet.player_id = subject.player_id;
  packet.r = subject.name_color_r;
  packet.g = subject.name_color_g;
  packet.b = subject.name_color_b;
  BroadcastToRelevant(player_manager, subject, packet, TrafficClass::Control);
}

void SendPlayerSkillWeaponUpdate(PlayerManager& player_manager, const Character& subject, std::int32_t skill_id,
                                 std::int32_t percentage, Net::ConnectionHandle connection) {
  PlayerSkillWeaponUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_SKILL_WEAPON_UPDATE;
  packet.player_id = subject.player_id;
  packet.skill_id = skill_id;
  packet.percentage = percentage;
  SerializeAndSend(packet, TrafficClass::Control, connection);
}

void BroadcastPlayerSkillWeaponUpdate(PlayerManager& player_manager, const Character& subject, std::int32_t skill_id,
                                      std::int32_t percentage) {
  PlayerSkillWeaponUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_SKILL_WEAPON_UPDATE;
  packet.player_id = subject.player_id;
  packet.skill_id = skill_id;
  packet.percentage = percentage;
  BroadcastToRelevant(player_manager, subject, packet, TrafficClass::Control);
}

void SendPlayerTalentUpdate(PlayerManager& player_manager, const Character& subject, std::int32_t talent_id,
                            std::int32_t talent_value, Net::ConnectionHandle connection) {
  PlayerTalentUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_TALENT_UPDATE;
  packet.player_id = subject.player_id;
  packet.talent_id = talent_id;
  packet.talent_value = talent_value;
  SerializeAndSend(packet, TrafficClass::Control, connection);
}

void BroadcastPlayerTalentUpdate(PlayerManager& player_manager, const Character& subject, std::int32_t talent_id,
                                 std::int32_t talent_value) {
  PlayerTalentUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_TALENT_UPDATE;
  packet.player_id = subject.player_id;
  packet.talent_id = talent_id;
  packet.talent_value = talent_value;
  BroadcastToRelevant(player_manager, subject, packet, TrafficClass::Control);
}

void SendPlayerFatnessUpdate(PlayerManager& player_manager, const Character& subject, Net::ConnectionHandle connection) {
  PlayerFatnessUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_FATNESS_UPDATE;
  packet.player_id = subject.player_id;
  packet.fatness = subject.fatness;
  SerializeAndSend(packet, TrafficClass::Control, connection);
}

void BroadcastPlayerFatnessUpdate(PlayerManager& player_manager, const Character& subject) {
  PlayerFatnessUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_FATNESS_UPDATE;
  packet.player_id = subject.player_id;
  packet.fatness = subject.fatness;
  BroadcastToRelevant(player_manager, subject, packet, TrafficClass::Control);
}

void SendPlayerScaleUpdate(PlayerManager& player_manager, const Character& subject, Net::ConnectionHandle connection) {
  PlayerScaleUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_SCALE_UPDATE;
  packet.player_id = subject.player_id;
  packet.scale = subject.scale;
  SerializeAndSend(packet, TrafficClass::Control, connection);
}

void BroadcastPlayerScaleUpdate(PlayerManager& player_manager, const Character& subject) {
  PlayerScaleUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_SCALE_UPDATE;
  packet.player_id = subject.player_id;
  packet.scale = subject.scale;
  BroadcastToRelevant(player_manager, subject, packet, TrafficClass::Control);
}

void SendPlayerOverlayUpdate(PlayerManager& player_manager, const Character& subject, const std::string& overlay,
                             bool apply, Net::ConnectionHandle connection) {
  PlayerOverlayUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_OVERLAY_UPDATE;
  packet.player_id = subject.player_id;
  packet.overlay = overlay;
  packet.apply = apply ? 1 : 0;
  SerializeAndSend(packet, TrafficClass::Control, connection);
}

void BroadcastPlayerOverlayUpdate(PlayerManager& player_manager, const Character& subject, const std::string& overlay,
                                  bool apply) {
  PlayerOverlayUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_OVERLAY_UPDATE;
  packet.player_id = subject.player_id;
  packet.overlay = overlay;
  packet.apply = apply ? 1 : 0;
  BroadcastToRelevant(player_manager, subject, packet, TrafficClass::Control);
}

void InitializeLogger(const Config& config) {
  auto logger = spdlog::default_logger();
  logger->sinks().clear();

  if (config.Get<bool>("log_to_stdout")) {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_pattern("[%^%l%$] %v");
    logger->sinks().push_back(std::move(console_sink));
  }

  auto log_file = config.Get<std::string>("log_file");
  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(std::move(log_file), false);
  file_sink->set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");
  logger->sinks().push_back(std::move(file_sink));

  auto log_level = config.Get<std::string>("log_level");
  spdlog::set_level(spdlog::level::from_str(log_level));
  spdlog::flush_on(spdlog::level::debug);
}
}  // namespace

GameServer::GameServer() {
  InitializeLogger(config_);
  LogServerBanner();
  config_.LogConfigValues();
  server_world_ = SanitizeWorldName(config_.Get<std::string>("map"));
  config_.Set<std::string>("map", server_world_);
  g_server = this;
  DiagnosticsManager::Instance().Reset();

  // Register server-side events.
  EventManager::Instance().RegisterEvent(kEventOnTickName);
  EventManager::Instance().RegisterEvent(kEventOnNpcCreatedName);
  EventManager::Instance().RegisterEvent(kEventOnNpcDestroyedName);
  EventManager::Instance().RegisterEvent(kEventOnNpcChangeHostPlayerName);
  EventManager::Instance().RegisterEvent(kEventOnNpcActionFinishedName);
  EventManager::Instance().RegisterEvent(kEventOnNpcActionSentName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerConnectName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerDisconnectName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerMessageName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerCommandName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerUnconsciousName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerStandUpName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerDeathName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerDropItemName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerTakeItemName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerUseItemName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerChangeHealthName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerChangeManaName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerWorldChangeName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerWorldEnterName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerWeaponModeChangeName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerEquipAmuletName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerEquipArmorName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerEquipBeltName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerEquipHandItemName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerEquipHelmetName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerEquipMeleeWeaponName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerEquipRangedWeaponName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerEquipRingName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerEquipShieldName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerEquipSpellSlotName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerCastSpellName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerSummonName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerTeleportName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerSpawnName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerRespawnName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerSpawnForName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerUnspawnForName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerHitName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerVoiceStartName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerVoiceStopName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerVoiceChannelChangeName);
}

GameServer::~GameServer() {
  g_is_server_running = false;
  if (main_thread.joinable()) {
    main_thread_running.store(false, std::memory_order_release);
    main_thread.join();
  }

  resource_server_.reset();

  EventManager::Instance().Reset();
  resource_manager_.reset();
  lua_script_.reset();

  if (public_list_http_thread_future_.valid()) {
    public_list_http_thread_future_.wait();
  }

  transport_.SetCallbacks({}, {});
  transport_.SetAdmissionCheck({});
  transport_.Stop();
  g_transport = nullptr;

  g_server = nullptr;
}

bool GameServer::Init() {
  g_transport = &transport_;
  transport_.SetCallbacks(
      [this](Net::ConnectionHandle connection, unsigned char* data, std::uint32_t size) { HandlePacket(connection, data, size); },
      [this](const Net::ConnectionEvent& event) { HandleConnectionEvent(event); });
  ban_manager_ = std::make_unique<BanManager>();
  ban_manager_->Load();
  transport_.SetAdmissionCheck([this](const std::string& ip) {
    return ban_manager_->IsBanned(ip) ? CloseReason::Banned : CloseReason::None;
  });
#ifndef WIN32
  if (config_.Get<bool>("daemon")) {
    System::MakeMeDaemon(false);
  }
#endif
  auto slots = config_.Get<std::int32_t>("slots");
  allow_modification = config_.Get<bool>("allow_modification");

  auto port = config_.Get<std::int32_t>("port");

  if (!summon_registry_.Load("data/instances/summons.json")) {
    SPDLOG_ERROR("Invalid server summon definitions in data/instances/summons.json");
    return false;
  }

  addon_context_ = std::make_unique<ServerAddonContext>();
  std::string addon_error;
  if (!addon_context_->Initialize(config_.Get<std::vector<std::string>>("addon_vdfs"), std::filesystem::current_path(),
                                  addon_error)) {
    SPDLOG_CRITICAL("Server addon initialization failed: {}", addon_error);
    return false;
  }

  if (!g_transport->Start(port, slots)) {
    SPDLOG_CRITICAL("Failed to start server on port {}", port);
    return false;
  }

  const auto bound_port = static_cast<std::uint16_t>(g_transport->GetPort());

  g_is_server_running = true;

  auto seconds_per_game_minute = config_.Get<std::int32_t>("seconds_per_game_minute");
  clock_ = std::make_unique<GothicClock>(GothicClock::Time{}, seconds_per_game_minute);
  weather_.Initialize(clock_->GetTime());
  if (IsPublic() && !kMasterServerEndpoint.empty()) {
    public_list_http_thread_future_ = std::async(std::launch::async, &GameServer::AddToPublicListHTTP, this);
    SPDLOG_INFO("Master Server heartbeat started.");
  } else if (IsPublic()) {
    SPDLOG_WARN("Server marked as public, but no Master Server endpoint is configured. Skipping registration.");
  } else if (!IsPublic()) {
    SPDLOG_WARN("Server marked as private, skipping connection to Master Server..");
  }
  SPDLOG_INFO(kFrame);

  // Initialize Lua VM
  lua_script_ = std::make_unique<LuaScript>();

  // Initialize resource manager
  resource_manager_ = std::make_unique<ResourceManager>();

  // Set up resource-aware timer binding
  resource_manager_->BindResourceAwareTimer(*lua_script_);

  // Set up exports proxy
  resource_manager_->BindExports(lua_script_->GetLuaState());

  // Discover resource metadata, then load only the resources configured by server order.
  resource_manager_->DiscoverResources();
  resource_manager_->LogResourceInfo();

  const auto& configured_resource_names = config_.Get<std::vector<std::string>>("resources");
  std::vector<ResourceManager::DiscoveredResource> configured_resources;
  if (!SelectConfiguredResources(*resource_manager_, configured_resource_names, configured_resources)) {
    return false;
  }

  try {
    client_resource_descriptors_ = ClientResourcePackager::Build(configured_resources);
  } catch (const std::exception& ex) {
    SPDLOG_ERROR("Failed to pack client resources: {}", ex.what());
    return false;
  }

  std::uint64_t advertised_download_size = 0;
  const auto add_download_size = [&advertised_download_size](std::uint64_t size) {
    if (size > std::numeric_limits<std::uint64_t>::max() - advertised_download_size) {
      return false;
    }
    advertised_download_size += size;
    return true;
  };
  for (const auto& descriptor : client_resource_descriptors_) {
    if (!add_download_size(descriptor.archive_size) || !add_download_size(descriptor.manifest_size)) {
      SPDLOG_ERROR("Advertised client resource download size overflow");
      return false;
    }
  }
  if (!addon_context_->Empty()) {
    const auto& bundle = addon_context_->TransportBundle().descriptor;
    if (!add_download_size(bundle.archive_size) || !add_download_size(bundle.manifest_size)) {
      SPDLOG_ERROR("Advertised addon download size overflow");
      return false;
    }
  }
  if (advertised_download_size > kMaximumAdvertisedDownloadSize) {
    SPDLOG_CRITICAL("Configured resources require {} bytes, exceeding the hardcoded maximum download size of {} bytes", advertised_download_size,
                    kMaximumAdvertisedDownloadSize);
    return false;
  }
  SPDLOG_INFO("Advertised client download size: {} bytes", advertised_download_size);

  resource_server_ = std::make_unique<ResourceServer>(
      bound_port, std::filesystem::absolute(kPublicDataPath),
      static_cast<std::size_t>(config_.Get<std::int32_t>("downloader_file_max_chunk")),
      static_cast<std::uint32_t>(config_.Get<std::int32_t>("downloader_rate_limit")),
      static_cast<std::uint32_t>(config_.Get<std::int32_t>("downloader_download_timeout_seconds")));
  if (!addon_context_->Empty()) {
    const auto& bundle = addon_context_->TransportBundle();
    if (!resource_server_->RegisterAllowedAsset(bundle.descriptor.archive_path, bundle.archive_path) ||
        !resource_server_->RegisterAllowedAsset(bundle.descriptor.manifest_path, bundle.manifest_path)) {
      SPDLOG_ERROR("Failed to register addon bundle downloads");
      return false;
    }
  }
  if (!resource_server_->Start()) {
    return false;
  }

  for (const auto& resource : configured_resources) {
    if (!resource_manager_->LoadResource(resource.name, *lua_script_)) {
      return false;
    }
  }

  last_update_time_ = std::chrono::steady_clock::now();
  last_player_presence_update_time_ = last_update_time_;

  main_thread_running.store(true, std::memory_order_release);
  main_thread = std::thread([this]() {
    while (main_thread_running.load(std::memory_order_acquire)) {
      Run();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  });
  SPDLOG_INFO("");
  SPDLOG_INFO(kFrame);
  SPDLOG_INFO("Gothic Multiplayer Server initialized successfully!");
  SPDLOG_INFO(kFrame);
  return true;
}

void GameServer::Run() {
  DiagnosticsManager::Instance().BeginFrame();

  g_transport->Pulse();
  const auto advanced_times = clock_->RunClock();
  UpdateAuthoritativeWorldState(advanced_times);

  if (lua_script_) {
    lua_script_->ProcessTimers();
  }

  auto now = std::chrono::steady_clock::now();
  if (now - last_player_presence_update_time_ > kPlayerPresenceUpdateInterval) {
    last_player_presence_update_time_ = now;
    BroadcastPlayerPresence();
  }

  DispatchPendingActorEvents();
  ProcessRespawns();
  ProcessInstanceChanges();
  ProcessEquipmentCommands();
  ProcessActorEffects();
  ProcessVoiceActivity(now);

  EventManager::Instance().TriggerEvent(kEventOnTickName, OnTickEvent{});

  // Send updates to all players.
  if (now - last_update_time_ > std::chrono::milliseconds(config_.Get<std::int32_t>("tick_rate_ms"))) {
    last_update_time_ = now;
    ProcessNpcs();
    RefreshPlayerStreaming(true);
  }

  DiagnosticsManager::ServerSnapshot snapshot;
  snapshot.connected_players = player_manager_.GetPlayerCount();
  snapshot.client_packages = client_resource_descriptors_.size();
  player_manager_.ForEachPlayer([&](const Player& player) {
    if (player.is_ingame) {
      ++snapshot.ingame_players;
    }
    snapshot.streamed_player_links += player.streamed_by_players.size();
  });
  if (resource_manager_) {
    for (const auto& entry : resource_manager_->GetResources()) {
      const auto& resource = entry.second;
      if (resource && resource->IsLoaded()) {
        ++snapshot.loaded_resources;
      }
    }
  }
  snapshot.ground_items = item_ground_manager_.Items().size();
  for (const auto& entry : item_ground_manager_.Items()) {
    const auto& item_ground = entry.second;
    snapshot.streamed_ground_item_links += item_ground.streamed_to.size();
  }
  DiagnosticsManager::Instance().SetServerSnapshot(snapshot);
  DiagnosticsManager::Instance().EndFrame();
  DispatchPendingActorEvents();
}

void GameServer::ProcessRespawns() {
  const auto default_seconds = config_.Get<std::int32_t>("respawn_time_seconds");
  const auto now = std::chrono::steady_clock::now();
  std::vector<PlayerId> due;
  player_manager_.ForEachPlayer([&](Player& player) {
    if (!player.is_ingame || player.tod == 0 || !player.death_started_at) return;
    // An explicit override wins over a disabled default. Zero explicitly disables
    // automatic respawn for this player; default zero means immediate respawn.
    if (player.respawn_time_ms && *player.respawn_time_ms == 0) return;
    if (!player.respawn_time_ms && default_seconds < 0) return;
    const std::int64_t delay_ms = player.respawn_time_ms ? *player.respawn_time_ms
        : static_cast<std::int64_t>(default_seconds) * 1000;
    if (now - *player.death_started_at < std::chrono::milliseconds(delay_ms)) return;
    due.push_back(player.player_id);
  });
  for (const auto id : due) {
    const auto current = player_manager_.GetPlayer(id);
    if (current && current->get().is_ingame && current->get().tod != 0) RespawnPlayerInternal(current->get());
  }
}

bool GameServer::RespawnPlayerInternal(Player& player) {
  player.flags = 0;
  player.tod = 0;
  player.death_started_at.reset();
  const auto old_health = player.health;
  const auto old_mana = player.mana;
  player.health = player.max_health;
  player.mana = player.max_mana;
  player.state.health_points = player.health;
  player.state.mana_points = player.mana;
  player.state.life_state = GetPlayerLifeState(player);
  ClearTransientCombatState(player);
  AdvanceActorCommand(player);
  player.combat_barrier_revision = player.command_revision;
  AdvancePlayerStateSequence(player);
  // Lua health/mana callbacks may immediately override the restored values.
  // Deliver the respawn first so those ordered attribute updates remain final.
  SendRespawnInfo(player.player_id);
  const auto player_id = player.player_id;
  const auto generation = player.actor_generation;
  const auto restored_health = player.health;
  const auto restored_mana = player.mana;
  const auto position = player.state.position;
  if (old_health != restored_health) {
    EventManager::Instance().TriggerEvent(kEventOnPlayerChangeHealthName, OnPlayerChangeHealthEvent{player_id, old_health, restored_health});
  }
  auto current = player_manager_.GetPlayer(player_id);
  if (!current || current->get().actor_generation != generation) return true;
  if (old_mana != restored_mana) {
    EventManager::Instance().TriggerEvent(kEventOnPlayerChangeManaName, OnPlayerChangeManaEvent{player_id, old_mana, restored_mana});
  }
  current = player_manager_.GetPlayer(player_id);
  if (current && current->get().actor_generation == generation) {
    EventManager::Instance().TriggerEvent(kEventOnPlayerRespawnName, OnPlayerRespawnEvent{player_id, position});
  }
  return true;
}

bool GameServer::HandlePacket(Net::ConnectionHandle connectionHandle, unsigned char* data, std::uint32_t size) {
  if (data == nullptr || size == 0) {
    DiagnosticsManager::Instance().RecordPacketRejected(0);
    SPDLOG_WARN("Rejected empty packet from {}", FormatConnectionDetails(connectionHandle));
    return false;
  }

  Packet p(data, size, connectionHandle);

  unsigned char packetIdentifier = GetPacketIdentifier(p);
  DiagnosticsManager::Instance().RecordIncomingPacket(packetIdentifier, size);
  const auto packet_handler_started_at = std::chrono::steady_clock::now();

  switch (packetIdentifier) {
    case PT_REQUEST_FILE_LENGTH:
    case PT_REQUEST_FILE_PART:
      break;
    case PT_JOIN_GAME:
      SomeoneJoinGame(p);
      break;
    case PT_ACTUAL_STATISTICS:  // dostarcza nam informacji o sobie
      HandlePlayerUpdate(p);
      break;
    case PT_PLAYER_INSTANCE_READY:
      HandlePlayerInstanceReady(p);
      break;
    case PT_NPC_ACTION_RESULT:
      HandleNpcActionResult(p);
      break;
    case PT_PLAYER_TELEPORT:
      HandleNativeTeleport(p);
      break;
    case PT_PLAYER_TRANSFORMATION:
      HandleTransformationReturn(p);
      break;
    case PT_EQUIPMENT_RESULT:
      HandleEquipmentResult(p);
      break;
    case PT_ACTOR_ACTION:
      HandleActorAction(p);
      break;
    case PT_MSG:
      HandleNormalMsg(p);
      break;
    case PT_CASTSPELL:
      HandleCastSpell(p, false);
      break;
    case PT_CASTSPELLONTARGET:
      HandleCastSpell(p, true);
      break;
    case PT_PLAYER_HIT:
      HandlePlayerHitReport(p, packetIdentifier);
      break;
    case PT_PLAYER_UNCONSCIOUS:
      HandlePlayerUnconsciousReport(p, packetIdentifier);
      break;
    case PT_PLAYER_STANDUP:
      HandlePlayerStandUpReport(p, packetIdentifier);
      break;
    case PT_PLAYER_DIED:
      HandlePlayerDeathReport(p, packetIdentifier);
      break;
    case PT_DROPITEM:
      HandleDropItem(p);
      break;
    case PT_TAKEITEM:
      HandleTakeItem(p);
      break;
    case PT_PLAYER_WORLD_ENTER:
      HandlePlayerWorldEnter(p);
      break;
    case PT_GAME_INFO:  // na razie tylko czas
      HandleGameInfo(p);
      break;
    case PT_LUA_EVENT:
      HandleLuaEvent(p);
      break;
    case PT_VOICE:
      HandleVoice(p);
      break;
    case PT_VOICE_CHANNEL:
      HandleVoiceChannel(p);
      break;
    default:
      DiagnosticsManager::Instance().RecordPacketRejected(packetIdentifier);
      SPDLOG_WARN("(S)He or it try to do something strange. It's packet ID: {}", packetIdentifier);
      break;
  }
  DiagnosticsManager::Instance().RecordPacketHandlerTime(packetIdentifier, std::chrono::steady_clock::now() - packet_handler_started_at);
  return true;
}

bool GameServer::Receive() {
  g_transport->Pulse();
  return true;
}

unsigned char GameServer::GetPacketIdentifier(const Packet& p) {
  return p.data != nullptr && p.length != 0 ? p.data[0] : 0;
}

void GameServer::HandleConnectionEvent(const Net::ConnectionEvent& event) {
  if (event.kind == ConnectionEvent::Kind::Connected) {
    HandlePlayerConnected(event.connection);
    return;
  }

  const auto player = player_manager_.GetPlayerByConnection(event.connection);
  const auto label = player ? FormatPlayerLabel(player->get()) : FormatConnectionDetails(event.connection);
  if (player) {
    SendDisconnectionInfo(player->get().player_id);
  }
  const bool lost = event.kind == ConnectionEvent::Kind::Failure;
  HandlePlayerDisconnect(event.connection, lost ? gmp::server::LOST_CONNECTION : gmp::server::DISCONNECTED);
  SPDLOG_INFO("{} {} (reason {}: {}). Still connected users: {}.", label, lost ? "lost connection" : "disconnected",
              static_cast<int>(event.reason), event.detail, player_manager_.GetPlayerCount());
}

void GameServer::HandlePlayerConnected(Net::ConnectionHandle connection) {
  if (player_manager_.HasConnection(connection)) {
    return;
  }
  // Add player to the manager
  auto max_slots = GetMaxSlots();
  PlayerId new_player_id = player_manager_.AddPlayer(connection, "", max_slots);
  if (new_player_id == 0) {
    transport_.CloseConnection(connection, CloseReason::ServerFull);
    return;
  }

  if (auto new_player = player_manager_.GetPlayer(new_player_id)) {
    new_player->get().ip_address = transport_.GetPlayerIp(connection);
    new_player->get().identity = transport_.GetPlayerIdentity(connection);
    new_player->get().world = server_world_;
    new_player->get().virtual_world = 0;
    new_player->get().voice_range = static_cast<std::uint32_t>(config_.Get<std::int32_t>("voice_range"));
  }

  // Send packet with initial information.
  InitialInfoPacket packet;
  packet.packet_type = PT_INITIAL_INFO;
  packet.map_name = server_world_;
  packet.player_id = new_player_id;
  packet.server_name = GetHostname();
  packet.max_slots = static_cast<std::uint16_t>(max_slots);
  if (auto new_player = player_manager_.GetPlayer(new_player_id)) {
    const Player& player = new_player->get();
    packet.actor_generation = player.actor_generation;
    packet.voice_enabled = config_.Get<bool>("voice_enabled") && player.voice_enabled ? 1 : 0;
    packet.voice_transmit_enabled = packet.voice_enabled != 0 && !player.voice_muted ? 1 : 0;
    packet.voice_range = player.voice_range;
  }
  packet.resource_token = resource_server_->IssueToken(connection);
  packet.resource_base_path = "/public";
  packet.downloader_group = config_.Get<std::string>("downloader_group");
  packet.client_resources.reserve(client_resource_descriptors_.size());
  for (const auto& descriptor : client_resource_descriptors_) {
    ClientResourceInfoEntry entry;
    entry.name = descriptor.name;
    entry.version = descriptor.version;
    entry.manifest_path = descriptor.manifest_path;
    entry.manifest_sha256 = descriptor.manifest_sha256;
    entry.archive_path = descriptor.archive_path;
    entry.archive_sha256 = descriptor.archive_sha256;
    entry.archive_size = descriptor.archive_size;
    entry.manifest_size = descriptor.manifest_size;
    packet.client_resources.push_back(std::move(entry));
  }
  packet.addon_vdfs.reserve(addon_context_->Archives().size());
  for (const auto& addon : addon_context_->Archives()) {
    packet.addon_vdfs.push_back(addon.descriptor);
  }
  packet.addon_bundle = addon_context_->TransportBundle().descriptor;
  SerializeAndSend(packet, TrafficClass::Control, connection);
  SPDLOG_INFO("Incoming connection from {}.", FormatConnectionDetails(connection));
}

void GameServer::DeleteFromPlayerList(PlayerId player_id) {
  ReleaseNpcsForPlayer(player_id);
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (player_opt.has_value()) {
    UnstreamGroundItemsFromPlayer(player_opt->get(), false);
  }
  EndPlayerVoiceActivity(player_id);
  voice_rate_states_.erase(player_id);
  player_manager_.RemovePlayer(player_id);
}

void GameServer::HandlePlayerDisconnect(Net::ConnectionHandle connection, std::int32_t reason) {
  resource_server_->RevokeToken(connection);

  auto player_opt = player_manager_.GetPlayerByConnection(connection);
  if (player_opt.has_value()) {
    auto& player = player_opt.value().get();
    if (player.is_ingame) {
      EventManager::Instance().TriggerEvent(kEventOnPlayerDisconnectName, OnPlayerDisconnectEvent{player.player_id, reason});
    }
    DeleteFromPlayerList(player.player_id);
  }
}

std::optional<std::reference_wrapper<GameServer::Player>> GameServer::GetIngamePlayerByConnection(Net::ConnectionHandle connection) {
  auto player_opt = player_manager_.GetPlayerByConnection(connection);
  if (!player_opt.has_value() || !player_opt->get().is_ingame) {
    return std::nullopt;
  }

  return player_opt;
}

void GameServer::HandlePlayerHitReport(Packet p, std::uint8_t packet_identifier) {
  PlayerHitReportPacket packet;
  if (!DeserializeClientPacket(p, packet, packet_identifier, "PlayerHitReportPacket")) {
    return;
  }

  auto sender = GetIngamePlayerByConnection(p.id);
  if (!sender || !sender->get().npc_world_ready) return;
  const auto attacker_id = packet.attacker_id.value_or(sender->get().player_id);
  auto attacker_opt = GetCharacter(attacker_id);
  auto victim_opt = GetCharacter(packet.victim_id);
  if (!attacker_opt || !victim_opt || attacker_id == packet.victim_id) return;
  if (attacker_id != sender->get().player_id) {
    const auto npc = npc_manager_.GetNpc(attacker_id);
    if (!npc || npc->get().host_player_id != sender->get().player_id || npc->get().control_epoch != packet.control_epoch ||
        !IsEligibleNpcHost(npc->get(), sender->get().player_id)) return;
  }
  auto& attacker = attacker_opt->get();
  auto& victim = victim_opt->get();
  if (packet.actor_generation != attacker.actor_generation || packet.victim_generation != victim.actor_generation ||
      !attacker.CombatRevisionIsCurrent(packet.command_revision) ||
      !IsLiveCharacter(attacker) || !victim.is_ingame || victim.tod != 0 || !SameCharacterScope(attacker, victim) ||
      !SameCharacterScope(sender->get(), victim) || (!sender->get().spawned_players.contains(victim.player_id) &&
       victim.player_id != sender->get().player_id) || victim.invulnerable || !IsFinitePosition(attacker.state.position) ||
      !IsFinitePosition(victim.state.position) || (!packet.finishing_move &&
       (packet.damage <= 0 || packet.damage_type == 0 || packet.damage_type > 255)) ||
      packet.attack_id == 0) return;
  if (packet.finishing_move && !CanFinishCharacter(attacker, victim)) return;

  const auto now = std::chrono::steady_clock::now();
  auto bounds = GetCombatBounds(attacker);
  while (!attacker.approved_projectiles.empty() && attacker.approved_projectiles.front().expires_at < now)
    attacker.approved_projectiles.pop_front();
  const auto approved = packet.finishing_move ? attacker.approved_projectiles.end() :
      std::find_if(attacker.approved_projectiles.begin(), attacker.approved_projectiles.end(),
      [&](const Character::ApprovedAttack& attack) {
        return attack.expires_at >= now && (packet.damage_type & ~attack.damage_types) == 0 &&
               !attack.victims.contains(victim.player_id) && attack.victims.size() < attack.max_victims &&
               glm::distance(attack.origin, victim.state.position) <= static_cast<float>(attack.range);
      });
  const bool projectile = approved != attacker.approved_projectiles.end();
  if (!projectile && attacker.state.weapon_mode >= 5) return;
  if (projectile) {
    bounds.damage = approved->damage_cap;
    bounds.types = approved->damage_types;
    bounds.reach = approved->range;
  }
  if (!packet.finishing_move && (bounds.damage <= 0 || bounds.reach == 0 || (packet.damage_type & ~bounds.types) != 0 ||
      (!projectile && glm::distance(attacker.state.position, victim.state.position) > static_cast<float>(bounds.reach)))) return;
  if (attacker.last_hit_action_id && *attacker.last_hit_action_id == packet.attack_id) {
    if ((packet.finishing_move && !attacker.hit_victims.empty()) || attacker.hit_victims.contains(victim.player_id) ||
        attacker.hit_victims.size() >= 32 ||
        now - attacker.last_attack_at > std::chrono::seconds(1)) return;
  } else {
    if (attacker.last_hit_action_id && (!IsNewActorSequence(packet.attack_id, *attacker.last_hit_action_id) ||
        (!projectile && now - attacker.last_attack_at < std::chrono::milliseconds(bounds.interval_ms)))) return;
    attacker.last_hit_action_id = packet.attack_id;
    attacker.last_attack_at = now;
    attacker.hit_victims.clear();
  }
  attacker.hit_victims.insert(victim.player_id);
  if (projectile) approved->victims.insert(victim.player_id);
  victim.last_validated_attacker = attacker_id;
  victim.last_damage_at = now;
  if (packet.finishing_move) {
    // Gothic's finishing move calls DoDie directly, bypassing damage and armor.
    ApplyCharacterDamage(victim, attacker_id, victim.health, bounds.types ? bounds.types : 4u, false, false, true);
    return;
  }
  // Native protection/critical-hit calculation is attested, but its result may
  // never exceed the catalog weapon and actor attribute bound.
  ApplyCharacterDamage(victim, attacker_id,
      std::min(packet.damage, bounds.damage),
      packet.damage_type, packet.dont_kill,
      !projectile && attacker.state.weapon_mode < 5 && (packet.damage_type & ~6u) == 0);
}

void GameServer::HandlePlayerUnconsciousReport(Packet p, std::uint8_t packet_identifier) {
  PlayerUnconsciousPacket packet;
  if (!DeserializeClientPacket(p, packet, packet_identifier, "PlayerUnconsciousPacket")) {
    return;
  }

  auto player_opt = GetIngamePlayerByConnection(p.id);
  if (!player_opt.has_value()) {
    return;
  }

  auto& player = player_opt->get();
  if (packet.actor_generation != player.actor_generation || packet.command_revision != player.command_revision ||
      !player.npc_world_ready || player.tod != 0 || player.health > 1) {
    return;
  }

  std::optional<PlayerId> attacker;
  if (packet.attacker_id.has_value()) {
    auto attacker_opt = GetCharacter(packet.attacker_id.value());
    if (attacker_opt && attacker_opt->get().is_ingame && SameCharacterScope(player, attacker_opt->get()) &&
        player.last_validated_attacker == packet.attacker_id &&
        std::chrono::steady_clock::now() - player.last_damage_at <= std::chrono::seconds(2)) {
      attacker = packet.attacker_id.value();
    }
  }

  MakePlayerUnconscious(player, attacker);
}

void GameServer::HandlePlayerStandUpReport(Packet p, std::uint8_t packet_identifier) {
  PlayerStandUpPacket packet;
  if (!DeserializeClientPacket(p, packet, packet_identifier, "PlayerStandUpPacket")) {
    return;
  }

  auto player_opt = GetIngamePlayerByConnection(p.id);
  if (!player_opt.has_value()) {
    return;
  }

  auto& player = player_opt->get();
  if (packet.actor_generation != player.actor_generation || packet.command_revision != player.command_revision ||
      !player.npc_world_ready || player.tod != 0 || (player.flags & PlayerManager::PL_UNCONCIOUS) == 0 ||
      std::chrono::steady_clock::now() - player.last_damage_at < std::chrono::seconds(1)) {
    return;
  }

  const auto old_health = player.health;
  player.flags &= ~PlayerManager::PL_UNCONCIOUS;
  ClearRecoveredPose(player);
  if (player.health <= 0) {
    player.health = 1;
  }
  player.state.health_points = player.health;
  player.state.life_state = GetPlayerLifeState(player);
  AdvanceActorCommand(player);
  player.combat_barrier_revision = player.command_revision;
  AdvancePlayerStateSequence(player);
  if (old_health != player.health) {
    SendPlayerAttributeUpdate(player_manager_, player, ATTR_HEALTH, player.health);
  }
  const auto player_id = player.player_id;
  const auto new_hp = player.health;
  SendStandUpInfo(player_id);
  const auto generation = player.actor_generation;
  pending_npc_events_.push_back([this, player_id, generation, old_health, new_hp] {
    const auto current = GetCharacter(player_id);
    if (!current || current->get().actor_generation != generation) return;
    if (old_health != new_hp) EventManager::Instance().TriggerEvent(kEventOnPlayerChangeHealthName, OnPlayerChangeHealthEvent{player_id, old_health, new_hp});
    const auto after_health = GetCharacter(player_id);
    if (after_health && after_health->get().actor_generation == generation &&
        GetPlayerLifeState(after_health->get()) == PLAYER_LIFE_ALIVE)
      EventManager::Instance().TriggerEvent(kEventOnPlayerStandUpName, OnPlayerStandUpEvent{player_id});
  });
}

void GameServer::HandlePlayerDeathReport(Packet p, std::uint8_t packet_identifier) {
  PlayerDeathReportPacket packet;
  if (!DeserializeClientPacket(p, packet, packet_identifier, "PlayerDeathReportPacket")) {
    return;
  }

  auto player_opt = GetIngamePlayerByConnection(p.id);
  if (!player_opt.has_value()) {
    return;
  }

  auto& player = player_opt->get();
  if (packet.actor_generation != player.actor_generation || !player.npc_world_ready || player.tod != 0 || player.health > 0) {
    return;
  }

  std::optional<PlayerId> killer_id;
  if (packet.killer_id.has_value()) {
    auto killer_opt = GetCharacter(packet.killer_id.value());
    if (killer_opt && killer_opt->get().is_ingame && SameCharacterScope(player, killer_opt->get()) &&
        player.last_validated_attacker == packet.killer_id &&
        std::chrono::steady_clock::now() - player.last_damage_at <= std::chrono::seconds(2)) {
      killer_id = packet.killer_id.value();
    }
  }

  HandlePlayerDeath(player, killer_id);
}

bool GameServer::ApplyPlayerDamage(Player& victim, std::optional<PlayerId> attacker_id, std::int32_t damage, std::uint32_t damage_type,
                                   bool dont_kill) {
  return ApplyCharacterDamage(victim, attacker_id, damage, damage_type, dont_kill);
}

bool GameServer::ApplyCharacterDamage(Character& victim, std::optional<PlayerId> attacker_id, std::int32_t damage,
                                     std::uint32_t damage_type, bool dont_kill, bool melee_attack, bool finishing_move) {
  if (victim.instance_change) return false;
  if (!victim.is_ingame || victim.tod != 0 || victim.invulnerable || damage <= 0) {
    return false;
  }
  const auto victim_id = victim.player_id;
  const auto victim_generation = victim.actor_generation;
  const auto victim_world = victim.world;
  const auto victim_virtual_world = victim.virtual_world;
  std::optional<std::uint64_t> attacker_generation;
  std::optional<std::pair<std::uint32_t, std::uint32_t>> npc_control;
  if (attacker_id) {
    const auto attacker = GetCharacter(*attacker_id);
    if (!attacker) return false;
    attacker_generation = attacker->get().actor_generation;
    if (const auto npc = npc_manager_.GetNpc(*attacker_id))
      npc_control = std::pair{npc->get().host_player_id, npc->get().control_epoch};
  }
  std::optional<std::uint64_t> event_attacker;
  if (attacker_id.has_value()) {
    event_attacker = attacker_id.value();
  }
  auto result = EventManager::Instance().DispatchEvent(kEventOnPlayerHitName, std::any(OnPlayerHitEvent{event_attacker, victim.player_id, damage, damage_type}));
  if (result.cancelled) {
    return false;
  }

  if (result.value.has_value()) {
    damage = result.value.value();
  }
  if (damage <= 0) {
    return false;
  }
  auto current_opt = GetCharacter(victim_id);
  if (!current_opt || current_opt->get().actor_generation != victim_generation || !current_opt->get().is_ingame ||
      current_opt->get().tod != 0 || current_opt->get().invulnerable || current_opt->get().world != victim_world ||
      current_opt->get().virtual_world != victim_virtual_world) return false;
  auto& current = current_opt->get();
  if (attacker_id) {
    const auto attacker = GetCharacter(*attacker_id);
    if (!attacker || attacker->get().actor_generation != attacker_generation || !IsLiveCharacter(attacker->get()) ||
        !SameCharacterScope(current, attacker->get())) return false;
    if (finishing_move && !CanFinishCharacter(attacker->get(), current)) return false;
    if (npc_control) {
      const auto npc = npc_manager_.GetNpc(*attacker_id);
      if (!npc || npc->get().host_player_id != npc_control->first || npc->get().control_epoch != npc_control->second ||
          !IsEligibleNpcHost(npc->get(), npc_control->first)) return false;
    }
  }
  const auto old_health = current.health;
  auto new_health = static_cast<std::int64_t>(old_health) - damage;
  const bool player_damage = attacker_id && player_manager_.GetPlayer(*attacker_id) && player_manager_.GetPlayer(victim_id);
  const bool victim_was_unconscious = (current.flags & PlayerManager::PL_UNCONCIOUS) != 0;

  if (new_health <= 0) {
    // Classic PvP melee first knocks out; paid projectiles and environmental
    // damage can kill. Explicit nonlethal damage also protects an existing KO.
    if (player_manager_.GetPlayer(victim_id) && !victim_was_unconscious &&
        (dont_kill || (player_damage && melee_attack))) {
      MakePlayerUnconscious(player_manager_.GetPlayer(victim_id)->get(), attacker_id);
      return false;
    }
    if (dont_kill && old_health > 0) {
      new_health = 1;
    } else {
      HandlePlayerDeath(current, attacker_id);
      return true;
    }
  }
  new_health = std::clamp<std::int64_t>(new_health, 0, current.max_health);

  current.health = static_cast<std::int32_t>(new_health);
  current.state.health_points = current.health;
  current.state.life_state = GetPlayerLifeState(current);
  AdvancePlayerStateSequence(current);
  if (old_health != current.health) {
    const auto new_hp = current.health;
    SendPlayerAttributeUpdate(player_manager_, current, ATTR_HEALTH, new_hp);
    if (attacker_id) {
      ActorDamagePacket reaction;
      reaction.victim_id = victim_id;
      reaction.victim_generation = victim_generation;
      reaction.state_sequence = current.state_sequence;
      reaction.command_revision = current.command_revision;
      reaction.source_time_ms = ActorClockMilliseconds();
      reaction.attacker_id = attacker_id;
      reaction.attacker_generation = attacker_generation.value_or(0);
      reaction.damage = old_health - new_hp;
      reaction.damage_type = damage_type;
      player_manager_.ForEachIngamePlayer([&](const Player& viewer) {
        if (viewer.npc_world_ready && SameCharacterScope(current, viewer) &&
            (viewer.player_id == victim_id || current.streamed_by_players.contains(viewer.player_id)))
          SerializeAndSend(reaction, TrafficClass::Control, viewer.connection);
      });
    }
    pending_npc_events_.push_back([this, victim_id, victim_generation, old_health, new_hp] {
      const auto actor = GetCharacter(victim_id);
      if (!actor || actor->get().actor_generation != victim_generation) return;
      EventManager::Instance().TriggerEvent(kEventOnPlayerChangeHealthName, OnPlayerChangeHealthEvent{victim_id, old_health, new_hp});
    });
  }

  return false;
}

bool GameServer::MakePlayerUnconscious(Player& victim, std::optional<PlayerId> attacker_id) {
  if (!victim.is_ingame || victim.tod != 0) {
    return false;
  }

  const bool was_unconscious = (victim.flags & PlayerManager::PL_UNCONCIOUS) != 0;
  const auto old_health = victim.health;
  victim.flags |= PlayerManager::PL_UNCONCIOUS;
  victim.health = 1;
  victim.state.health_points = 1;
  victim.state.life_state = GetPlayerLifeState(victim);
  ClearTransientCombatState(victim);
  AdvanceActorCommand(victim);
  victim.combat_barrier_revision = victim.command_revision;
  AdvancePlayerStateSequence(victim);

  if (old_health != victim.health) {
    SendPlayerAttributeUpdate(player_manager_, victim, ATTR_HEALTH, victim.health);
  }

  if (was_unconscious) {
    return false;
  }

  std::optional<std::uint64_t> event_attacker;
  if (attacker_id.has_value()) {
    event_attacker = attacker_id.value();
  }
  SendUnconsciousInfo(victim.player_id, attacker_id);
  const auto victim_id = victim.player_id;
  const auto generation = victim.actor_generation;
  pending_npc_events_.push_back([this, victim_id, generation, old_health, event_attacker] {
    const auto actor = GetCharacter(victim_id);
    if (!actor || actor->get().actor_generation != generation) return;
    if (old_health != 1) EventManager::Instance().TriggerEvent(kEventOnPlayerChangeHealthName, OnPlayerChangeHealthEvent{victim_id, old_health, 1});
    const auto current = GetCharacter(victim_id);
    if (current && current->get().actor_generation == generation &&
        GetPlayerLifeState(current->get()) == PLAYER_LIFE_UNCONSCIOUS)
      EventManager::Instance().TriggerEvent(kEventOnPlayerUnconsciousName, OnPlayerUnconsciousEvent{event_attacker, victim_id});
  });
  return true;
}

void GameServer::HandlePlayerDeath(Character& victim, std::optional<PlayerId> killer_id) {
  if (victim.tod != 0) {
    return;
  }

  const auto old_health = victim.health;
  const auto victim_id = victim.player_id;
  const auto generation = victim.actor_generation;
  victim.health = 0;
  victim.state.health_points = 0;
  victim.flags &= ~PlayerManager::PL_UNCONCIOUS;
  victim.tod = std::time(nullptr);
  victim.death_started_at = std::chrono::steady_clock::now();
  victim.state.life_state = GetPlayerLifeState(victim);
  ClearTransientCombatState(victim);
  AdvanceActorCommand(victim);
  victim.combat_barrier_revision = victim.command_revision;
  AdvancePlayerStateSequence(victim);
  if (npc_manager_.GetNpc(victim.player_id)) {
    const auto id = victim.player_id;
    SetNpcHostPlayer(id, 0);
    ClearNpcActions(id);
    PlayerDeathInfoPacket packet{};
    packet.packet_type = PT_DODIE;
    packet.player_id = id;
    packet.state_sequence = victim.state_sequence;
    packet.actor_generation = victim.actor_generation;
    packet.command_revision = victim.command_revision;
    BroadcastToRelevant(player_manager_, victim, packet, TrafficClass::Control);
  } else SendDeathInfo(victim_id);
  pending_npc_events_.push_back([this, victim_id, generation, old_health, killer_id] {
    const auto actor = GetCharacter(victim_id);
    if (!actor || actor->get().actor_generation != generation) return;
    if (old_health != 0) EventManager::Instance().TriggerEvent(kEventOnPlayerChangeHealthName, OnPlayerChangeHealthEvent{victim_id, old_health, 0});
    const auto current = GetCharacter(victim_id);
    if (current && current->get().actor_generation == generation && current->get().tod != 0)
      EventManager::Instance().TriggerEvent(kEventOnPlayerDeathName, OnPlayerDeathEvent{victim_id, killer_id});
  });
}

void GameServer::SomeoneJoinGame(Packet p) {
  auto player_opt = player_manager_.GetPlayerByConnection(p.id);
  if (!player_opt) {
    SPDLOG_WARN("Someone tried to join game, but he is not on the player list, connection {}!", p.id);
    return;
  }
  auto& player = player_opt.value().get();

  if (player.has_joined) return;

  if (!allow_modification) {
    if (!player.passed_crc_test) {
      ban_manager_->AddTemporaryBan(transport_.GetPlayerIp(p.id), std::chrono::hours(1));
      transport_.CloseConnection(p.id, CloseReason::Banned, "Client integrity check failed");
      return;
    }
  }

  JoinGamePacket packet;
  if (!DeserializeClientPacket(p, packet, GetPacketIdentifier(p), "JoinGamePacket")) {
    return;
  }
  SPDLOG_TRACE("{} from {}", packet, p.id);
  if (packet.actor_generation != player.actor_generation) {
    transport_.CloseConnection(p.id, CloseReason::Failure, "Invalid actor generation");
    return;
  }
  if (!IsValidActorPosition(packet.position) || !IsFinitePosition(packet.normal) ||
      glm::dot(packet.normal, packet.normal) < 0.0001f || glm::dot(packet.normal, packet.normal) > 4.0f || packet.walk_style > 4 ||
      SanitizeServerText(packet.body_model) != packet.body_model || SanitizeServerText(packet.head_model) != packet.head_model ||
      !gmp::IsCharacterWalkingOverlay(packet.walking_overlay) ||
      packet.body_texture > 32767 || packet.head_texture > 32767 || packet.teeth_texture > 32767 || packet.skin_color > 32767) return;
  player.has_joined = true;
  player.state.position = packet.position;
  player.state.nrot = glm::normalize(packet.normal);
  player.state.source_time_ms = ActorClockMilliseconds();
  player.state.left_hand_item_instance = packet.left_hand_item_instance;
  player.state.right_hand_item_instance = packet.right_hand_item_instance;
  player.state.equipped_armor_instance = packet.equipped_armor_instance;
  player.state.equipped_helmet_instance = packet.equipped_helmet_instance;
  player.state.equipped_shield_instance = packet.equipped_shield_instance;
  player.state.equipped_amulet_instance = packet.equipped_amulet_instance;
  player.state.equipped_belt_instance = packet.equipped_belt_instance;
  player.state.equipped_ring_left_instance = packet.equipped_ring_left_instance;
  player.state.equipped_ring_right_instance = packet.equipped_ring_right_instance;
  NormalizePlayerStateInstances(player.state);
  player.state.animation_name = SanitizePlayerAnimationName(std::move(packet.animation_name));
  player.body_model = packet.body_model;
  player.body_texture = packet.body_texture;
  player.head_model = packet.head_model;
  player.head_texture = packet.head_texture;
  player.teeth_texture = packet.teeth_texture;
  player.skin_color = packet.skin_color;
  player.walkstyle = packet.walk_style;
  if (!packet.walking_overlay.empty()) player.overlays.push_back(packet.walking_overlay);
  player.name = SanitizePlayerName(packet.player_name);

  BroadcastPlayerJoined(player);

  SPDLOG_INFO("{} joined the server. Now connected users: {}.", FormatPlayerLabel(player), player_manager_.GetPlayerCount());

  // join
  EventManager::Instance().TriggerEvent(kEventOnPlayerConnectName, player.player_id);
  BroadcastPlayerPresence();
}

void GameServer::HandlePlayerInstanceReady(Packet p) {
  PlayerInstanceReadyPacket packet;
  if (!DeserializeClientPacket(p, packet, GetPacketIdentifier(p), "PlayerInstanceReadyPacket"))
    return;
  const auto sender = GetIngamePlayerByConnection(p.id);
  auto actor = GetCharacter(packet.defaults.player_id);
  if (!sender || !actor)
    return;
  auto& character = actor->get();
  const auto npc = npc_manager_.GetNpc(character.player_id);
  const bool controls = npc ? npc->get().host_player_id == sender->get().player_id && npc->get().control_epoch == packet.control_epoch &&
                                  IsEligibleNpcHost(npc->get(), sender->get().player_id)
                            : character.player_id == sender->get().player_id;
  if (!controls || !character.instance_change || packet.request_revision != character.instance_change->request_revision ||
      packet.defaults.actor_generation != character.actor_generation || packet.defaults.instance != character.instance)
    return;

  const auto& defaults = packet.defaults;
  std::unordered_map<std::string, std::int32_t> inventory;
  if (packet.success) {
    const bool valid = [&] {
      if (defaults.packet_type != PT_PLAYER_SPAWN)
        return false;
      const auto nonnegative = [](int value) { return value >= 0; };
      const std::array stats{defaults.health,    defaults.max_health, defaults.mana, defaults.max_mana,       defaults.strength,
                             defaults.dexterity, defaults.level,      defaults.exp,  defaults.next_level_exp, defaults.learn_points};
      if (!std::all_of(stats.begin(), stats.end(), nonnegative) || defaults.health > defaults.max_health || defaults.mana > defaults.max_mana ||
          !std::isfinite(defaults.fatness) || !IsFinitePosition(defaults.scale) || defaults.scale.x <= 0 || defaults.scale.y <= 0 ||
          defaults.scale.z <= 0 || defaults.weapon_mode > 7 || defaults.body_state >= 64 || defaults.walk_mode > 10 ||
          defaults.body_texture > 32767 || defaults.head_texture > 32767 || defaults.skin_color > 32767 || defaults.teeth_texture > 32767 ||
          defaults.inventory.size() > kMaxInventoryEntries || defaults.overlays.size() > 64)
        return false;
      std::unordered_set<int> skills, talents;
      for (const auto& entry : defaults.weapon_skills)
        if (!gmp::IsWeaponSkill(entry.skill_id) || entry.percentage < 0 || entry.percentage > 100 || !skills.insert(entry.skill_id).second)
          return false;
      for (const auto& entry : defaults.talents)
        if (entry.talent_id < 1 || entry.talent_id >= 128 || entry.value < 0 || !talents.insert(entry.talent_id).second)
          return false;
      for (const auto& entry : defaults.inventory) {
        if (!gmp::IsValidScriptName(entry.item_instance) || entry.amount <= 0 ||
            !inventory.emplace(gmp::NormalizeScriptName(entry.item_instance), entry.amount).second)
          return false;
      }
      for (const auto& overlay : defaults.overlays)
        if (overlay.empty() || overlay != SanitizeServerText(overlay))
          return false;
      if (defaults.body_model != SanitizeServerText(defaults.body_model) || defaults.head_model != SanitizeServerText(defaults.head_model))
        return false;
      return true;
    }();
    if (!valid) {
      SPDLOG_WARN("Rejected constructor defaults for actor {} instance '{}'", character.player_id, character.instance);
      packet.success = false;
    }
  }

  CompletePlayerInstanceChange(character, defaults, std::move(inventory), packet.success);
}

void GameServer::CompletePlayerInstanceChange(Character& character, const PlayerSpawnPacket& defaults,
                                              std::unordered_map<std::string, std::int32_t> inventory, bool success) {
  const auto npc = npc_manager_.GetNpc(character.player_id);
  auto pending = std::move(*character.instance_change);
  character.instance_change.reset();
  if (success) {
    for (const auto& [instance, delta] : pending.inventory_deltas) {
      const auto count = std::clamp<std::int64_t>(static_cast<std::int64_t>(inventory[instance]) + delta, 0, INT_MAX);
      if (count)
        inventory[instance] = static_cast<std::int32_t>(count);
      else
        inventory.erase(instance);
    }
    std::size_t name_bytes = 0;
    for (const auto& [name, _] : inventory) name_bytes += name.size();
    if (inventory.size() > kMaxInventoryEntries || name_bytes > kMaxInventoryNameBytes) {
      SPDLOG_WARN("Rejected oversized constructor inventory for actor {}", character.player_id);
      success = false;
    }
  }
  if (!success) {
    character.instance = pending.previous_instance;
  } else {
    // Constructors run in Gothic, once for this issued request. Never accept
    // an unsolicited state replacement or another player's completion report.
    character.health = defaults.health;
    character.max_health = defaults.max_health;
    character.mana = defaults.mana;
    character.max_mana = defaults.max_mana;
    character.strength = defaults.strength;
    character.dexterity = defaults.dexterity;
    character.level = defaults.level;
    character.exp = defaults.exp;
    character.next_level_exp = defaults.next_level_exp;
    character.learn_points = defaults.learn_points;
    const std::pair<PlayerAttributeId, std::int32_t Character::*> attributes[] = {{ATTR_HEALTH, &Character::health},
                                                                                  {ATTR_MAX_HEALTH, &Character::max_health},
                                                                                  {ATTR_MANA, &Character::mana},
                                                                                  {ATTR_MAX_MANA, &Character::max_mana},
                                                                                  {ATTR_STRENGTH, &Character::strength},
                                                                                  {ATTR_DEXTERITY, &Character::dexterity},
                                                                                  {ATTR_LEVEL, &Character::level},
                                                                                  {ATTR_EXP, &Character::exp},
                                                                                  {ATTR_NEXT_LEVEL_EXP, &Character::next_level_exp},
                                                                                  {ATTR_LEARN_POINTS, &Character::learn_points}};
    for (const auto& [id, member] : attributes) {
      const auto override = pending.attributes.find(id);
      if (override != pending.attributes.end())
        character.*member = override->second;
    }
    character.health = std::clamp(character.health, 0, character.max_health);
    character.mana = std::clamp(character.mana, 0, character.max_mana);
    character.inventory = std::move(inventory);
    character.weapon_skills.clear();
    character.talents.clear();
    for (const auto& entry : defaults.weapon_skills) character.weapon_skills[entry.skill_id] = entry.percentage;
    for (const auto& entry : defaults.talents) character.talents[entry.talent_id] = entry.value;
    for (const auto& [id, value] : pending.weapon_skills) character.weapon_skills[id] = value;
    for (const auto& [id, value] : pending.talents) character.talents[id] = value;
    if (!pending.visual_changed) {
      character.body_model = defaults.body_model;
      character.body_texture = static_cast<std::int16_t>(defaults.body_texture);
      character.head_model = defaults.head_model;
      character.head_texture = static_cast<std::int16_t>(defaults.head_texture);
      character.teeth_texture = static_cast<std::int16_t>(defaults.teeth_texture);
      character.skin_color = static_cast<std::int16_t>(defaults.skin_color);
    }
    if (!pending.fatness_changed)
      character.fatness = defaults.fatness;
    if (!pending.scale_changed)
      character.scale = defaults.scale;
    if (!pending.invulnerable_changed)
      character.invulnerable = defaults.invulnerable;
    character.overlays = defaults.overlays;
    for (const auto& [overlay, apply] : pending.overlays) {
      auto found = std::find(character.overlays.begin(), character.overlays.end(), overlay);
      if (apply && found == character.overlays.end())
        character.overlays.push_back(overlay);
      else if (!apply && found != character.overlays.end())
        character.overlays.erase(found);
    }
    PlayerState state = character.state;
    const std::pair<std::string PlayerState::*, std::string> equipment[] = {
        {&PlayerState::equipped_armor_instance, defaults.equipped_armor_instance},
        {&PlayerState::equipped_helmet_instance, defaults.equipped_helmet_instance},
        {&PlayerState::equipped_shield_instance, defaults.equipped_shield_instance},
        {&PlayerState::equipped_amulet_instance, defaults.equipped_amulet_instance},
        {&PlayerState::equipped_belt_instance, defaults.equipped_belt_instance},
        {&PlayerState::equipped_ring_left_instance, defaults.equipped_ring_left_instance},
        {&PlayerState::equipped_ring_right_instance, defaults.equipped_ring_right_instance},
        {&PlayerState::melee_weapon_instance, defaults.melee_weapon_instance},
        {&PlayerState::ranged_weapon_instance, defaults.ranged_weapon_instance}};
    for (const auto& [member, value] : equipment)
      if (character.state.*member == pending.previous_state.*member)
        state.*member = value;
    for (const auto& [slot_id, command] : character.pending_equipment_commands)
      if (const auto slot = PacketEquipmentSlot(slot_id); slot && IsNewActorSequence(command.command_revision, pending.request_revision))
        EquipmentStateField(state, *slot) = command.expected_instance;
    for (const auto slot :
         {EquipmentSlot::Armor, EquipmentSlot::Helmet, EquipmentSlot::Shield, EquipmentSlot::MeleeWeapon, EquipmentSlot::RangedWeapon,
          EquipmentSlot::Amulet, EquipmentSlot::Belt, EquipmentSlot::RingLeft, EquipmentSlot::RingRight})
      PendingEquipmentStateField(character, slot).reset();
    state.left_hand_item_instance.clear();
    state.right_hand_item_instance.clear();
    state.weapon_mode = pending.weapon_mode_changed ? character.state.weapon_mode : defaults.weapon_mode;
    state.active_spell_instance.clear();
    state.active_spell_nr = state.active_spell_level = 0;
    state.spell_investing = false;
    state.animation_name.clear();
    state.body_state = 0;  // Start with the new controller's standing pose.
    state.walk_mode = defaults.walk_mode;
    state.focus_target_id.reset();
    character.pending_equipment_commands.clear();
    NormalizePlayerStateInstances(state);
    ValidatePlayerEquipment(character, state);
    character.state = state;
    ReconcileWeaponModeHands(character);
    character.flags &= ~PlayerManager::PL_UNCONCIOUS;
    character.tod = character.health ? 0 : std::time(nullptr);
    character.death_started_at.reset();
  }
  character.state.health_points = character.health;
  character.state.mana_points = character.mana;
  character.state.life_state = GetPlayerLifeState(character);
  character.state.invulnerable = character.invulnerable;
  character.state.source_time_ms = ActorClockMilliseconds();
  character.approved_projectiles.clear();
  character.last_hit_action_id.reset();
  character.combat_barrier_revision = AdvanceActorCommand(character);
  AdvancePlayerStateSequence(character);
  if (npc) {
    UnstreamNpc(npc->get());
    SetNpcHostPlayer(character.player_id, 0);
    character.RenewGeneration();
    RefreshNpcStreaming(character.player_id);
  } else {
    if (const auto connection = player_manager_.GetConnectionHandle(character.player_id))
      SerializeAndSend(MakePlayerSpawnPacket(character), TrafficClass::Control, *connection);
    const auto snapshot = MakePlayerSpawnPacket(character);
    for (const auto viewer : character.streamed_by_players)
      if (const auto connection = player_manager_.GetConnectionHandle(viewer))
        SerializeAndSend(snapshot, TrafficClass::Control, *connection);
  }
}

void GameServer::ProcessInstanceChanges() {
  const auto now = std::chrono::steady_clock::now();
  const auto process = [&](Character& actor, bool ready) {
    if (!actor.instance_change)
      return;
    // World loading and NPCs without a host cannot execute constructors yet.
    if (!ready) {
      actor.instance_change->deadline = now + std::chrono::seconds(30);
      return;
    }
    if (now < actor.instance_change->deadline)
      return;
    SPDLOG_WARN("Instance change timed out for actor {} instance '{}'", actor.player_id, actor.instance);
    CompletePlayerInstanceChange(actor, PlayerSpawnPacket{}, {}, false);
  };
  player_manager_.ForEachPlayer([&](Player& player) { process(player, player.is_ingame && player.npc_world_ready); });
  for (const auto id : npc_manager_.GetNpcIds())
    if (auto npc = npc_manager_.GetNpc(id))
      process(npc->get(), npc->get().is_ingame && npc->get().host_player_id != 0);
}

void GameServer::HandlePlayerUpdate(Packet p) {
  auto player_opt = GetIngamePlayerByConnection(p.id);
  if (!player_opt.has_value()) {
    return;
  }
  auto& updated_player = player_opt.value().get();
  if (!updated_player.npc_world_ready || updated_player.instance_change) {
    return;
  }

  PlayerStateUpdatePacket packet;
  if (!DeserializeClientPacket(p, packet, GetPacketIdentifier(p), "PlayerStateUpdatePacket")) {
    return;
  }

  if (packet.actor_generation != updated_player.actor_generation || packet.command_revision != updated_player.command_revision) {
    DiagnosticsManager::Instance().RecordPacketRejected(GetPacketIdentifier(p));
    return;
  }

  const auto old_state = updated_player.state;
  const auto updated_actor_id = updated_player.player_id;
  const auto old_health = updated_player.health;
  const auto item_instance_or_nil = [](const std::string& instance) -> std::optional<std::string> {
    return instance.empty() ? std::nullopt : std::optional<std::string>{instance};
  };

  auto client_state = packet.state;
  if (!IsValidActorPosition(client_state.position) || !IsFinitePosition(client_state.nrot) ||
      glm::dot(client_state.nrot, client_state.nrot) < 0.0001f || glm::dot(client_state.nrot, client_state.nrot) > 4.0f ||
      client_state.weapon_mode > 7 || client_state.active_spell_nr > 8 || client_state.active_spell_level > kMaxSpellInvestLevel ||
      client_state.head_direction > 4 || client_state.walk_mode > 2 || client_state.body_state >= 31 || !std::isfinite(client_state.look_yaw) ||
      !std::isfinite(client_state.look_pitch) || client_state.look_yaw < 0.0f || client_state.look_yaw > 1.0f || client_state.look_pitch < 0.0f ||
      client_state.look_pitch > 1.0f || client_state.health_points < 0 || client_state.mana_points < 0 || client_state.source_time_ms == 0) {
    DiagnosticsManager::Instance().RecordPacketRejected(GetPacketIdentifier(p));
    return;
  }
  client_state.nrot = glm::normalize(client_state.nrot);
  if (!updated_player.AcceptClientState(packet.actor_generation, packet.command_revision, packet.state_sequence)) return;
  const auto now_ms = ActorClockMilliseconds();
  if (!updated_player.client_clock_origin) {
    updated_player.client_clock_origin = client_state.source_time_ms;
    updated_player.server_clock_origin = now_ms;
  }
  if (client_state.source_time_ms < *updated_player.client_clock_origin) return;
  const auto elapsed_ms = client_state.source_time_ms - *updated_player.client_clock_origin;
  if (elapsed_ms > now_ms - updated_player.server_clock_origin + 250) return;
  const auto mapped_time = updated_player.server_clock_origin + elapsed_ms;
  if (mapped_time < updated_player.last_source_time_ms) return;
  updated_player.last_source_time_ms = mapped_time;
  client_state.source_time_ms = mapped_time;
  client_state.correction_mask = kSyncAll;
  if (client_state.focus_target_id) {
    const auto focus = GetCharacter(*client_state.focus_target_id);
    if (!focus || !IsSameVisibilityScope(updated_player, focus->get()) ||
        !updated_player.spawned_players.count(*client_state.focus_target_id)) client_state.focus_target_id.reset();
  }
  NormalizePlayerStateInstances(client_state);
  ValidatePlayerEquipment(updated_player, client_state);

  updated_player.state.position = client_state.position;
  updated_player.state.nrot = client_state.nrot;
  updated_player.state.source_time_ms = client_state.source_time_ms;
  updated_player.state.body_state = client_state.body_state;
  updated_player.state.walk_mode = client_state.walk_mode;
  updated_player.state.look_yaw = client_state.look_yaw;
  updated_player.state.look_pitch = client_state.look_pitch;
  updated_player.state.focus_target_id = client_state.focus_target_id;
  updated_player.state.left_hand_item_instance = client_state.left_hand_item_instance;
  updated_player.state.right_hand_item_instance = client_state.right_hand_item_instance;
  updated_player.state.equipped_armor_instance =
      MergePendingEquipmentState(updated_player.pending_equipped_armor_instance, updated_player.state.equipped_armor_instance,
                                 client_state.equipped_armor_instance);
  updated_player.state.equipped_helmet_instance =
      MergePendingEquipmentState(updated_player.pending_equipped_helmet_instance, updated_player.state.equipped_helmet_instance,
                                 client_state.equipped_helmet_instance);
  updated_player.state.equipped_shield_instance =
      MergePendingEquipmentState(updated_player.pending_equipped_shield_instance, updated_player.state.equipped_shield_instance,
                                 client_state.equipped_shield_instance);
  updated_player.state.equipped_amulet_instance = client_state.equipped_amulet_instance;
  updated_player.state.equipped_belt_instance = client_state.equipped_belt_instance;
  updated_player.state.equipped_ring_left_instance = client_state.equipped_ring_left_instance;
  updated_player.state.equipped_ring_right_instance = client_state.equipped_ring_right_instance;
  updated_player.state.animation_name = SanitizePlayerAnimationName(std::move(client_state.animation_name));
  updated_player.state.weapon_mode = client_state.weapon_mode;
  updated_player.state.active_spell_nr = client_state.active_spell_nr;
  updated_player.state.active_spell_instance = client_state.active_spell_instance;
  updated_player.state.active_spell_level = client_state.active_spell_level;
  updated_player.state.spell_investing = client_state.spell_investing;
  updated_player.state.head_direction = client_state.head_direction;
  updated_player.state.melee_weapon_instance =
      MergePendingEquipmentState(updated_player.pending_melee_weapon_instance, updated_player.state.melee_weapon_instance,
                                 client_state.melee_weapon_instance);
  updated_player.state.ranged_weapon_instance =
      MergePendingEquipmentState(updated_player.pending_ranged_weapon_instance, updated_player.state.ranged_weapon_instance,
                                 client_state.ranged_weapon_instance);
  if (updated_player.tod != 0 || (updated_player.flags & PlayerManager::PL_UNCONCIOUS) != 0) {
    ClearTransientCombatState(updated_player);
  }

  if (updated_player.tod == 0) {
    const auto requested_health = std::clamp<std::int32_t>(client_state.health_points, 0, updated_player.max_health);
    const auto effective_health = requested_health > old_health ? static_cast<std::int32_t>(old_health) : requested_health;

    if (effective_health < old_health && (updated_player.flags & PlayerManager::PL_UNCONCIOUS) == 0) {
      ApplyPlayerDamage(updated_player, std::nullopt, static_cast<std::int32_t>(old_health) - effective_health, 0, false);
    }

  }

  updated_player.state.health_points = updated_player.health;
  updated_player.state.mana_points = updated_player.mana;
  updated_player.state.life_state = GetPlayerLifeState(updated_player);
  AdvancePlayerStateSequence(updated_player);

  if (old_state.weapon_mode != updated_player.state.weapon_mode) {
    EventManager::Instance().TriggerEvent(
        kEventOnPlayerWeaponModeChangeName,
        OnPlayerWeaponModeChangeEvent{updated_player.player_id, old_state.weapon_mode, updated_player.state.weapon_mode});
  }

  const auto correct_held_equipment = [&](Character& actor, std::uint16_t components = kSyncEquipment) {
    AdvanceActorCommand(actor);
    AdvancePlayerStateSequence(actor);
    auto correction = MakePlayerStateUpdatePacket(actor);
    correction.state.correction_mask = components;
    BroadcastToRelevant(player_manager_, actor, correction, TrafficClass::Control);
  };
  for (const auto hand : {0, 1}) {
    auto actor = GetCharacter(updated_actor_id);
    if (!actor || actor->get().actor_generation != packet.actor_generation) return;
    auto& state = actor->get().state;
    auto& field = hand == 0 ? state.left_hand_item_instance : state.right_hand_item_instance;
    const auto observed = field;
    const auto previous = hand == 0 ? old_state.left_hand_item_instance : old_state.right_hand_item_instance;
    if (observed == previous) continue;
    field = previous;
    const auto result = EventManager::Instance().DispatchEvent(
        kEventOnPlayerEquipHandItemName,
        OnPlayerEquipHandItemEvent{updated_actor_id, static_cast<std::uint8_t>(hand),
                                  item_instance_or_nil(observed), item_instance_or_nil(previous)});
    actor = GetCharacter(updated_actor_id);
    if (!actor || actor->get().actor_generation != packet.actor_generation) return;
    if (result.cancelled) correct_held_equipment(actor->get());
    else {
      auto& current = actor->get().state;
      (hand == 0 ? current.left_hand_item_instance : current.right_hand_item_instance) = observed;
    }
  }

  const std::array previous_equipment{
      std::pair{EquipmentSlot::Armor, old_state.equipped_armor_instance},
      std::pair{EquipmentSlot::Helmet, old_state.equipped_helmet_instance},
      std::pair{EquipmentSlot::Shield, old_state.equipped_shield_instance},
      std::pair{EquipmentSlot::Amulet, old_state.equipped_amulet_instance},
      std::pair{EquipmentSlot::Belt, old_state.equipped_belt_instance},
      std::pair{EquipmentSlot::RingLeft, old_state.equipped_ring_left_instance},
      std::pair{EquipmentSlot::RingRight, old_state.equipped_ring_right_instance},
      std::pair{EquipmentSlot::MeleeWeapon, old_state.melee_weapon_instance},
      std::pair{EquipmentSlot::RangedWeapon, old_state.ranged_weapon_instance}};
  for (const auto& [slot, previous] : previous_equipment) {
    auto actor = GetCharacter(updated_actor_id);
    if (!actor || actor->get().actor_generation != packet.actor_generation) return;
    const auto observed = EquipmentStateField(actor->get().state, slot);
    const auto slot_id = EquipmentSlotPacketId(slot);
    if (observed == previous ||
        IsNewActorSequence(actor->get().equipment_command_revisions[slot_id], packet.command_revision)) continue;
    const auto revision = actor->get().command_revision;
    EquipmentStateField(actor->get().state, slot) = previous;
    const bool accepted = TriggerEquipmentEvent(updated_actor_id, slot, item_instance_or_nil(observed), item_instance_or_nil(previous));
    actor = GetCharacter(updated_actor_id);
    if (!actor || actor->get().actor_generation != packet.actor_generation) return;
    if (!IsNewActorSequence(actor->get().equipment_command_revisions[slot_id], revision)) {
      if (accepted) EquipmentStateField(actor->get().state, slot) = observed;
      else SendEquipmentCorrection(actor->get(), slot, observed);
    }
  }

  if (old_state.active_spell_nr != updated_player.state.active_spell_nr ||
      old_state.active_spell_instance != updated_player.state.active_spell_instance) {
    const auto observed_slot = updated_player.state.active_spell_nr;
    const auto observed_item = updated_player.state.active_spell_instance;
    updated_player.state.active_spell_nr = old_state.active_spell_nr;
    updated_player.state.active_spell_instance = old_state.active_spell_instance;
    const auto result = EventManager::Instance().DispatchEvent(
        kEventOnPlayerEquipSpellSlotName, OnPlayerEquipSpellSlotEvent{updated_actor_id, observed_slot, item_instance_or_nil(observed_item),
                                   item_instance_or_nil(old_state.active_spell_instance)});
    const auto actor = GetCharacter(updated_actor_id);
    if (!actor || actor->get().actor_generation != packet.actor_generation) return;
    if (result.cancelled) {
      actor->get().state.active_spell_level = old_state.active_spell_level;
      actor->get().state.spell_investing = old_state.spell_investing;
      correct_held_equipment(actor->get(), kSyncEquipment | kSyncCombat);
    } else {
      actor->get().state.active_spell_nr = observed_slot;
      actor->get().state.active_spell_instance = observed_item;
    }
  }

  // Publish player poses on the server cadence. Immediate relay plus tick relay
  // sent identical revisions twice and amplified bursty client arrivals.
  if (client_state.health_points != updated_player.health) {
    SendPlayerAttributeUpdate(player_manager_, updated_player, ATTR_HEALTH, updated_player.health);
    if (client_state.life_state != GetPlayerLifeState(updated_player)) {
      auto correction = MakePlayerStateUpdatePacket(updated_player);
      correction.state.correction_mask = kSyncHealth | kSyncLife;
      SerializeAndSend(correction, TrafficClass::Control, updated_player.connection);
    }
  }
  if (client_state.mana_points != updated_player.mana) {
    SendPlayerAttributeUpdate(player_manager_, updated_player, ATTR_MANA, updated_player.mana);
  }
}

void GameServer::HandleLuaEvent(Packet p) {
  const auto sender = GetIngamePlayerByConnection(p.id);
  if (!sender) {
    DiagnosticsManager::Instance().RecordPacketRejected(GetPacketIdentifier(p));
    return;
  }
  const auto sender_player_id = sender->get().player_id;
  LuaEventPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  if (!state.second) {
    DiagnosticsManager::Instance().RecordPacketRejected(GetPacketIdentifier(p));
    SPDLOG_ERROR("Failed to deserialize LuaEventPacket, error code: {}", static_cast<int>(state.first));
    return;
  }

  if (!lua_script_) {
    SPDLOG_ERROR("Lua event '{}' received without active Lua script", packet.event_name);
    return;
  }

  std::string payload(packet.payload.begin(), packet.payload.end());
  std::vector<sol::object> args;
  std::string error;
  if (!gmp::lua::DecodeLuaArgs(lua_script_->GetLuaState(), payload, args, error)) {
    DiagnosticsManager::Instance().RecordPacketRejected(GetPacketIdentifier(p));
    SPDLOG_ERROR("Failed to decode Lua event '{}' payload: {}", packet.event_name, error);
    return;
  }

  if (!lua::bindings::TriggerRemoteEvent(lua_script_->GetLuaState(), packet.event_name, sender_player_id, packet.source_element, args)) {
    SPDLOG_WARN("Lua event '{}' rejected or cancelled", packet.event_name);
  }
}

void GameServer::HandleVoice(Packet p) {
  if (!config_.Get<bool>("voice_enabled")) {
    return;
  }

  VoicePacket packet;
  if (!DeserializeClientPacket(p, packet, GetPacketIdentifier(p), "VoicePacket")) {
    return;
  }

  auto sender_opt = GetIngamePlayerByConnection(p.id);
  if (!sender_opt.has_value() || packet.talkspurt_id == 0 || packet.voice_data.empty() ||
      packet.voice_data.size() > kMaxVoiceFrameSize) {
    DiagnosticsManager::Instance().RecordPacketRejected(GetPacketIdentifier(p));
    return;
  }

  Player& sender = sender_opt->get();
  if (!sender.voice_enabled || sender.voice_muted) {
    return;
  }

  const int frame_samples = opus_packet_get_nb_samples(packet.voice_data.data(),
                                                       static_cast<opus_int32>(packet.voice_data.size()),
                                                       kVoiceSampleRate);
  if (frame_samples != kVoiceFrameSamples) {
    DiagnosticsManager::Instance().RecordPacketRejected(GetPacketIdentifier(p));
    return;
  }

  const auto now = std::chrono::steady_clock::now();
  VoiceRateState& rate = voice_rate_states_[sender.player_id];
  if (rate.window_started.time_since_epoch().count() == 0 || now - rate.window_started >= std::chrono::seconds(1)) {
    rate.window_started = now;
    rate.packets_in_window = 0;
  }
  if (++rate.packets_in_window > kMaxVoicePacketsPerSecond) {
    DiagnosticsManager::Instance().RecordPacketRejected(GetPacketIdentifier(p));
    return;
  }

  const bool spatial = IsVoiceChannelSpatial(sender.voice_channel);
  const float voice_range = static_cast<float>(sender.voice_range);
  if (spatial && voice_range <= 0.0f) {
    return;
  }

  MarkPlayerVoiceActive(sender.player_id, now);
  packet.player_id = sender.player_id;
  packet.spatial = spatial ? 1 : 0;
  packet.range = sender.voice_range;
  const float voice_range_squared = voice_range * voice_range;
  player_manager_.ForEachIngamePlayer([&](const Player& viewer) {
    if (viewer.player_id == sender.player_id || !viewer.voice_enabled || viewer.voice_channel != sender.voice_channel) {
      return;
    }

    if (spatial) {
      if (!IsSameVisibilityScope(viewer, sender)) {
        return;
      }
      const glm::vec3 delta = viewer.state.position - sender.state.position;
      const float distance_squared = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
      if (distance_squared > voice_range_squared) {
        return;
      }
    }

    SerializeAndSend(packet, TrafficClass::Voice, viewer.connection);
  });
}

void GameServer::HandleVoiceChannel(Packet p) {
  VoiceChannelPacket packet;
  if (!DeserializeClientPacket(p, packet, GetPacketIdentifier(p), "VoiceChannelPacket")) {
    return;
  }

  auto player_opt = GetIngamePlayerByConnection(p.id);
  if (!player_opt.has_value() || !IsValidVoiceChannelName(packet.channel)) {
    DiagnosticsManager::Instance().RecordPacketRejected(GetPacketIdentifier(p));
    return;
  }

  Player& player = player_opt->get();
  if (player.voice_channel == packet.channel) {
    return;
  }

  const std::string old_channel = player.voice_channel;
  EndPlayerVoiceActivity(player.player_id);
  player.voice_channel = packet.channel;
  EventManager::Instance().TriggerEvent(
      kEventOnPlayerVoiceChannelChangeName,
      OnPlayerVoiceChannelChangeEvent{player.player_id, old_channel, player.voice_channel});
}

void GameServer::SendVoiceConfiguration(const Player& player) {
  VoiceConfigurationPacket packet{};
  packet.packet_type = PT_VOICE_CONFIG;
  packet.enabled = config_.Get<bool>("voice_enabled") && player.voice_enabled ? 1 : 0;
  packet.transmit_enabled = packet.enabled != 0 && !player.voice_muted ? 1 : 0;
  packet.range = player.voice_range;
  SerializeAndSend(packet, TrafficClass::Control, player.connection);
}

bool GameServer::IsVoiceChannelSpatial(const std::string& channel) const {
  auto setting = voice_channel_spatial_.find(channel);
  return setting == voice_channel_spatial_.end() ? true : setting->second;
}

void GameServer::MarkPlayerVoiceActive(PlayerId player_id, std::chrono::steady_clock::time_point now) {
  const bool inserted = active_voice_players_.insert_or_assign(player_id, now).second;
  if (inserted) {
    EventManager::Instance().TriggerEvent(kEventOnPlayerVoiceStartName, OnPlayerVoiceEvent{player_id});
  }
}

void GameServer::EndPlayerVoiceActivity(PlayerId player_id) {
  if (active_voice_players_.erase(player_id) != 0) {
    EventManager::Instance().TriggerEvent(kEventOnPlayerVoiceStopName, OnPlayerVoiceEvent{player_id});
  }
}

void GameServer::ProcessVoiceActivity(std::chrono::steady_clock::time_point now) {
  std::vector<PlayerId> expired_players;
  expired_players.reserve(active_voice_players_.size());
  for (const auto& [player_id, last_packet_at] : active_voice_players_) {
    if (now - last_packet_at >= kVoiceActivityTimeout) {
      expired_players.push_back(player_id);
    }
  }

  for (const PlayerId player_id : expired_players) {
    EndPlayerVoiceActivity(player_id);
  }
}

void GameServer::SendAdminAuthStatus(const Player& player) {
  AdminAuthPacket packet{};
  packet.packet_type = PT_ADMIN_AUTH;
  packet.authenticated = player.is_admin ? 1 : 0;
  SerializeAndSend(packet, TrafficClass::Control, player.connection);
}

void GameServer::HandleAdminLogin(Player& player, const std::string& password) {
  const auto& configured_password = config_.Get<std::string>("admin_passwd");

  if (configured_password.empty()) {
    player.is_admin = false;
    SendAdminAuthStatus(player);
    SendMessageToPlayer(player.player_id, 255, 80, 0, "Admin login is disabled on this server.");
    SPDLOG_WARN("{} attempted admin login, but admin_passwd is empty", FormatPlayerLabel(player));
    return;
  }

  if (password == configured_password) {
    player.is_admin = true;
    SendAdminAuthStatus(player);
    SendMessageToPlayer(player.player_id, 0, 255, 0, "Admin login successful. Noclip unlocked.");
    SPDLOG_INFO("{} logged in as admin", FormatPlayerLabel(player));
    return;
  }

  player.is_admin = false;
  SendAdminAuthStatus(player);
  SendMessageToPlayer(player.player_id, 255, 0, 0, "Invalid admin password.");
  SPDLOG_WARN("{} failed admin login", FormatPlayerLabel(player));
}

void GameServer::HandleDiagnosticsCommand(Player& player) {
  if (!player.is_admin) {
    SendMessageToPlayer(player.player_id, 255, 80, 0, "Login first with /rcon login <password>.");
    return;
  }

  std::ofstream output(kDiagnosticsFileName, std::ios::out | std::ios::trunc);
  if (!output.is_open()) {
    SendMessageToPlayer(player.player_id, 255, 80, 0, "Failed to write diagnostic.txt.");
    SPDLOG_ERROR("{} failed to open {} for diagnostics output", FormatPlayerLabel(player), kDiagnosticsFileName);
    return;
  }

  output << "Generated: " << FormatCurrentDateTime() << "\n\n";
  output << DiagnosticsManager::Instance().BuildReport();
  output << "\nGameNetworkingSockets transport diagnostics\n"
            "Live transport rates/quality/queues are GNS estimates; gameplay counts above are application messages.\n";
  player_manager_.ForEachPlayer([&](const Player& connected_player) {
    output << "\nPlayer " << connected_player.player_id << ", connection " << connected_player.connection << "\n";
    output << transport_.GetDiagnostics(connected_player.connection) << '\n';
  });
  output.close();
  if (!output) {
    SendMessageToPlayer(player.player_id, 255, 80, 0, "Failed to write diagnostic.txt.");
    SPDLOG_ERROR("{} failed to write diagnostics output to {}", FormatPlayerLabel(player), kDiagnosticsFileName);
    return;
  }

  SendMessageToPlayer(player.player_id, 0, 200, 255, "Diagnostics written to diagnostic.txt.");
  SPDLOG_INFO("{} wrote diagnostics report to {}", FormatPlayerLabel(player), kDiagnosticsFileName);
}

void GameServer::HandleRconCommand(Player& player, const std::string& params) {
  const auto [subcommand, subcommand_params] = SplitCommandArgs(params);
  if (subcommand.empty()) {
    SendMessageToPlayer(player.player_id, 255, 80, 0, "Usage: /rcon login <password> | /rcon diagnostics");
    return;
  }

  if (IsCommand(subcommand, "login")) {
    HandleAdminLogin(player, std::string(subcommand_params));
    return;
  }

  if (IsCommand(subcommand, "diagnostics")) {
    HandleDiagnosticsCommand(player);
    return;
  }

  SendMessageToPlayer(player.player_id, 255, 80, 0, "Unknown rcon command.");
  SPDLOG_WARN("{} attempted unknown rcon command: {}", FormatPlayerLabel(player), subcommand);
}

void GameServer::HandleNormalMsg(Packet p) {
  auto player_opt = GetIngamePlayerByConnection(p.id);
  if (!player_opt.has_value()) {
    return;
  }

  auto& player = player_opt.value().get();

  MessagePacket packet;
  if (!DeserializeClientPacket(p, packet, GetPacketIdentifier(p), "MessagePacket")) {
    return;
  }

  packet.message = SanitizeServerText(packet.message);
  packet.r = 255;
  packet.g = 255;
  packet.b = 255;

  if (!packet.message.empty() && packet.message.front() == '/') {
    std::string command_line = packet.message.substr(1);
    const auto [command_view, params_view] = SplitCommandArgs(command_line);
    if (!command_view.empty()) {
      if (IsCommand(command_view, "rcon")) {
        HandleRconCommand(player, std::string(params_view));
        return;
      }

      std::string command(command_view);
      std::string params(params_view);
      SPDLOG_INFO("{} issued command: /{} {}", player.name, command, params);
      EventManager::Instance().TriggerEvent(kEventOnPlayerCommandName, OnPlayerCommandEvent{player.player_id, command, params});
    }
    return;
  }

  EventManager::Instance().TriggerEvent(kEventOnPlayerMessageName, OnPlayerMessageEvent{player.player_id, packet.message});
}

bool GameServer::CanReportActorAction(const Player& sender, const Character& actor, std::uint32_t control_epoch,
                                     std::uint32_t command_revision) const {
  return CanControlActor(sender, actor, control_epoch) && IsLiveCharacter(actor) && actor.CombatRevisionIsCurrent(command_revision);
}

bool GameServer::CanControlActor(const Player& sender, const Character& actor, std::uint32_t control_epoch) const {
  if (!sender.npc_world_ready || !SameCharacterScope(sender, actor)) return false;
  if (actor.instance_change) return false;
  if (sender.player_id == actor.player_id) return true;
  const auto npc = npc_manager_.GetNpc(actor.player_id);
  return npc && npc->get().host_player_id == sender.player_id && npc->get().control_epoch == control_epoch &&
         IsEligibleNpcHost(npc->get(), sender.player_id);
}

void GameServer::HandleNativeTeleport(Packet p) {
  PlayerTeleportPacket packet;
  if (!DeserializeClientPacket(p, packet, PT_PLAYER_TELEPORT, "PlayerTeleportPacket"))
    return;
  const auto sender = GetIngamePlayerByConnection(p.id);
  auto actor = GetCharacter(packet.player_id);
  if (!sender || !actor || packet.actor_generation != actor->get().actor_generation || !actor->get().is_ingame ||
      !CanControlActor(sender->get(), actor->get(), packet.control_epoch) || !actor->get().CombatRevisionIsCurrent(packet.command_revision) ||
      (actor->get().native_teleport_barrier_revision && packet.command_revision != *actor->get().native_teleport_barrier_revision &&
       !IsNewActorSequence(packet.command_revision, *actor->get().native_teleport_barrier_revision)) ||
      !IsValidActorPosition(packet.position) || !IsFinitePosition(packet.normal) || glm::dot(packet.normal, packet.normal) < 0.0001f ||
      glm::dot(packet.normal, packet.normal) > 4.0f || packet.destination.empty() || SanitizeServerText(packet.destination) != packet.destination ||
      !AcceptActionId(actor->get().last_teleport_action_id, packet.action_id))
    return;

  auto& character = actor->get();
  character.state.position = packet.position;
  character.state.nrot = glm::normalize(packet.normal);
  character.state.source_time_ms = ActorClockMilliseconds();
  packet.normal = character.state.nrot;
  packet.source_time_ms = character.state.source_time_ms;
  packet.command_revision = AdvanceActorCommand(character);
  packet.state_sequence = AdvancePlayerStateSequence(character);
  // Notify current observers before refreshing visibility: the native jump
  // can move the actor out of their stream. New observers get the final spawn.
  BroadcastToRelevant(player_manager_, character, packet, TrafficClass::Control);
  if (npc_manager_.GetNpc(packet.player_id))
    RefreshNpcStreaming(packet.player_id);
  else
    RefreshPlayerStreaming();
  pending_npc_events_.emplace_back([this, packet] {
    const auto current = GetCharacter(packet.player_id);
    if (current && current->get().actor_generation == packet.actor_generation)
      EventManager::Instance().TriggerEvent(kEventOnPlayerTeleportName, OnPlayerTeleportEvent{packet.player_id, packet.destination});
  });
}

void GameServer::HandleTransformationReturn(Packet p) {
  PlayerTransformationPacket packet;
  if (!DeserializeClientPacket(p, packet, PT_PLAYER_TRANSFORMATION, "PlayerTransformationPacket"))
    return;
  auto sender = GetIngamePlayerByConnection(p.id);
  if (!sender || !sender->get().npc_world_ready || packet.player_id != sender->get().player_id ||
      packet.actor_generation != sender->get().actor_generation)
    return;
  auto& actor = sender->get();
  if (!actor.transformation || actor.transformation->cast_id != packet.cast_id)
    return;
  RestoreTransformation(actor);
  actor.state.source_time_ms = ActorClockMilliseconds();
  AdvanceActorCommand(actor);
  AdvancePlayerStateSequence(actor);
  actor.combat_barrier_revision = actor.command_revision;
  BroadcastToRelevant(player_manager_, actor, MakeTransformationPacket(actor, packet.cast_id), TrafficClass::Control);
}

void GameServer::HandleCastSpell(Packet p, bool target) {
  CastSpellPacket packet{};
  if (!DeserializeClientPacket(p, packet, GetPacketIdentifier(p), "CastSpellPacket")) {
    return;
  }
  auto sender = GetIngamePlayerByConnection(p.id);
  if (!sender || !sender->get().npc_world_ready) return;
  const auto caster_id = packet.caster_id.value_or(sender->get().player_id);
  auto actor = GetCharacter(caster_id);
  if (!actor || packet.actor_generation != actor->get().actor_generation ||
      !CanControlActor(sender->get(), actor->get(), packet.control_epoch)) return;
  auto& caster = actor->get();
  const auto actor_generation = caster.actor_generation;
  const auto observed_item_instance = gmp::NormalizeScriptName(packet.item_instance.empty() ? caster.state.active_spell_instance : packet.item_instance);
  const auto reject_cast = [&] {
    // The native cast may already have paid mana or consumed a scroll. A Lua
    // callback can replace either actor; only reconcile this same incarnation.
    const auto current = GetCharacter(caster_id);
    if (!current || current->get().actor_generation != actor_generation) return;
    AdvancePlayerStateSequence(current->get());
    SendPlayerAttributeUpdate(player_manager_, current->get(), ATTR_MANA, current->get().mana);
    if (!observed_item_instance.empty()) SendItemTransactionResult(current->get(), 0, observed_item_instance, false);
    if (gmp::GetTransformationForm(packet.spell_id))
      SerializeAndSend(MakeTransformationPacket(current->get(), packet.action_id), TrafficClass::Control, p.id);
  };
  const auto now = std::chrono::steady_clock::now();
  // Casts share the same source timeline as poses and other actor actions.
  // Validate/map before consuming resources so malformed presentation time
  // cannot create an approved attack without its viewer presentation.
  const auto cast_server_time = ActorClockMilliseconds();
  if (sender->get().client_clock_origin && packet.source_time_ms >= *sender->get().client_clock_origin) {
    const auto elapsed = packet.source_time_ms - *sender->get().client_clock_origin;
    if (elapsed > cast_server_time - sender->get().server_clock_origin + 2000) {
      reject_cast();
      return;
    }
    packet.source_time_ms = sender->get().server_clock_origin + elapsed;
  } else {
    packet.source_time_ms = cast_server_time;
  }
  if (!CanReportActorAction(sender->get(), caster, packet.control_epoch, packet.command_revision)) {
    reject_cast();
    return;
  }
  if (packet.spell_id >= 100 || packet.spell_level > kMaxSpellInvestLevel || !AcceptActionId(caster.last_cast_action_id, packet.action_id)) {
    reject_cast();
    return;
  }
  if (!gmp::IsValidScriptName(observed_item_instance) || caster.InventoryCount(observed_item_instance) == 0) {
    reject_cast();
    return;
  }
  const auto spell_range = 10000u;
  const auto spell_damage = 1000000;
  const auto spell_types = 255u;
  const auto item_instance = observed_item_instance;
  packet.item_instance = item_instance;
  const bool scroll = packet.scroll;
  const auto* summon = summon_registry_.Find(packet.spell_id);
  const auto healing = GetHealingSpell(packet.spell_id);
  const auto* transformation = gmp::GetTransformationForm(packet.spell_id);
  const auto transformation_cost = scroll ? 5 : 10;
  if (transformation && (caster_id != sender->get().player_id || caster.transformation || !packet.transformation_succeeded ||
      !IsLiveCharacter(caster) || caster.mana < transformation_cost)) {
    reject_cast();
    return;
  }
  const auto summon_mana_cost = summon ? (scroll ? summon->scroll_mana_cost : summon->mana_cost) : 0;
  const auto healing_mana_cost = healing ? (scroll ? 5 : healing->mana) : 0;
  if ((summon || healing) && (!IsLiveCharacter(caster) || caster.mana < (summon ? summon_mana_cost : healing_mana_cost))) {
    reject_cast();
    return;
  }
  if (target != packet.target_id.has_value()) {
    reject_cast();
    return;
  }
  if (packet.target_id) {
    auto victim = GetCharacter(*packet.target_id);
    if (!victim || !IsLiveCharacter(victim->get()) || !SameCharacterScope(caster, victim->get()) ||
        packet.target_generation != victim->get().actor_generation ||
        (victim->get().player_id != sender->get().player_id && !sender->get().spawned_players.contains(victim->get().player_id)) ||
        glm::distance(caster.state.position, victim->get().state.position) > static_cast<float>(spell_range)) {
      reject_cast();
      return;
    }
  }
  const auto mana_before_callbacks = caster.mana;
  const auto health_before_callbacks = caster.health;
  const auto result = EventManager::Instance().DispatchEvent(kEventOnPlayerCastSpellName,
                                                             OnPlayerCastSpellEvent{caster_id, packet.spell_id, packet.target_id});
  actor = GetCharacter(caster_id);
  sender = GetIngamePlayerByConnection(p.id);
  if (!actor || !sender || actor->get().actor_generation != actor_generation) {
    reject_cast();
    return;
  }
  if (!CanReportActorAction(sender->get(), actor->get(), packet.control_epoch, packet.command_revision)) {
    reject_cast();
    return;
  }
  if (result.cancelled || actor->get().InventoryCount(item_instance) == 0) {
    reject_cast();
    return;
  }
  if (packet.target_id) {
    const auto victim = GetCharacter(*packet.target_id);
    if (!victim || victim->get().actor_generation != packet.target_generation || !IsLiveCharacter(victim->get()) ||
        !SameCharacterScope(actor->get(), victim->get()) ||
        glm::distance(actor->get().state.position, victim->get().state.position) > static_cast<float>(spell_range)) {
      reject_cast();
      return;
    }
  }
  auto& current = actor->get();
  if (transformation && (current.transformation || !IsLiveCharacter(current) || current.mana < transformation_cost)) {
    reject_cast();
    return;
  }
  if (packet.target_id) {
    const auto victim = GetCharacter(*packet.target_id);
    if (!victim || victim->get().actor_generation != packet.target_generation || !IsLiveCharacter(victim->get()) ||
        !SameCharacterScope(current, victim->get()) ||
        glm::distance(current.state.position, victim->get().state.position) > static_cast<float>(spell_range)) {
      reject_cast();
      return;
    }
  }
  std::vector<PlayerId> summons;
  if (healing && (!IsLiveCharacter(current) || current.mana < healing_mana_cost)) {
    reject_cast();
    return;
  }
  if (summon) {
    if (!IsLiveCharacter(current) || current.mana < summon_mana_cost || (summons = PrepareSpellSummons(current, packet.action_id, *summon)).empty()) {
      reject_cast();
      return;
    }
  }
  if (scroll && !RemoveInventoryItem(current, item_instance, 1)) {
    for (const auto id : summons) npc_manager_.Remove(id);
    reject_cast();
    return;
  }
  for (const auto id : summons) {
    if (!SpawnNpc(id, std::nullopt)) {
      // Preparation/spawning execute without script callbacks. Roll back the
      // whole group and reserved scroll before committing mana or cast effects.
      for (const auto prepared_id : summons) {
        if (auto npc = npc_manager_.GetNpc(prepared_id)) {
          UnstreamNpc(npc->get());
          npc_manager_.Remove(prepared_id);
        }
      }
      if (scroll)
        current.AddInventory(item_instance, 1);
      reject_cast();
      return;
    }
  }
  current.last_cast_at = now;
  const auto old_mana = current.mana;
  if (summon)
    current.mana -= summon_mana_cost;
  else if (healing)
    current.mana -= healing_mana_cost;
  else if (transformation)
    current.mana -= transformation_cost;
  else if (current.mana == mana_before_callbacks)
    current.mana = static_cast<std::int32_t>(std::clamp<std::int64_t>(
        static_cast<std::int64_t>(current.mana) + packet.mana_delta, 0, current.max_mana));
  current.state.mana_points = current.mana;
  current.state.active_spell_instance = scroll && current.InventoryCount(item_instance) == 0 ? std::string{} : item_instance;
  current.state.active_spell_level = 0;
  current.state.spell_investing = false;
  current.state.weapon_mode = 7;
  // Utility spells do not authorize projectile damage reports.
  if (!summon && !healing && !transformation && packet.spell_id != 0 && packet.spell_id != 18) {
    auto approval = Character::ApprovedAttack{packet.action_id,           spell_damage, spell_types, spell_range, now + std::chrono::seconds(5), {},
                                              packet.target_id ? 1u : 32u};
    approval.origin = current.state.position;
    while (current.approved_projectiles.size() >= 64) current.approved_projectiles.pop_front();
    current.approved_projectiles.push_back(std::move(approval));
  }
  AdvancePlayerStateSequence(current);
  SendPlayerAttributeUpdate(player_manager_, current, ATTR_MANA, current.mana);
  if (scroll) SendItemTransactionResult(current, 0, item_instance, true);
  const auto new_mana = current.mana;
  if (old_mana != new_mana) pending_npc_events_.push_back([this, caster_id, actor_generation, old_mana, new_mana] {
    const auto caster = GetCharacter(caster_id);
    if (caster && caster->get().actor_generation == actor_generation)
      EventManager::Instance().TriggerEvent(kEventOnPlayerChangeManaName, OnPlayerChangeManaEvent{caster_id, old_mana, new_mana});
  });
  if (transformation) {
    EnterTransformation(current, packet.spell_id, packet.action_id);
    AdvanceActorCommand(current);
    AdvancePlayerStateSequence(current);
    current.combat_barrier_revision = current.command_revision;
    BroadcastToRelevant(player_manager_, current, MakeTransformationPacket(current, packet.action_id), TrafficClass::Control);
    packet.command_revision = current.command_revision;
  }
  if (healing && current.health == health_before_callbacks) {
    const auto old_health = current.health;
    const auto health = static_cast<std::int32_t>(std::min<std::int64_t>(
        static_cast<std::int64_t>(current.health) + healing->health, current.max_health));
    current.health = current.state.health_points = health;
    SendPlayerAttributeUpdate(player_manager_, current, ATTR_HEALTH, health);
    if (health != old_health) pending_npc_events_.emplace_back([this, caster_id, actor_generation, old_health, health] {
      const auto caster = GetCharacter(caster_id);
      if (caster && caster->get().actor_generation == actor_generation)
        EventManager::Instance().TriggerEvent(kEventOnPlayerChangeHealthName, OnPlayerChangeHealthEvent{caster_id, old_health, health});
    });
  }
  packet.caster_id = caster_id;
  packet.item_instance = item_instance;
  player_manager_.ForEachIngamePlayer([&](const Player& viewer) {
    if (viewer.connection != p.id && viewer.npc_world_ready && current.streamed_by_players.contains(viewer.player_id) &&
        SameCharacterScope(current, viewer)) SerializeAndSend(packet, TrafficClass::Control, viewer.connection);
  });
  for (const auto id : summons) {
    // Normal NPC streaming keeps every viewer's replica passive and includes
    // new/late observers automatically. Notify scripts only after full commit.
    QueueNpcAnimation(id, "T_SPAWN");
    const auto generation = npc_manager_.GetNpc(id)->get().actor_generation;
    const auto spell_id = packet.spell_id;
    const auto instance = summon->instance;
    PlayerSummonPacket notification;
    notification.caster_id = caster_id;
    notification.caster_generation = actor_generation;
    notification.npc_id = id;
    notification.npc_generation = generation;
    notification.spell_id = spell_id;
    notification.summon_instance = instance;
    const auto& summoned = npc_manager_.GetNpc(id)->get();
    player_manager_.ForEachIngamePlayer([&](const Player& viewer) {
      if (viewer.npc_world_ready && summoned.streamed_by_players.contains(viewer.player_id) &&
          (viewer.player_id == caster_id || current.streamed_by_players.contains(viewer.player_id)))
        SerializeAndSend(notification, TrafficClass::Control, viewer.connection);
    });
    pending_npc_events_.emplace_back([this, id, generation, caster_id, actor_generation, spell_id, instance] {
      auto npc = npc_manager_.GetNpc(id);
      auto caster = GetCharacter(caster_id);
      if (!npc || npc->get().actor_generation != generation || !caster || caster->get().actor_generation != actor_generation)
        return;
      EventManager::Instance().TriggerEvent(kEventOnNpcCreatedName, OnNpcCreatedEvent{id});
      npc = npc_manager_.GetNpc(id);
      caster = GetCharacter(caster_id);
      if (npc && npc->get().actor_generation == generation && caster && caster->get().actor_generation == actor_generation)
        EventManager::Instance().TriggerEvent(kEventOnPlayerSummonName, OnPlayerSummonEvent{caster_id, spell_id, instance, id});
    });
  }
}

std::vector<GameServer::PlayerId> GameServer::PrepareSpellSummons(const Character& caster, std::uint32_t cast_id,
                                                                  const SummonRegistry::Definition& definition) {
  std::size_t owned = 0;
  for (const auto id : npc_manager_.GetNpcIds()) {
    const auto& npc = npc_manager_.GetNpc(id)->get();
    if (npc.summon && npc.summon->caster_id == caster.player_id && npc.summon->caster_generation == caster.actor_generation)
      ++owned;
  }
  if (owned + definition.count > 12 || npc_manager_.GetNpcCount() + definition.count > 4096)
    return {};

  std::vector<PlayerId> prepared;
  const auto rollback = [&] {
    for (const auto id : prepared) npc_manager_.Remove(id);
  };
  glm::vec3 forward(caster.state.nrot.x, 0.0f, caster.state.nrot.z);
  if (!IsFinitePosition(forward) || glm::length(forward) < 0.01f)
    forward = {0.0f, 0.0f, 1.0f};
  else
    forward = glm::normalize(forward);
  const glm::vec3 right(forward.z, 0.0f, -forward.x);
  for (int index = 0; index < definition.count; ++index) {
    // A bounded formation in front of the caster. Placement uses the normal
    // server NPC path; clients do not create separate native summon entities.
    const auto position =
        caster.state.position + forward * (200.0f + 150.0f * (index / 3)) + right * (150.0f * (index % 3 - (definition.count == 1 ? 0 : 1)));
    if (!IsValidActorPosition(position)) {
      rollback();
      return {};
    }
    const auto id = npc_manager_.Create(definition.instance, definition.instance, caster.world, GetMaxSlots());
    if (!id) {
      rollback();
      return {};
    }
    prepared.push_back(id);
    auto& npc = npc_manager_.GetNpc(id)->get();
    npc.virtual_world = caster.virtual_world;
    npc.max_health = definition.max_health;
    npc.max_mana = 0;
    npc.state.position = position;
    npc.state.nrot = forward;
    npc.summon = NpcManager::Npc::Summon{caster.player_id, caster.actor_generation, cast_id,
                                         NpcManager::Clock::now() + std::chrono::milliseconds(definition.lifetime_ms)};
  }
  return prepared;
}

void GameServer::HandleActorAction(Packet p) {
  ActorActionPacket packet{};
  if (!DeserializeClientPacket(p, packet, GetPacketIdentifier(p), "ActorActionPacket")) return;
  auto sender = GetIngamePlayerByConnection(p.id);
  if (!sender || !sender->get().npc_world_ready) return;
  const auto actor_id = packet.actor_id ? packet.actor_id : sender->get().player_id;
  auto actor = GetCharacter(actor_id);
  if (!actor || packet.actor_generation != actor->get().actor_generation ||
      !CanControlActor(sender->get(), actor->get(), packet.control_epoch)) return;
  auto& current = actor->get();
  const auto now = std::chrono::steady_clock::now();
  if (!CanReportActorAction(sender->get(), current, packet.control_epoch, packet.command_revision)) {
    if (packet.kind == ActorActionKind::ItemUse) {
      SendItemTransactionResult(current, 0, packet.item_instance, false);
      SendPlayerAttributeUpdate(player_manager_, current, ATTR_HEALTH, current.health);
      SendPlayerAttributeUpdate(player_manager_, current, ATTR_MANA, current.mana);
    } else if (packet.kind == ActorActionKind::Shot) {
      if (gmp::IsValidScriptName(packet.item_instance)) SendItemTransactionResult(current, 0, packet.item_instance, false);
    }
    return;
  }
  if (!AcceptActionId(current.last_actor_action_id, packet.action_id)) return;
  if (now - current.action_window_started >= std::chrono::seconds(1)) {
    current.action_window_started = now;
    current.actions_in_window = 0;
  }
  if (++current.actions_in_window > 100) return;
  packet.actor_id = actor_id;
  // Map the host's sample clock onto the same server timeline as actor poses.
  const auto server_time = ActorClockMilliseconds();
  if (sender->get().client_clock_origin && packet.source_time_ms >= *sender->get().client_clock_origin) {
    const auto elapsed = packet.source_time_ms - *sender->get().client_clock_origin;
    if (elapsed > server_time - sender->get().server_clock_origin + 2000) return;
    packet.source_time_ms = sender->get().server_clock_origin + elapsed;
  } else packet.source_time_ms = server_time;
  if (!IsFinitePosition(packet.position) || !IsFinitePosition(packet.direction)) return;

  switch (packet.kind) {
    case ActorActionKind::AnimationStart:
    case ActorActionKind::AnimationStop:
      if (!gmp::IsValidAnimationName(packet.name)) return;
      packet.name = gmp::NormalizeScriptName(packet.name);
      break;
    case ActorActionKind::Shot: {
      if ((packet.weapon_mode != 5 && packet.weapon_mode != 6) || packet.weapon_mode != current.state.weapon_mode ||
          glm::distance(packet.position, current.state.position) > 250.0f) return;
      const auto direction_length = glm::length(packet.direction);
      if (!std::isfinite(direction_length) || direction_length < 0.01f || direction_length > 2.0f) return;
      packet.direction /= direction_length;
      auto bounds = GetCombatBounds(current);
      if (!gmp::IsValidScriptName(packet.item_instance)) return;
      packet.item_instance = gmp::NormalizeScriptName(packet.item_instance);
      if (bounds.damage <= 0 || now - current.last_attack_at < std::chrono::milliseconds(bounds.interval_ms) ||
          !RemoveInventoryItem(current, packet.item_instance, 1)) return;
      current.last_attack_at = now;
      auto approval = Character::ApprovedAttack{packet.action_id, bounds.damage, bounds.types, bounds.reach,
                                                             now + std::chrono::seconds(5), {}, 1};
      approval.origin = packet.position;
      while (current.approved_projectiles.size() >= 64) current.approved_projectiles.pop_front();
      current.approved_projectiles.push_back(std::move(approval));
      SendItemTransactionResult(current, 0, packet.item_instance, true);
      break;
    }
    case ActorActionKind::ItemUse: {
      const auto generation = current.actor_generation;
      const auto health_before_callbacks = current.health;
      const auto mana_before_callbacks = current.mana;
      if (!gmp::IsValidScriptName(packet.item_instance) || packet.scheme.size() > 128 ||
          SanitizeServerText(packet.scheme) != packet.scheme) return;
      packet.item_instance = gmp::NormalizeScriptName(packet.item_instance);
      const auto decision = EventManager::Instance().DispatchEvent(
          kEventOnPlayerUseItemName, OnPlayerUseItemEvent{actor_id, packet.item_instance, packet.quantity, packet.scheme});
      const auto reject_use = [&] {
        const auto actor = GetCharacter(actor_id);
        if (!actor || actor->get().actor_generation != generation) return;
        AdvancePlayerStateSequence(actor->get());
        SendItemTransactionResult(actor->get(), 0, packet.item_instance, false);
        SendPlayerAttributeUpdate(player_manager_, actor->get(), ATTR_HEALTH, actor->get().health);
        SendPlayerAttributeUpdate(player_manager_, actor->get(), ATTR_MANA, actor->get().mana);
      };
      sender = GetIngamePlayerByConnection(p.id);
      actor = GetCharacter(actor_id);
      if (decision.cancelled || !sender || !actor || actor->get().actor_generation != generation ||
          !CanReportActorAction(sender->get(), actor->get(), packet.control_epoch, packet.command_revision) ||
          packet.quantity < 0 || packet.quantity > 32 ||
          actor->get().InventoryCount(packet.item_instance) < std::max(1, packet.quantity)) {
        reject_use();
        return;
      }
      const auto instance = packet.item_instance;
      if (packet.quantity > 0 && !RemoveInventoryItem(actor->get(), instance, packet.quantity)) {
        reject_use();
        return;
      }
      auto& user = actor->get();
      const auto health = user.health == health_before_callbacks
          ? static_cast<std::int32_t>(std::clamp<std::int64_t>(
              static_cast<std::int64_t>(user.health) + packet.health_delta, 0, user.max_health)) : user.health;
      const auto mana = user.mana == mana_before_callbacks
          ? static_cast<std::int32_t>(std::clamp<std::int64_t>(
              static_cast<std::int64_t>(user.mana) + packet.mana_delta, 0, user.max_mana)) : user.mana;
      packet.health_delta = health - health_before_callbacks;
      packet.mana_delta = mana - mana_before_callbacks;
      ClearRemovedEquipment(user, packet.item_instance, user.InventoryCount(instance));
      SendItemTransactionResult(user, 0, packet.item_instance, true);
      SetCharacterHealth(actor_id, health);
      actor = GetCharacter(actor_id);
      if (!actor || actor->get().actor_generation != generation) return;
      SetCharacterMana(actor_id, mana);
      break;
    }
    case ActorActionKind::MobState: {
      if (actor_id != sender->get().player_id || packet.name.empty() || packet.name.size() > 128 ||
          SanitizeServerText(packet.name) != packet.name || packet.target_state < 0 || packet.target_state > 64 ||
          glm::distance(packet.position, current.state.position) > 400.0f) return;
      const auto record = std::find_if(mob_states_.begin(), mob_states_.end(), [&](const MobStateRecord& value) {
        return value.world == current.world && value.virtual_world == current.virtual_world && value.name == packet.name &&
               glm::distance(value.position, packet.position) < 50.0f;
      });
      if (record == mob_states_.end()) {
        if (mob_states_.size() >= 4096) return;
        mob_states_.push_back({current.world, current.virtual_world, packet.name, packet.position, packet.target_state});
      } else record->state = packet.target_state;
      player_manager_.ForEachIngamePlayer([&](const Player& viewer) {
        if (viewer.connection != p.id && viewer.npc_world_ready && SameCharacterScope(current, viewer)) {
          packet.target_generation = viewer.actor_generation;
          SerializeAndSend(packet, TrafficClass::Control, viewer.connection);
        }
      });
      return;
    }
    default: return;
  }
  // Script events can replace the actor, disconnect it or change its world.
  actor = GetCharacter(actor_id);
  if (!actor || actor->get().actor_generation != packet.actor_generation || !IsLiveCharacter(actor->get())) return;
  player_manager_.ForEachIngamePlayer([&](const Player& viewer) {
    if (viewer.connection != p.id && viewer.npc_world_ready && actor->get().streamed_by_players.contains(viewer.player_id) &&
        SameCharacterScope(actor->get(), viewer)) SerializeAndSend(packet, TrafficClass::Control, viewer.connection);
  });
}

void GameServer::ReplayActorActions(Player& viewer) {
  if (!viewer.npc_world_ready) return;
  for (const auto& record : mob_states_) {
    if (record.world != viewer.world || record.virtual_world != viewer.virtual_world) continue;
    ActorActionPacket packet{};
    packet.kind = ActorActionKind::MobState;
    packet.target_generation = viewer.actor_generation;
    packet.name = record.name;
    packet.position = record.position;
    packet.target_state = record.state;
    packet.source_time_ms = ActorClockMilliseconds();
    SerializeAndSend(packet, TrafficClass::Control, viewer.connection);
  }
}

void GameServer::ProcessActorEffects() {
  const auto now = std::chrono::steady_clock::now();
  std::vector<std::pair<PlayerId, std::string>> expired;
  auto collect = [&](const Character& character) {
    for (const auto& [overlay, deadline] : character.timed_overlays) {
      if (deadline <= now) expired.emplace_back(character.player_id, overlay);
    }
  };
  for (const auto& [_, player] : player_manager_.GetAllPlayers()) collect(player);
  for (const auto id : npc_manager_.GetNpcIds()) if (const auto npc = npc_manager_.GetNpc(id)) collect(npc->get());
  for (const auto& [id, overlay] : expired) {
    auto actor = GetCharacter(id);
    if (!actor) continue;
    actor->get().timed_overlays.erase(overlay);
    RemoveCharacterOverlay(id, overlay);
  }
}

void GameServer::DispatchPendingActorEvents() {
  auto events = std::move(pending_npc_events_);
  pending_npc_events_.clear();
  for (auto& event : events) event();
}

void GameServer::HandleDropItem(Packet p) {
  auto player_opt = GetIngamePlayerByConnection(p.id);
  if (!player_opt.has_value()) {
    return;
  }

  auto& player = player_opt.value().get();

  DropItemPacket packet;
  if (!DeserializeClientPacket(p, packet, GetPacketIdentifier(p), "DropItemPacket")) {
    return;
  }

  const auto item_instance = ResolveItemInstance(packet.item_instance);
  if (item_instance) packet.item_instance = *item_instance;
  const bool request_is_new = packet.actor_generation == player.actor_generation && player.AcceptItemRequest(packet.request_id);
  // Native mutations are speculative until this transfer commits. Every validly
  // decoded request receives the class's authoritative resulting count.
  if (!request_is_new || !item_instance || !player.inventory_initialized || !player.npc_world_ready ||
      player.state.life_state != PLAYER_LIFE_ALIVE || packet.item_amount <= 0 ||
      !IsFinitePosition(packet.position) || !IsFinitePosition(packet.rotation) ||
      glm::distance(packet.position, player.state.position) > 400.0f ||
      player.InventoryCount(packet.item_instance) < packet.item_amount) {
    SendItemTransactionResult(player, packet.request_id, packet.item_instance, false);
    return;
  }
  const auto instance = packet.item_instance;
  const auto amount = packet.item_amount;
  const auto player_id = player.player_id;
  const auto actor_generation = player.actor_generation;
  // Reserve ownership before exposing the object to a re-entrant script.
  if (!RemoveInventoryItem(player, instance, amount)) {
    SendItemTransactionResult(player, packet.request_id, packet.item_instance, false);
    return;
  }
  ItemGroundManager::CreateOptions options;
  options.instance = instance;
  options.amount = amount;
  options.physics_enabled = packet.physics_enabled;
  options.position = packet.position;
  options.rotation = packet.rotation;
  options.world = player.world;
  options.virtual_world = player.virtual_world;

  const auto item_ground_id = CreateItemGround(std::move(options));
  auto result = EventManager::Instance().DispatchEvent(kEventOnPlayerDropItemName, OnPlayerDropItemEvent{player_id, item_ground_id});
  const auto current_player = GetIngamePlayerByConnection(p.id);
  if (!current_player || current_player->get().actor_generation != actor_generation) {
    DestroyItemGround(item_ground_id);
    return;
  }
  auto& current = current_player->get();
  if (result.cancelled || item_ground_id == 0 || !item_ground_manager_.Get(item_ground_id)) {
    DestroyItemGround(item_ground_id);
    AddInventoryItem(current, instance, amount);
    SendItemTransactionResult(current, packet.request_id, packet.item_instance, false);
    return;
  }
  ClearRemovedEquipment(current, packet.item_instance, current.InventoryCount(instance));
  SendItemTransactionResult(current, packet.request_id, packet.item_instance, true, item_ground_id);
}

void GameServer::HandleTakeItem(Packet p) {
  auto player_opt = GetIngamePlayerByConnection(p.id);
  if (!player_opt.has_value()) {
    return;
  }

  auto& player = player_opt.value().get();

  TakeItemPacket packet;
  if (!DeserializeClientPacket(p, packet, GetPacketIdentifier(p), "TakeItemPacket")) {
    return;
  }

  const bool request_is_new = packet.actor_generation == player.actor_generation && player.AcceptItemRequest(packet.request_id);
  auto* item_ground = packet.item_ground_id ? item_ground_manager_.Get(*packet.item_ground_id) : nullptr;
  const auto item_instance = ResolveItemInstance(packet.item_instance);
  if (item_instance) packet.item_instance = *item_instance;
  if (!request_is_new || !player.inventory_initialized || !player.npc_world_ready ||
      player.state.life_state != PLAYER_LIFE_ALIVE || !item_ground || !item_instance ||
      item_ground->instance != packet.item_instance || packet.item_amount != item_ground->amount ||
      !CanSeeItemGround(player, *item_ground) || !item_ground->streamed_to.contains(player.player_id) ||
      glm::distance(item_ground->position, player.state.position) > 400.0f ||
      !item_ground_manager_.Reserve(item_ground->id, player.player_id)) {
    if (item_ground && CanSeeItemGround(player, *item_ground)) SendItemGroundCreate(*item_ground, player.connection);
    SendItemTransactionResult(player, packet.request_id, packet.item_instance, false, packet.item_ground_id);
    return;
  }

  const auto instance = item_ground->instance;
  const auto amount = item_ground->amount;
  const auto ground_id = item_ground->id;
  const auto player_id = player.player_id;
  const auto actor_generation = player.actor_generation;
  auto result = EventManager::Instance().DispatchEvent(kEventOnPlayerTakeItemName, OnPlayerTakeItemEvent{player_id, ground_id});
  // Both entities can be destroyed or moved by Lua. Never retain pointers across
  // the callback, and never transfer a replacement/mutated ground item.
  const auto current_player = GetIngamePlayerByConnection(p.id);
  item_ground = item_ground_manager_.Get(ground_id);
  if (!current_player || current_player->get().actor_generation != actor_generation) {
    item_ground_manager_.ReleaseReservation(ground_id, player_id);
    return;
  }
  auto& current = current_player->get();
  const bool unchanged_item = item_ground && item_ground->reserved_by == player_id && item_ground->instance == instance &&
                              item_ground->amount == amount && CanSeeItemGround(current, *item_ground) &&
                              glm::distance(item_ground->position, current.state.position) <= 400.0f;
  if (result.cancelled || !unchanged_item || !current.npc_world_ready || current.state.life_state != PLAYER_LIFE_ALIVE ||
      !AddInventoryItem(current, instance, amount)) {
    item_ground_manager_.ReleaseReservation(ground_id, player_id);
    if (item_ground && CanSeeItemGround(current, *item_ground)) SendItemGroundCreate(*item_ground, current.connection);
    SendItemTransactionResult(current, packet.request_id, packet.item_instance, false, ground_id);
    return;
  }
  DestroyItemGround(ground_id);
  SendItemTransactionResult(current, packet.request_id, packet.item_instance, true, ground_id);
}

void GameServer::HandlePlayerWorldEnter(Packet p) {
  auto player_opt = GetIngamePlayerByConnection(p.id);
  if (!player_opt.has_value()) {
    return;
  }

  auto& player = player_opt.value().get();

  PlayerWorldEnterPacket packet;
  if (!DeserializeClientPacket(p, packet, GetPacketIdentifier(p), "PlayerWorldEnterPacket")) {
    return;
  }

  const auto entered_world = SanitizeWorldName(packet.world_name);
  if (!entered_world.empty() && entered_world != player.world) {
    SPDLOG_WARN("Player {} reported entering world '{}', but server state is '{}'", player.player_id, entered_world,
                player.world);
    return;
  }
  if (entered_world.empty()) {
    return;
  }
  if (packet.actor_generation != player.actor_generation || player.npc_world_ready) return;
  if (!InitializePlayerInventory(player, packet.inventory)) return;
  player.npc_world_ready = true;

  EventManager::Instance().TriggerEvent(kEventOnPlayerWorldEnterName, OnPlayerWorldEnterEvent{player.player_id, player.world});
  // A script can redirect the player into another world, unspawn or disconnect
  // them from this callback. Re-check eligibility before using the result.
  const auto ready_player = GetIngamePlayerByConnection(p.id);
  if (!ready_player || !ready_player->get().npc_world_ready || ready_player->get().world != entered_world) {
    return;
  }
  RefreshPlayerStreaming();
  for (const auto npc_id : npc_manager_.GetNpcIds()) {
    RefreshNpcStreaming(npc_id);
  }
  StreamRelevantGroundItemsToPlayer(ready_player->get(), true);
  // Grants and commands issued while the world was loading may have reached a
  // discarded native hero. Reconcile canonical counts after readiness rather
  // than interpreting a later GiveItem as an acknowledgement of a pending take.
  auto& canonical = ready_player->get();
  for (const auto& [instance, amount] : canonical.inventory) {
    SendItemTransactionResult(canonical, 0, instance, true);
  }
  for (const auto& entry : packet.inventory) {
    if (canonical.InventoryCount(entry.item_instance) == 0)
      SendItemTransactionResult(canonical, 0, entry.item_instance, false);
  }
  ValidatePlayerEquipment(canonical, canonical.state);
  AdvanceActorCommand(canonical);
  AdvancePlayerStateSequence(canonical);
  AdvancePlayerStateSequence(canonical);
  auto equipment = MakePlayerStateUpdatePacket(canonical);
  equipment.state.correction_mask = kSyncEquipment | kSyncCombat;
  BroadcastToRelevant(player_manager_, canonical, equipment, TrafficClass::Control);
  ReplayActorActions(ready_player->get());
}

void GameServer::SendItemGroundCreate(const ItemGroundManager::ItemGround& item_ground, Net::ConnectionHandle connection) {

  ItemGroundCreatePacket packet{};
  packet.packet_type = PT_ITEM_GROUND_CREATE;
  packet.item_ground_id = item_ground.id;
  packet.item_instance = item_ground.instance;
  packet.amount = item_ground.amount;
  packet.physics_enabled = item_ground.physics_enabled;
  packet.position = item_ground.position;
  packet.rotation = item_ground.rotation;
  // Item mutations must stay ordered with world transitions. They carry no
  // world epoch that could make an independently delivered item snapshot safe.
  SerializeAndSend(packet, TrafficClass::Control, connection);
}

void GameServer::SendItemGroundDestroy(std::uint32_t item_ground_id, Net::ConnectionHandle connection) {
  ItemGroundDestroyPacket packet{};
  packet.packet_type = PT_ITEM_GROUND_DESTROY;
  packet.item_ground_id = item_ground_id;
  SerializeAndSend(packet, TrafficClass::Control, connection);
}

void GameServer::SendItemGroundClear(Net::ConnectionHandle connection) {
  ItemGroundClearPacket packet{};
  packet.packet_type = PT_ITEM_GROUND_CLEAR;
  SerializeAndSend(packet, TrafficClass::Control, connection);
}

void GameServer::StreamItemGroundToPlayer(ItemGroundManager::ItemGround& item_ground, Player& player, bool force) {
  if (!CanSeeItemGround(player, item_ground)) {
    return;
  }

  const bool newly_streamed = item_ground.streamed_to.insert(player.player_id).second;
  if (newly_streamed || force) {
    SendItemGroundCreate(item_ground, player.connection);
  }
}

void GameServer::StreamRelevantGroundItemsToPlayer(Player& player, bool force) {
  for (auto& [_, item_ground] : item_ground_manager_.Items()) {
    StreamItemGroundToPlayer(item_ground, player, force);
  }
}

void GameServer::UnstreamGroundItemsFromPlayer(Player& player, bool notify_client) {
  bool had_streamed_items = false;
  for (auto& [_, item_ground] : item_ground_manager_.Items()) {
    had_streamed_items = item_ground.streamed_to.erase(player.player_id) > 0 || had_streamed_items;
  }

  if (notify_client && had_streamed_items && player.is_ingame) {
    SendItemGroundClear(player.connection);
  }
}

void GameServer::RefreshItemGroundStreaming(ItemGroundManager::ItemGround& item_ground) {
  std::vector<PlayerId> stale_viewers;
  stale_viewers.reserve(item_ground.streamed_to.size());

  for (const auto player_id : item_ground.streamed_to) {
    auto player_opt = player_manager_.GetPlayer(player_id);
    if (!player_opt.has_value() || !CanSeeItemGround(player_opt->get(), item_ground)) {
      stale_viewers.push_back(player_id);
    } else {
      SendItemGroundCreate(item_ground, player_opt->get().connection);
    }
  }

  for (const auto player_id : stale_viewers) {
    item_ground.streamed_to.erase(player_id);
    auto player_opt = player_manager_.GetPlayer(player_id);
    if (player_opt.has_value() && player_opt->get().is_ingame) {
      SendItemGroundDestroy(item_ground.id, player_opt->get().connection);
    }
  }

  player_manager_.ForEachIngamePlayer([&](Player& player) { StreamItemGroundToPlayer(item_ground, player); });
}

bool GameServer::InitializePlayerInventory(Player& player, const std::vector<InventoryEntry>& inventory) {
  if (!ValidateNativeInventoryDeclaration(inventory)) return false;
  player.inventory_initialized = true;
  ValidatePlayerEquipment(player, player.state);
  return true;
}

void GameServer::SendItemTransactionResult(Character& player, std::uint32_t request_id, const std::string& item_instance,
                                         bool accepted, std::optional<std::uint32_t> item_ground_id) {
  AdvancePlayerStateSequence(player);
  ItemTransactionResultPacket packet{};
  packet.packet_type = PT_ITEM_TRANSACTION_RESULT;
  packet.actor_id = player.player_id;
  packet.request_id = request_id;
  packet.item_instance = item_instance;
  packet.resulting_amount = player.InventoryCount(item_instance);
  packet.accepted = accepted;
  packet.item_ground_id = item_ground_id;
  packet.actor_generation = player.actor_generation;
  packet.state_sequence = player.state_sequence;
  if (const auto connection = player_manager_.GetConnectionHandle(player.player_id))
    SerializeAndSend(packet, TrafficClass::Control, *connection);
  else BroadcastToRelevant(player_manager_, player, packet, TrafficClass::Control);
}

std::optional<std::string> GameServer::ResolveItemInstance(std::string instance) const {
  if (!gmp::IsValidScriptName(instance)) return std::nullopt;
  return gmp::NormalizeScriptName(instance);
}

void GameServer::NormalizePlayerStateInstances(PlayerState& state) const {
  state.left_hand_item_instance = ResolveItemInstance(state.left_hand_item_instance).value_or(std::string{});
  state.right_hand_item_instance = ResolveItemInstance(state.right_hand_item_instance).value_or(std::string{});
  state.equipped_armor_instance = ResolveItemInstance(state.equipped_armor_instance).value_or(std::string{});
  state.equipped_helmet_instance = ResolveItemInstance(state.equipped_helmet_instance).value_or(std::string{});
  state.equipped_shield_instance = ResolveItemInstance(state.equipped_shield_instance).value_or(std::string{});
  state.equipped_amulet_instance = ResolveItemInstance(state.equipped_amulet_instance).value_or(std::string{});
  state.equipped_belt_instance = ResolveItemInstance(state.equipped_belt_instance).value_or(std::string{});
  state.equipped_ring_left_instance = ResolveItemInstance(state.equipped_ring_left_instance).value_or(std::string{});
  state.equipped_ring_right_instance = ResolveItemInstance(state.equipped_ring_right_instance).value_or(std::string{});
  state.active_spell_instance = ResolveItemInstance(state.active_spell_instance).value_or(std::string{});
  state.melee_weapon_instance = ResolveItemInstance(state.melee_weapon_instance).value_or(std::string{});
  state.ranged_weapon_instance = ResolveItemInstance(state.ranged_weapon_instance).value_or(std::string{});
}

std::uint32_t GameServer::CreateItemGround(ItemGroundManager::CreateOptions options) {
  if (options.amount <= 0 || !IsFinitePosition(options.position) || !IsFinitePosition(options.rotation)) return 0;
  auto resolved_instance = ResolveItemInstance(options.instance);
  if (!resolved_instance.has_value()) {
    SPDLOG_WARN("Cannot create ground item for unknown item instance '{}'", SanitizeServerText(options.instance));
    return 0;
  }
  options.instance = *resolved_instance;
  options.world = SanitizeWorldName(options.world.empty() ? server_world_ : options.world);
  options.virtual_world = std::clamp<std::int32_t>(options.virtual_world, 0, 65535);

  auto& item_ground = item_ground_manager_.Create(std::move(options));
  RefreshItemGroundStreaming(item_ground);
  return item_ground.id;
}

bool GameServer::DestroyItemGround(std::uint32_t item_ground_id) {
  auto* item_ground = item_ground_manager_.Get(item_ground_id);
  if (item_ground == nullptr) {
    return false;
  }

  const auto streamed_to = item_ground->streamed_to;
  for (const auto player_id : streamed_to) {
    auto player_opt = player_manager_.GetPlayer(player_id);
    if (player_opt.has_value() && player_opt->get().is_ingame) {
      SendItemGroundDestroy(item_ground_id, player_opt->get().connection);
    }
  }

  return item_ground_manager_.Destroy(item_ground_id);
}

bool GameServer::SetItemGroundPosition(std::uint32_t item_ground_id, const glm::vec3& position) {
  if (!IsFinitePosition(position)) return false;
  auto* item_ground = item_ground_manager_.Get(item_ground_id);
  if (item_ground == nullptr) {
    return false;
  }

  item_ground->position = position;
  RefreshItemGroundStreaming(*item_ground);
  return true;
}

bool GameServer::SetItemGroundRotation(std::uint32_t item_ground_id, const glm::vec3& rotation) {
  if (!IsFinitePosition(rotation)) return false;
  auto* item_ground = item_ground_manager_.Get(item_ground_id);
  if (item_ground == nullptr) {
    return false;
  }

  item_ground->rotation = rotation;
  RefreshItemGroundStreaming(*item_ground);
  return true;
}

bool GameServer::SetItemGroundVirtualWorld(std::uint32_t item_ground_id, std::int32_t virtual_world) {
  auto* item_ground = item_ground_manager_.Get(item_ground_id);
  if (item_ground == nullptr) {
    return false;
  }

  item_ground->virtual_world = std::clamp<std::int32_t>(virtual_world, 0, 65535);
  RefreshItemGroundStreaming(*item_ground);
  return true;
}

bool GameServer::SetItemGroundPhysicsEnabled(std::uint32_t item_ground_id, bool enabled) {
  auto* item_ground = item_ground_manager_.Get(item_ground_id);
  if (item_ground == nullptr) {
    return false;
  }

  item_ground->physics_enabled = enabled;
  RefreshItemGroundStreaming(*item_ground);
  return true;
}

nlohmann::json GameServer::BuildMasterServerPayload() const {
  const auto port = g_transport ? g_transport->GetPort() : static_cast<std::uint32_t>(config_.Get<std::int32_t>("port"));
  const auto ip_address = g_transport ? g_transport->GetAddress() : std::string{};

  const auto name = SanitizeServerText(config_.Get<std::string>("name"));
  const auto player_count = static_cast<std::uint32_t>(player_manager_.GetPlayerCount());
  const auto max_slots = static_cast<std::uint32_t>(config_.Get<std::int32_t>("slots"));
  const auto map = SanitizeServerText(config_.Get<std::string>("map"));

  return nlohmann::json{{"server_seed", config_.Get<std::string>("server_identity_seed")},
                        {"ip_address", ip_address},
                        {"port", port},
                        {"name", name},
                        {"current_players", player_count},
                        {"max_slots", max_slots},
                        {"map", map}};
}

void GameServer::AddToPublicListHTTP() {
  using namespace std::chrono_literals;

  auto endpoint_info_opt = master_server::ParseEndpoint(kMasterServerEndpoint);
  if (!endpoint_info_opt) {
    SPDLOG_WARN("Master server endpoint must be a valid HTTPS URL. Skipping public list updates.");
    return;
  }

  const auto endpoint_info = *endpoint_info_opt;

  auto client = master_server::CreateClient(endpoint_info);
  if (!client) {
    SPDLOG_ERROR("Unable to create HTTPS client for master server endpoint '{}'. Check OpenSSL support.", kMasterServerEndpoint);
    return;
  }

  client->set_connection_timeout(5, 0);
  client->set_read_timeout(5, 0);
  client->set_write_timeout(5, 0);

  auto last_update = std::chrono::system_clock::now() - 15s;

  while (g_is_server_running) {
    auto now = std::chrono::system_clock::now();
    if (now - last_update >= 15s) {
      last_update = now;
      auto payload = BuildMasterServerPayload();

      auto response = client->Post(endpoint_info.path.c_str(), payload.dump(), "application/json");
      if (!response) {
        SPDLOG_WARN("Failed to update master server at {}:{}{}: {}", endpoint_info.host, endpoint_info.port, endpoint_info.path,
                    httplib::to_string(response.error()));
      } else if (response->status < 200 || response->status >= 300) {
        SPDLOG_WARN("Master server responded with status {} when updating {}:{}{}", response->status, endpoint_info.host, endpoint_info.port,
                    endpoint_info.path);
      } else {
        SPDLOG_DEBUG("Master server heartbeat succeeded with status {}", response->status);
      }
    }
    std::this_thread::sleep_for(100ms);
  }
}

void GameServer::HandleGameInfo(Packet p) {
  SendGameInfo(p.id);
  SendSkySettings(p.id);
}

// void GameServer::HandleGameInfo(Packet p){
void GameServer::SendGameInfo(Net::ConnectionHandle who) {
  SendGameInfo(who, clock_->GetTime());
}

void GameServer::SendGameInfo(Net::ConnectionHandle who, GothicClock::Time time) {
  GameInfoPacket packet;
  packet.packet_type = PT_GAME_INFO;
  GothicClock::TimeUnion game_time = time;
  packet.raw_game_time = game_time.raw;
  packet.day_length_ms = static_cast<float>(clock_->GetDayLengthMs());

  if (config_.Get<bool>("hide_map")) {
    packet.flags |= HIDE_MAP;
  }

  SerializeAndSend(packet, TrafficClass::Control, who);
}

void GameServer::SendSkySettings(Net::ConnectionHandle connection) {
  const auto& state = weather_.GetState();
  auto packet = MakeSkySettingsPacket(kFullSkySettingsFlags, state.weather_type, state.rain_start_hour, state.rain_start_min,
                                      state.rain_stop_hour, state.rain_stop_min, state.wind_scale, state.dont_rain, state.rain_weight,
                                      state.render_lightning);

  SerializeAndSend(packet, TrafficClass::Control, connection);
}

void GameServer::BroadcastGameInfo() {
  BroadcastGameInfo(clock_->GetTime());
}

void GameServer::BroadcastGameInfo(GothicClock::Time time) {
  player_manager_.ForEachIngamePlayer([&](const Player& player) { SendGameInfo(player.connection, time); });
}

void GameServer::BroadcastSkySettings() {
  const auto& state = weather_.GetState();
  auto packet = MakeSkySettingsPacket(kFullSkySettingsFlags, state.weather_type, state.rain_start_hour, state.rain_start_min,
                                      state.rain_stop_hour, state.rain_stop_min, state.wind_scale, state.dont_rain, state.rain_weight,
                                      state.render_lightning);

  player_manager_.ForEachIngamePlayer([&](const Player& player) { SerializeAndSend(packet, TrafficClass::Control, player.connection); });
}

void GameServer::BroadcastPlayerPresence() {
  PlayerPresencePacket packet;
  packet.packet_type = PT_PLAYER_PRESENCE;
  packet.players.reserve(player_manager_.GetPlayerCount());

  player_manager_.ForEachIngamePlayer([&](const Player& player) {
    PlayerPresenceEntry entry;
    entry.player_id = player.player_id;
    entry.ping = GetPlayerPing(player.player_id);
    entry.name = player.name;
    entry.world = player.world;
    entry.position = player.state.position;
    entry.r = player.name_color_r;
    entry.g = player.name_color_g;
    entry.b = player.name_color_b;
    packet.players.push_back(std::move(entry));
  });

  if (packet.players.empty()) {
    return;
  }

  player_manager_.ForEachIngamePlayer([&](const Player& player) { SerializeAndSend(packet, TrafficClass::Control, player.connection); });
}

void GameServer::UpdateAuthoritativeWorldState(const std::vector<GothicClock::Time>& advanced_times) {
  if (advanced_times.empty()) {
    return;
  }

  for (const auto& current_time : advanced_times) {
    const auto current_minute = GothicWeather::ToTotalGameMinutes(current_time);
    if (current_minute == last_weather_update_minute_) {
      continue;
    }

    last_weather_update_minute_ = current_minute;
    BroadcastGameInfo(current_time);

    if (weather_.Update(current_time)) {
      BroadcastSkySettings();
    }
  }
}

void GameServer::HandleMapNameReq(Packet p) {
}

void GameServer::SendDisconnectionInfo(PlayerId disconnected_player_id) {
  DisconnectionInfoPacket packet;
  packet.disconnected_id = disconnected_player_id;
  packet.packet_type = PT_LEFT_GAME;

  player_manager_.ForEachIngamePlayer([&](const Player& player) {
    if (player.player_id != disconnected_player_id) {
      SerializeAndSend(packet, TrafficClass::Control, player.connection);
    }
  });
}

bool GameServer::IsPublic() {
  return config_.Get<bool>("public");
}

void GameServer::SendMessageToAll(std::uint8_t r, std::uint8_t g, std::uint8_t b, const std::string& text) {
  if (text.empty()) {
    return;
  }

  auto packet = CreateMessagePacket(std::nullopt, std::nullopt, r, g, b, text);
  player_manager_.ForEachIngamePlayer([&](const Player& player) { SerializeAndSend(packet, TrafficClass::Control, player.connection); });
}

void GameServer::SendMessageToPlayer(PlayerId player_id, std::uint8_t r, std::uint8_t g, std::uint8_t b, const std::string& text) {
  if (text.empty()) {
    return;
  }

  auto target_player = player_manager_.GetPlayer(player_id);
  if (!target_player.has_value() || !target_player->get().is_ingame) {
    SPDLOG_WARN("Cannot send message to player {} because they are not connected", player_id);
    return;
  }

  auto packet = CreateMessagePacket(std::nullopt, player_id, r, g, b, text);
  SerializeAndSend(packet, TrafficClass::Control, target_player->get().connection);
}

void GameServer::SendPlayerMessageToAll(PlayerId sender_id, std::uint8_t r, std::uint8_t g, std::uint8_t b, const std::string& text) {
  if (text.empty()) {
    return;
  }

  auto sender = player_manager_.GetPlayer(sender_id);
  if (!sender.has_value() || !sender->get().is_ingame) {
    SPDLOG_WARN("Cannot broadcast message from invalid sender {}", sender_id);
    return;
  }

  auto packet = CreateMessagePacket(sender_id, std::nullopt, r, g, b, text);
  player_manager_.ForEachIngamePlayer([&](const Player& player) { SerializeAndSend(packet, TrafficClass::Control, player.connection); });
}

void GameServer::SendPlayerMessageToPlayer(PlayerId sender_id, PlayerId receiver_id, std::uint8_t r, std::uint8_t g, std::uint8_t b,
                                           const std::string& text) {
  if (text.empty()) {
    return;
  }

  auto sender = player_manager_.GetPlayer(sender_id);
  if (!sender.has_value() || !sender->get().is_ingame) {
    SPDLOG_WARN("Cannot send player message from invalid sender {}", sender_id);
    return;
  }

  auto receiver = player_manager_.GetPlayer(receiver_id);
  if (!receiver.has_value() || !receiver->get().is_ingame) {
    SPDLOG_WARN("Cannot send player message to invalid receiver {}", receiver_id);
    return;
  }

  auto packet = CreateMessagePacket(sender_id, receiver_id, r, g, b, text);
  SerializeAndSend(packet, TrafficClass::Control, receiver->get().connection);
}

bool GameServer::TriggerClientEvent(const std::vector<PlayerId>& targets, const std::string& event_name, PlayerId source_element,
                                    const std::string& payload) {
  if (!g_transport) {
    SPDLOG_WARN("Cannot trigger client event '{}' before network server is initialized", event_name);
    return false;
  }

  if (payload.size() > kMaxLuaEventPayloadSize) {
    SPDLOG_WARN("Lua event '{}' payload too large ({} bytes)", event_name, payload.size());
    return false;
  }

  LuaEventPacket packet;
  packet.packet_type = PT_LUA_EVENT;
  packet.event_name = event_name;
  packet.source_element = source_element;
  packet.payload.assign(payload.begin(), payload.end());

  bool sent = false;
  for (auto player_id : targets) {
    auto connection = player_manager_.GetConnectionHandle(player_id);
    if (!connection.has_value()) {
      SPDLOG_WARN("triggerClientEvent: player {} is not connected", player_id);
      continue;
    }
    sent = SerializeAndSend(packet, TrafficClass::Control, connection.value()) || sent;
  }

  return sent;
}

void GameServer::SendDeathInfo(PlayerId dead_player_id) {
  PlayerDeathInfoPacket packet;
  packet.packet_type = PT_DODIE;
  packet.player_id = dead_player_id;

  if (const auto subject = GetCharacter(dead_player_id)) {
    packet.state_sequence = subject->get().state_sequence;
    packet.actor_generation = subject->get().actor_generation;
    packet.command_revision = subject->get().command_revision;
    BroadcastToRelevant(player_manager_, subject->get(), packet, TrafficClass::Control);
  }
}

void GameServer::SendRespawnInfo(PlayerId respawned_player_id) {
  PlayerRespawnInfoPacket packet;
  packet.packet_type = PT_RESPAWN;
  packet.player_id = respawned_player_id;

  if (const auto subject = GetCharacter(respawned_player_id)) {
    packet.state_sequence = subject->get().state_sequence;
    packet.actor_generation = subject->get().actor_generation;
    packet.command_revision = subject->get().command_revision;
    packet.health = subject->get().health;
    packet.mana = subject->get().mana;
    BroadcastToRelevant(player_manager_, subject->get(), packet, TrafficClass::Control);
  }
}

void GameServer::SendUnconsciousInfo(PlayerId player_id, std::optional<PlayerId> attacker_id) {
  PlayerUnconsciousPacket packet;
  packet.packet_type = PT_PLAYER_UNCONSCIOUS;
  packet.player_id = player_id;
  packet.attacker_id = attacker_id;

  if (const auto subject = GetCharacter(player_id)) {
    packet.state_sequence = subject->get().state_sequence;
    packet.actor_generation = subject->get().actor_generation;
    packet.command_revision = subject->get().command_revision;
    BroadcastToRelevant(player_manager_, subject->get(), packet, TrafficClass::Control);
  }
}

void GameServer::SendStandUpInfo(PlayerId player_id) {
  PlayerStandUpPacket packet;
  packet.packet_type = PT_PLAYER_STANDUP;
  packet.player_id = player_id;

  if (const auto subject = GetCharacter(player_id)) {
    packet.state_sequence = subject->get().state_sequence;
    packet.actor_generation = subject->get().actor_generation;
    packet.command_revision = subject->get().command_revision;
    BroadcastToRelevant(player_manager_, subject->get(), packet, TrafficClass::Control);
  }
}

void GameServer::BroadcastPlayerJoined(const Player& joining_player) {
  JoinGamePacket packet;
  packet.packet_type = PT_JOIN_GAME;
  packet.actor_generation = joining_player.actor_generation;
  packet.position = joining_player.state.position;
  packet.normal = joining_player.state.nrot;
  packet.left_hand_item_instance = joining_player.state.left_hand_item_instance;
  packet.right_hand_item_instance = joining_player.state.right_hand_item_instance;
  packet.equipped_armor_instance = joining_player.state.equipped_armor_instance;
  packet.equipped_helmet_instance = joining_player.state.equipped_helmet_instance;
  packet.equipped_shield_instance = joining_player.state.equipped_shield_instance;
  packet.equipped_amulet_instance = joining_player.state.equipped_amulet_instance;
  packet.equipped_belt_instance = joining_player.state.equipped_belt_instance;
  packet.equipped_ring_left_instance = joining_player.state.equipped_ring_left_instance;
  packet.equipped_ring_right_instance = joining_player.state.equipped_ring_right_instance;
  packet.animation_name = joining_player.state.animation_name;
  packet.body_model = joining_player.body_model;
  packet.body_texture = joining_player.body_texture;
  packet.head_model = joining_player.head_model;
  packet.head_texture = joining_player.head_texture;
  packet.teeth_texture = joining_player.teeth_texture;
  packet.skin_color = joining_player.skin_color;
  packet.walk_style = joining_player.walkstyle;
  packet.player_name = joining_player.name;
  for (const auto& overlay : joining_player.overlays) {
    if (const auto walking = gmp::NormalizeCharacterWalkingOverlay(overlay); walking && !walking->empty())
      packet.walking_overlay = *walking;
  }
  packet.player_id = joining_player.player_id;

  player_manager_.ForEachPlayer([&](const Player& existing_player) {
    if (existing_player.player_id == joining_player.player_id) {
      return;
    }
    if (!existing_player.is_ingame) {
      return;
    }
    if (!IsSameVisibilityScope(existing_player, joining_player)) {
      return;
    }
    SerializeAndSend(packet, TrafficClass::Control, existing_player.connection);
  });
}

void GameServer::RefreshPlayerStreaming(bool send_state_updates) {
  const auto radius = static_cast<float>(GetStreamerRadius());
  const auto height = static_cast<float>(GetStreamerHeight());
  std::vector<Player*> players;
  players.reserve(player_manager_.GetPlayerCount());
  player_manager_.ForEachIngamePlayer([&](Player& player) { players.push_back(&player); });

  const auto stream_subject_to_viewer = [send_state_updates](Player& subject, Player& viewer) {
    if (subject.streamed_by_players.insert(viewer.player_id).second) {
      viewer.spawned_players.insert(subject.player_id);
      SerializeAndSend(MakePlayerSpawnPacket(subject), TrafficClass::Control, viewer.connection);
    } else if (send_state_updates) {
      auto& sent = viewer.sent_actor_states[subject.player_id];
      const auto now = std::chrono::steady_clock::now();
      if (sent.generation == subject.actor_generation && sent.sequence == subject.state_sequence &&
          now - sent.sent_at < std::chrono::seconds(1)) return;
      SerializeAndSend(MakePlayerStateUpdatePacket(subject), TrafficClass::State, viewer.connection);
      sent = {subject.actor_generation, subject.state_sequence, now};
    }
  };

  for (std::size_t i = 0; i < players.size(); ++i) {
    for (std::size_t j = i + 1; j < players.size(); ++j) {
      auto& first = *players[i];
      auto& second = *players[j];
      // Remote actors must not be instantiated in a world the viewer is still
      // loading. Both sides enter through the same complete spawn snapshot.
      if (first.npc_world_ready && second.npc_world_ready && IsInsideStreamRange(first, second, radius, height)) {
        stream_subject_to_viewer(first, second);
        stream_subject_to_viewer(second, first);
      } else {
        StreamOutSubjectFromViewer(first, second);
        StreamOutSubjectFromViewer(second, first);
      }
    }
  }
}

std::optional<std::reference_wrapper<Character>> GameServer::GetCharacter(PlayerId id) {
  if (auto player = player_manager_.GetPlayer(id)) {
    return std::ref(static_cast<Character&>(player->get()));
  }
  if (auto npc = npc_manager_.GetNpc(id)) {
    return std::ref(static_cast<Character&>(npc->get()));
  }
  return std::nullopt;
}

std::optional<std::reference_wrapper<const Character>> GameServer::GetCharacter(PlayerId id) const {
  if (auto player = player_manager_.GetPlayer(id)) {
    return std::cref(static_cast<const Character&>(player->get()));
  }
  if (auto npc = npc_manager_.GetNpc(id)) {
    return std::cref(static_cast<const Character&>(npc->get()));
  }
  return std::nullopt;
}

GameServer::PlayerId GameServer::CreateNpc(const std::string& name, const std::string& instance) {
  auto* resource = ResourceManager::GetCurrentResource();
  const auto owner = resource ? resource->GetName() : std::string{};
  if (unloading_npc_resources_.count(owner) != 0 || npc_manager_.GetNpcCount() >= 4096) {
    return 0;
  }
  const auto id = npc_manager_.Create(SanitizePlayerName(name), instance, server_world_, GetMaxSlots());
  if (id == 0) {
    return 0;
  }
  npc_manager_.GetNpc(id)->get().owner_resource = owner;
  pending_npc_events_.emplace_back([id] {
    EventManager::Instance().TriggerEvent(kEventOnNpcCreatedName, OnNpcCreatedEvent{id});
  });
  return id;
}

void GameServer::UnstreamNpc(NpcManager::Npc& npc) {
  const std::vector<PlayerId> viewers(npc.streamed_by_players.begin(), npc.streamed_by_players.end());
  for (const auto viewer_id : viewers) {
    if (auto viewer = player_manager_.GetPlayer(viewer_id)) {
      StreamOutSubjectFromViewer(npc, viewer->get());
    } else {
      npc.streamed_by_players.erase(viewer_id);
    }
  }
}

bool GameServer::DestroyNpc(PlayerId npc_id) {
  auto npc = npc_manager_.GetNpc(npc_id);
  if (!npc) {
    return false;
  }
  for (const auto child_id : npc_manager_.GetNpcIds()) {
    const auto child = npc_manager_.GetNpc(child_id);
    if (child && child->get().summon && child->get().summon->caster_id == npc_id)
      DestroyNpc(child_id);
  }
  // Detach before callbacks. Destruction must also work with exhausted counters.
  UnstreamNpc(npc->get());
  const auto old_host = npc->get().host_player_id;
  const auto actions = std::move(npc->get().actions);
  npc_manager_.Remove(npc_id);
  if (old_host != 0) {
    pending_npc_events_.emplace_back([npc_id, old_host] {
      EventManager::Instance().TriggerEvent(kEventOnNpcChangeHostPlayerName, OnNpcChangeHostPlayerEvent{npc_id, 0, old_host});
    });
  }
  for (const auto& action : actions) {
    pending_npc_events_.emplace_back([npc_id, id = action.id] {
      EventManager::Instance().TriggerEvent(kEventOnNpcActionFinishedName,
          OnNpcActionFinishedEvent{npc_id, kNpcActionPlayAnimation, id, false});
    });
  }
  pending_npc_events_.emplace_back([npc_id] {
    EventManager::Instance().TriggerEvent(kEventOnNpcDestroyedName, OnNpcDestroyedEvent{npc_id});
  });
  return true;
}

void GameServer::DestroyNpcsForResource(const std::string& owner) {
  if (owner.empty()) {
    return;
  }
  unloading_npc_resources_.insert(owner);
  for (const auto id : npc_manager_.GetNpcIds()) {
    const auto npc = npc_manager_.GetNpc(id);
    if (npc && npc->get().owner_resource == owner) {
      DestroyNpc(id);
    }
  }
  unloading_npc_resources_.erase(owner);
}

bool GameServer::SpawnNpc(PlayerId npc_id, std::optional<glm::vec3> position) {
  const auto npc_opt = npc_manager_.GetNpc(npc_id);
  if (!npc_opt || npc_opt->get().is_ingame || npc_opt->get().max_health <= 0 || (position && !IsValidActorPosition(*position)) ||
      !npc_manager_.BumpControlEpoch(npc_id)) {
    return false;
  }
  auto& npc = npc_opt->get();
  if (position) {
    npc.state.position = *position;
  }
  npc.flags = 0;
  npc.tod = 0;
  npc.health = npc.max_health;
  npc.mana = npc.max_mana;
  npc.state.health_points = npc.health;
  npc.state.mana_points = npc.mana;
  npc.state.life_state = PLAYER_LIFE_ALIVE;
  ClearTransientCombatState(npc);
  AdvancePlayerStateSequence(npc);
  npc.is_ingame = 1;
  npc.RenewGeneration();
  npc.state.source_time_ms = ActorClockMilliseconds();
  AdvanceActorCommand(npc);
  RefreshNpcStreaming(npc_id);
  return true;
}

bool GameServer::UnspawnNpc(PlayerId npc_id) {
  const auto npc = npc_manager_.GetNpc(npc_id);
  if (!npc || !npc->get().is_ingame) {
    return false;
  }
  npc->get().is_ingame = 0;
  UnstreamNpc(npc->get());
  SetNpcHostPlayer(npc_id, 0);
  return ClearNpcActions(npc_id);
}

bool GameServer::IsEligibleNpcHost(const NpcManager::Npc& npc, PlayerId host_id) const {
  const auto player = player_manager_.GetPlayer(host_id);
  return npc.is_ingame && npc.tod == 0 && player && player->get().is_ingame && player->get().npc_world_ready &&
      npc.streamed_by_players.count(host_id) != 0 &&
      IsInsideStreamRange(player->get(), npc, static_cast<float>(GetStreamerRadius()), static_cast<float>(GetStreamerHeight()));
}

void GameServer::BroadcastNpcControl(PlayerId npc_id) {
  const auto npc = npc_manager_.GetNpc(npc_id);
  if (!npc || !npc->get().is_ingame) {
    return;
  }
  const auto packet = MakeNpcControlPacket(npc->get());
  BroadcastToRelevant(player_manager_, npc->get(), packet, TrafficClass::Control);
  if (packet.action_id != 0 && packet.host_player_id != 0) {
    pending_npc_events_.emplace_back([npc_id, id = packet.action_id] {
      EventManager::Instance().TriggerEvent(kEventOnNpcActionSentName, OnNpcActionSentEvent{npc_id, kNpcActionPlayAnimation, id});
    });
  }
}

bool GameServer::SetNpcHostPlayer(PlayerId npc_id, PlayerId host_id) {
  const auto npc = npc_manager_.GetNpc(npc_id);
  if (!npc || (host_id != 0 && !IsEligibleNpcHost(npc->get(), host_id))) {
    return false;
  }
  const auto previous = npc->get().host_player_id;
  if (!npc_manager_.SetHost(npc_id, host_id)) {
    return false;
  }
  if (previous != host_id) {
    pending_npc_events_.emplace_back([npc_id, host_id, previous] {
      EventManager::Instance().TriggerEvent(kEventOnNpcChangeHostPlayerName, OnNpcChangeHostPlayerEvent{npc_id, host_id, previous});
    });
    BroadcastNpcControl(npc_id);
  }
  return true;
}

bool GameServer::SetNpcAnimation(PlayerId npc_id, const std::string& animation) {
  const auto npc = npc_manager_.GetNpc(npc_id);
  if (!npc) {
    return false;
  }
  const auto old_revision = npc->get().animation_revision;
  if (!npc_manager_.SetAnimation(npc_id, animation)) {
    return false;
  }
  if (npc->get().is_ingame && old_revision != npc->get().animation_revision) {
    BroadcastToRelevant(player_manager_, npc->get(), MakeNpcAnimationPacket(npc->get()), TrafficClass::Control);
  }
  return true;
}

void GameServer::RefreshNpcStreaming(PlayerId npc_id) {
  const auto npc_opt = npc_manager_.GetNpc(npc_id);
  if (!npc_opt || !npc_opt->get().is_ingame) {
    return;
  }
  auto& npc = npc_opt->get();
  const auto radius = static_cast<float>(GetStreamerRadius());
  const auto height = static_cast<float>(GetStreamerHeight());
  const std::vector<PlayerId> old_viewers(npc.streamed_by_players.begin(), npc.streamed_by_players.end());
  for (const auto id : old_viewers) {
    const auto viewer = player_manager_.GetPlayer(id);
    if (!viewer) {
      npc.streamed_by_players.erase(id);
    } else if (!viewer->get().is_ingame || !viewer->get().npc_world_ready || !IsInsideStreamRange(viewer->get(), npc, radius, height)) {
      StreamOutSubjectFromViewer(npc, viewer->get());
    }
  }
  std::vector<PlayerId> added_viewers;
  player_manager_.ForEachIngamePlayer([&](Player& player) {
    if (player.npc_world_ready && IsInsideStreamRange(player, npc, radius, height) && npc.streamed_by_players.insert(player.player_id).second) {
      player.spawned_players.insert(npc_id);
      added_viewers.push_back(player.player_id);
    }
  });

  // Keep a valid host. Distance alone must not cause rapid ownership changes.
  const auto old_host = npc.host_player_id;
  auto host = old_host;
  if (!IsEligibleNpcHost(npc, host)) {
    host = 0;
    float nearest = std::numeric_limits<float>::infinity();
    for (const auto id : npc.streamed_by_players) {
      if (!IsEligibleNpcHost(npc, id)) {
        continue;
      }
      const auto delta = player_manager_.GetPlayer(id)->get().state.position - npc.state.position;
      const float distance = glm::dot(delta, delta);
      if (distance < nearest || (distance == nearest && (host == 0 || id < host))) {
        host = id;
        nearest = distance;
      }
    }
  }
  if (!npc_manager_.SetHost(npc_id, host)) {
    DestroyNpc(npc_id);
    return;
  }
  if (!added_viewers.empty()) {
    NpcSpawnPacket packet;
    packet.actor = MakePlayerSpawnPacket(npc);
    packet.control = MakeNpcControlPacket(npc);
    packet.animation = MakeNpcAnimationPacket(npc);
    for (const auto viewer_id : added_viewers) {
      const auto connection = player_manager_.GetConnectionHandle(viewer_id);
      if (connection) {
        SerializeAndSend(packet, TrafficClass::Control, *connection);
      }
    }
  }
  if (old_host != host) {
    pending_npc_events_.emplace_back([npc_id, host, old_host] {
      EventManager::Instance().TriggerEvent(kEventOnNpcChangeHostPlayerName, OnNpcChangeHostPlayerEvent{npc_id, host, old_host});
    });
    BroadcastNpcControl(npc_id);
  }
}

void GameServer::ReleaseNpcsForPlayer(PlayerId player_id) {
  for (const auto npc_id : npc_manager_.GetNpcIds()) {
    const auto npc = npc_manager_.GetNpc(npc_id);
    const auto viewer = player_manager_.GetPlayer(player_id);
    if (!npc) {
      continue;
    }
    if (npc->get().summon && npc->get().summon->caster_id == player_id) {
      DestroyNpc(npc_id);
      continue;
    }
    if (viewer) {
      StreamOutSubjectFromViewer(npc->get(), viewer->get());
    } else {
      npc->get().streamed_by_players.erase(player_id);
    }
    if (npc->get().host_player_id == player_id) {
      if (!SetNpcHostPlayer(npc_id, 0)) {
        DestroyNpc(npc_id);
      }
    }
  }
}

void GameServer::ProcessNpcs() {
  const auto now = NpcManager::Clock::now();
  for (const auto id : npc_manager_.GetNpcIds()) {
    const auto npc = npc_manager_.GetNpc(id);
    if (!npc) {
      continue;
    }
    if (npc->get().summon) {
      const auto& summon = *npc->get().summon;
      const auto caster = GetCharacter(summon.caster_id);
      if (now >= summon.expires_at || npc->get().tod != 0 || !caster || caster->get().actor_generation != summon.caster_generation ||
          !IsLiveCharacter(caster->get()) || !SameCharacterScope(caster->get(), npc->get())) {
        DestroyNpc(id);
        continue;
      }
    }
    if (npc->get().control_epoch == std::numeric_limits<std::uint32_t>::max()) {
      DestroyNpc(id);
      continue;
    }
    const auto old_epoch = npc->get().control_epoch;
    const auto expired = npc_manager_.ExpireActions(id, now);
    for (const auto& action : expired) {
      pending_npc_events_.emplace_back([id, action_id = action.id] {
        EventManager::Instance().TriggerEvent(kEventOnNpcActionFinishedName,
          OnNpcActionFinishedEvent{id, kNpcActionPlayAnimation, action_id, false});
      });
    }
    if (old_epoch != npc->get().control_epoch) {
      BroadcastNpcControl(id);
    }
    RefreshNpcStreaming(id);
  }
}

std::uint32_t GameServer::QueueNpcAnimation(PlayerId npc_id, const std::string& animation, std::uint32_t timeout_ms) {
  const auto npc = npc_manager_.GetNpc(npc_id);
  if (!npc || npc->get().tod != 0) {
    return 0;
  }
  const auto epoch = npc->get().control_epoch;
  const auto id = npc_manager_.QueueAnimation(npc_id, animation, timeout_ms);
  if (id != 0 && epoch != npc->get().control_epoch) {
    BroadcastNpcControl(npc_id);
  }
  return id;
}

bool GameServer::ClearNpcActions(PlayerId npc_id) {
  const auto cleared = npc_manager_.ClearActions(npc_id);
  if (!cleared) {
    return false;
  }
  BroadcastNpcControl(npc_id);
  for (const auto& action : *cleared) {
    pending_npc_events_.emplace_back([npc_id, id = action.id] {
      EventManager::Instance().TriggerEvent(kEventOnNpcActionFinishedName,
          OnNpcActionFinishedEvent{npc_id, kNpcActionPlayAnimation, id, false});
    });
  }
  return true;
}

void GameServer::HandleNpcActionResult(Packet p) {
  const auto player = GetIngamePlayerByConnection(p.id);
  if (!player) {
    return;
  }
  NpcActionResultPacket packet;
  using Adapter = bitsery::InputBufferAdapter<unsigned char*>;
  const auto decoded = bitsery::quickDeserialization<Adapter>({p.data, p.length}, packet);
  if (decoded.first != bitsery::ReaderError::NoError || !decoded.second || packet.success > 1) {
    return;
  }
  const auto npc = npc_manager_.GetNpc(packet.npc_id);
  if (!npc || packet.actor_generation != npc->get().actor_generation ||
      !IsEligibleNpcHost(npc->get(), player->get().player_id)) {
    return;
  }
  const auto completed = npc_manager_.FinishAction(packet.npc_id, player->get().player_id,
      packet.control_epoch, packet.action_id);
  if (!completed) {
    return;
  }
  pending_npc_events_.emplace_back([packet] {
    EventManager::Instance().TriggerEvent(kEventOnNpcActionFinishedName,
        OnNpcActionFinishedEvent{packet.npc_id, kNpcActionPlayAnimation, packet.action_id, packet.success != 0});
  });
  BroadcastNpcControl(packet.npc_id);
}

bool GameServer::SpawnCharacter(PlayerId player_id, std::optional<glm::vec3> position_override) {
  if (npc_manager_.GetNpc(player_id)) {
    return SpawnNpc(player_id, position_override);
  }
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("spawnPlayer called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  if (player.is_ingame) {
    SPDLOG_WARN("spawnPlayer called for already spawned player {}", player_id);
    return false;
  }

  if (position_override.has_value()) {
    if (!IsValidActorPosition(*position_override)) return false;
    player.state.position = *position_override;
  }

  const bool was_dead = player.tod != 0;
  RestoreTransformation(player);

  player.flags = 0;
  player.tod = 0;
  player.health = player.max_health;
  player.mana = player.max_mana;
  player.state.health_points = player.health;
  player.state.mana_points = player.mana;
  player.state.life_state = GetPlayerLifeState(player);
  ClearTransientCombatState(player);
  AdvancePlayerStateSequence(player);

  player.is_ingame = 1;
  player.RenewGeneration();
  player.state.source_time_ms = ActorClockMilliseconds();
  AdvanceActorCommand(player);

  const auto packet = MakePlayerSpawnPacket(player);

  SerializeAndSend(packet, TrafficClass::Control, player.connection);
  StreamRelevantGroundItemsToPlayer(player, true);

  RefreshPlayerStreaming();

  if (was_dead) {
    EventManager::Instance().TriggerEvent(kEventOnPlayerRespawnName, OnPlayerRespawnEvent{player_id, packet.position});
  }
  const auto current = player_manager_.GetPlayer(player_id);
  if (current && current->get().actor_generation == packet.actor_generation) {
    EventManager::Instance().TriggerEvent(kEventOnPlayerSpawnName, OnPlayerSpawnEvent{player_id, packet.position});
  }
  return true;
}

bool GameServer::UnspawnCharacter(PlayerId player_id) {
  if (npc_manager_.GetNpc(player_id)) {
    return UnspawnNpc(player_id);
  }
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("unspawnPlayer called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  if (!player.is_ingame) {
    SPDLOG_WARN("unspawnPlayer called for already unspawned player {}", player_id);
    return false;
  }

  ReleaseNpcsForPlayer(player_id);
  UnstreamGroundItemsFromPlayer(player, true);
  RestoreTransformation(player);
  StreamOutAllKnownPlayers(player_manager_, player);

  DisconnectionInfoPacket subject_left_packet;
  subject_left_packet.packet_type = PT_LEFT_GAME;
  subject_left_packet.disconnected_id = player.player_id;

  SerializeAndSend(subject_left_packet, TrafficClass::Control, player.connection);

  player.is_ingame = 0;
  player.npc_world_ready = false;
  return true;
}

bool GameServer::SetCharacterName(PlayerId player_id, const std::string& name) {
  auto player_opt = GetCharacter(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerName called for unknown player id {}", player_id);
    return false;
  }

  auto sanitized_name = SanitizePlayerName(name);
  if (sanitized_name.empty()) {
    SPDLOG_WARN("setPlayerName called with empty name for player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  player.name = sanitized_name;

  PlayerNameUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_NAME_UPDATE;
  packet.player_id = player.player_id;
  packet.name = player.name;

  BroadcastToRelevant(player_manager_, player, packet, TrafficClass::Control);
  return true;
}

bool GameServer::SetCharacterPosition(PlayerId player_id, const glm::vec3& position, std::optional<bool> teleport) {
  if (!IsValidActorPosition(position)) {
    return false;
  }
  auto player_opt = GetCharacter(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerPosition called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  const bool is_npc = npc_manager_.GetNpc(player_id).has_value();
  player.state.position = position;
  player.state.source_time_ms = ActorClockMilliseconds();
  AdvanceActorCommand(player);
  player.native_teleport_barrier_revision = player.command_revision;
  const auto state_sequence = AdvancePlayerStateSequence(player);

  PlayerPositionUpdatePacket packet{};
  packet.packet_type = PT_MAP_ONLY;
  packet.player_id = player.player_id;
  packet.state_sequence = state_sequence;
  packet.position = position;
  packet.actor_generation = player.actor_generation;
  packet.command_revision = player.command_revision;
  packet.source_time_ms = player.state.source_time_ms;
  // Script-driven NPC movement participates in the pose timeline. Player
  // corrections are explicit teleports and reset prediction.
  packet.teleport = teleport.value_or(!is_npc);

  // Transform revisions are independent of the action epoch: Lua movement
  // must not restart a running action or extend its timeout.
  BroadcastToRelevant(player_manager_, player, packet, TrafficClass::Control);
  if (is_npc) {
    RefreshNpcStreaming(player_id);
  } else {
    RefreshPlayerStreaming();
  }

  return true;
}

bool GameServer::SetCharacterAngle(PlayerId player_id, float angle) {
  if (!std::isfinite(angle)) {
    return false;
  }
  auto player_opt = GetCharacter(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerAngle called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  const float angle_radians = angle;
  player.state.nrot = glm::vec3{std::sin(angle_radians), 0.0f, std::cos(angle_radians)};
  player.state.source_time_ms = ActorClockMilliseconds();
  AdvanceActorCommand(player);
  player.native_teleport_barrier_revision = player.command_revision;
  AdvancePlayerStateSequence(player);

  auto packet = MakePlayerStateUpdatePacket(player);
  packet.state.correction_mask = kSyncOrientation;
  BroadcastToRelevant(player_manager_, player, packet, TrafficClass::Control);
  return true;
}

bool GameServer::SetCharacterWorld(PlayerId player_id, const std::string& world, std::optional<std::string> start_point) {
  if (auto npc = npc_manager_.GetNpc(player_id)) {
    const auto sanitized = SanitizeWorldName(world);
    // A headless server cannot resolve a Gothic waypoint into coordinates.
    if (sanitized.empty() || (start_point && !start_point->empty())) {
      return false;
    }
    if (npc->get().world != sanitized) {
      UnstreamNpc(npc->get());
      SetNpcHostPlayer(player_id, 0);
      ClearNpcActions(player_id);
      npc->get().world = sanitized;
      npc->get().RenewGeneration();
      npc->get().state.source_time_ms = ActorClockMilliseconds();
      RefreshNpcStreaming(player_id);
    }
    return true;
  }
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerWorld called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  const auto sanitized_world = SanitizeWorldName(world);
  if (sanitized_world.empty()) return false;
  const bool world_changed = sanitized_world != player.world;
  std::string start_point_name = SanitizeServerText(start_point.value_or(""));
  if (start_point_name.size() > 64) {
    start_point_name.resize(64);
  }

  if (world_changed) {
    EventManager::Instance().TriggerEvent(kEventOnPlayerWorldChangeName,
                                          OnPlayerChangeWorldEvent{player.player_id, sanitized_world, start_point_name});
  }

  if (player.is_ingame && world_changed) {
    ReleaseNpcsForPlayer(player_id);
    player.npc_world_ready = false;
    UnstreamGroundItemsFromPlayer(player, true);
    StreamOutAllKnownPlayers(player_manager_, player);
  }

  player.world = sanitized_world;
  if (world_changed) {
    const bool was_transformed = player.transformation.has_value();
    RestoreTransformation(player);
    if (was_transformed) {
      AdvanceActorCommand(player);
      AdvancePlayerStateSequence(player);
      SerializeAndSend(MakeTransformationPacket(player, 0), TrafficClass::Control, player.connection);
    }
    player.RenewGeneration();
    AdvanceActorCommand(player);
    player.client_clock_origin.reset();
    player.last_source_time_ms = 0;
  }

  PlayerWorldUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_WORLD_UPDATE;
  packet.player_id = player.player_id;
  packet.world_name = player.world;
  packet.start_point = start_point_name;
  packet.actor_generation = player.actor_generation;
  packet.command_revision = player.command_revision;

  SerializeAndSend(packet, TrafficClass::Control, player.connection);

  if (!player.is_ingame || !world_changed) {
    return true;
  }

  RefreshPlayerStreaming();
  StreamRelevantGroundItemsToPlayer(player);

  return true;
}

bool GameServer::SetCharacterVirtualWorld(PlayerId player_id, std::int32_t virtual_world) {
  if (auto npc = npc_manager_.GetNpc(player_id)) {
    const auto clamped = std::clamp<std::int32_t>(virtual_world, 0, 65535);
    if (npc->get().virtual_world != clamped) {
      UnstreamNpc(npc->get());
      SetNpcHostPlayer(player_id, 0);
      ClearNpcActions(player_id);
      npc->get().virtual_world = clamped;
      npc->get().RenewGeneration();
      AdvanceActorCommand(npc->get());
      RefreshNpcStreaming(player_id);
    }
    return true;
  }
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerVirtualWorld called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  const auto clamped_virtual_world = std::clamp<std::int32_t>(virtual_world, 0, 65535);
  const bool virtual_world_changed = clamped_virtual_world != player.virtual_world;

  if (player.is_ingame && virtual_world_changed) {
    ReleaseNpcsForPlayer(player_id);
    UnstreamGroundItemsFromPlayer(player, true);
    StreamOutAllKnownPlayers(player_manager_, player);
  }

  player.virtual_world = clamped_virtual_world;
  if (virtual_world_changed) {
    RestoreTransformation(player);
    player.RenewGeneration();
    AdvanceActorCommand(player);
    player.state.source_time_ms = ActorClockMilliseconds();
    SerializeAndSend(MakePlayerSpawnPacket(player), TrafficClass::Control, player.connection);
  }

  if (!player.is_ingame || !virtual_world_changed) {
    return true;
  }

  RefreshPlayerStreaming();
  StreamRelevantGroundItemsToPlayer(player);

  return true;
}

bool GameServer::SetCharacterStrength(PlayerId player_id, std::int32_t strength) {
  auto player_opt = GetCharacter(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerStrength called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  player.strength = std::max<std::int32_t>(0, strength);
  SendPlayerAttributeUpdate(player_manager_, player, ATTR_STRENGTH, player.strength);
  return true;
}

bool GameServer::SetCharacterDexterity(PlayerId player_id, std::int32_t dexterity) {
  auto player_opt = GetCharacter(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerDexterity called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  player.dexterity = std::max<std::int32_t>(0, dexterity);
  SendPlayerAttributeUpdate(player_manager_, player, ATTR_DEXTERITY, player.dexterity);
  return true;
}

bool GameServer::SetCharacterLevel(PlayerId player_id, std::int32_t level) {
  auto player_opt = GetCharacter(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerLevel called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  player.level = std::max<std::int32_t>(0, level);
  SendPlayerAttributeUpdate(player_manager_, player, ATTR_LEVEL, player.level);
  return true;
}

bool GameServer::SetCharacterExperience(PlayerId player_id, std::int32_t exp) {
  auto player_opt = GetCharacter(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerExp called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  player.exp = std::max<std::int32_t>(0, exp);
  SendPlayerAttributeUpdate(player_manager_, player, ATTR_EXP, player.exp);
  return true;
}

bool GameServer::SetCharacterNextLevelExperience(PlayerId player_id, std::int32_t next_level_exp) {
  auto player_opt = GetCharacter(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerNextLevelExp called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  player.next_level_exp = std::max<std::int32_t>(0, next_level_exp);
  SendPlayerAttributeUpdate(player_manager_, player, ATTR_NEXT_LEVEL_EXP, player.next_level_exp);
  return true;
}

bool GameServer::SetCharacterLearnPoints(PlayerId player_id, std::int32_t learn_points) {
  auto player_opt = GetCharacter(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerLearnPoints called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  player.learn_points = std::max<std::int32_t>(0, learn_points);
  SendPlayerAttributeUpdate(player_manager_, player, ATTR_LEARN_POINTS, player.learn_points);
  return true;
}

bool GameServer::SetCharacterMaxHealth(PlayerId player_id, std::int32_t max_health) {
  auto player_opt = GetCharacter(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerMaxHealth called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  const auto old_health = player.health;
  player.max_health = std::max<std::int32_t>(0, max_health);
  SendPlayerAttributeUpdate(player_manager_, player, ATTR_MAX_HEALTH, player.max_health);
  if (player.tod == 0 && player.max_health <= 0) {
    HandlePlayerDeath(player, std::nullopt);
    return true;
  }
  if (player.health > player.max_health) {
    player.health = player.max_health;
  }
  player.state.health_points = player.health;
  player.state.life_state = GetPlayerLifeState(player);
  AdvancePlayerStateSequence(player);
  SendPlayerAttributeUpdate(player_manager_, player, ATTR_HEALTH, player.health);
  if (old_health != player.health && !npc_manager_.GetNpc(player_id)) {
    EventManager::Instance().TriggerEvent(kEventOnPlayerChangeHealthName, OnPlayerChangeHealthEvent{player_id, old_health, player.health});
  }
  return true;
}

bool GameServer::SetCharacterHealth(PlayerId player_id, std::int32_t health) {
  auto player_opt = GetCharacter(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerHealth called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  const auto clamped = std::clamp<std::int32_t>(health, 0, player.max_health);
  const auto old_health = player.health;
  if (player.tod != 0 && clamped > 0) {
    SPDLOG_WARN("setPlayerHealth called with positive health for dead player {}; use respawnPlayer instead", player_id);
    return false;
  }
  if (player.tod == 0 && old_health > 0 && clamped <= 0) {
    HandlePlayerDeath(player, std::nullopt);
    return true;
  }

  const bool should_stand_up = player.tod == 0 && (player.flags & PlayerManager::PL_UNCONCIOUS) != 0 && clamped > 1;
  if (should_stand_up) {
    player.flags &= ~PlayerManager::PL_UNCONCIOUS;
    ClearRecoveredPose(player);
    AdvanceActorCommand(player);
    player.combat_barrier_revision = player.command_revision;
  }

  player.health = clamped;
  player.state.health_points = player.health;
  player.state.life_state = GetPlayerLifeState(player);
  AdvancePlayerStateSequence(player);
  SendPlayerAttributeUpdate(player_manager_, player, ATTR_HEALTH, player.health);
  const auto generation = player.actor_generation;
  const auto new_health = player.health;
  if (should_stand_up) {
    SendStandUpInfo(player_id);
  }
  if (old_health != new_health && !npc_manager_.GetNpc(player_id)) {
    EventManager::Instance().TriggerEvent(kEventOnPlayerChangeHealthName, OnPlayerChangeHealthEvent{player_id, old_health, new_health});
  }
  const auto current = GetCharacter(player_id);
  if (should_stand_up && current && current->get().actor_generation == generation &&
      GetPlayerLifeState(current->get()) == PLAYER_LIFE_ALIVE) {
    EventManager::Instance().TriggerEvent(kEventOnPlayerStandUpName, OnPlayerStandUpEvent{player_id});
  }
  return true;
}

bool GameServer::SetCharacterInvulnerable(PlayerId player_id, bool enabled) {
  const auto actor = GetCharacter(player_id);
  if (!actor) return false;
  auto& subject = actor->get();
  if (subject.instance_change) subject.instance_change->invulnerable_changed = true;
  subject.invulnerable = enabled;
  subject.state.invulnerable = enabled;
  AdvanceActorCommand(subject);
  AdvancePlayerStateSequence(subject);
  auto packet = MakePlayerStateUpdatePacket(subject);
  packet.state.correction_mask = kSyncLife | kSyncHealth | kSyncMana;
  BroadcastToRelevant(player_manager_, subject, packet, TrafficClass::Control);
  return true;
}

bool GameServer::SetCharacterMaxMana(PlayerId player_id, std::int32_t max_mana) {
  auto player_opt = GetCharacter(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerMaxMana called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  const auto old_mana = player.mana;
  player.max_mana = std::max<std::int32_t>(0, max_mana);
  if (player.mana > player.max_mana) {
    player.mana = player.max_mana;
  }
  player.state.mana_points = player.mana;
  AdvancePlayerStateSequence(player);
  SendPlayerAttributeUpdate(player_manager_, player, ATTR_MAX_MANA, player.max_mana);
  SendPlayerAttributeUpdate(player_manager_, player, ATTR_MANA, player.mana);
  if (old_mana != player.mana && !npc_manager_.GetNpc(player_id)) {
    EventManager::Instance().TriggerEvent(kEventOnPlayerChangeManaName, OnPlayerChangeManaEvent{player_id, old_mana, player.mana});
  }
  return true;
}

bool GameServer::SetCharacterMana(PlayerId player_id, std::int32_t mana) {
  auto player_opt = GetCharacter(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerMana called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  const auto clamped = std::clamp<std::int32_t>(mana, 0, player.max_mana);
  const auto old_mana = player.mana;
  player.mana = clamped;
  player.state.mana_points = player.mana;
  AdvancePlayerStateSequence(player);
  SendPlayerAttributeUpdate(player_manager_, player, ATTR_MANA, player.mana);
  if (old_mana != player.mana && !npc_manager_.GetNpc(player_id)) {
    EventManager::Instance().TriggerEvent(kEventOnPlayerChangeManaName, OnPlayerChangeManaEvent{player_id, old_mana, player.mana});
  }
  return true;
}

bool GameServer::SetCharacterVisual(PlayerId player_id, const std::string& body_model, std::int16_t body_texture, const std::string& head_model,
                                 std::int16_t head_texture, std::int16_t teeth_texture, std::int16_t skin_color) {
  auto player_opt = GetCharacter(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerVisual called for unknown player id {}", player_id);
    return false;
  }

  if (npc_manager_.GetNpc(player_id) && !ClearNpcActions(player_id)) {
    return false;
  }
  auto& player = player_opt->get();
  if (player.instance_change) player.instance_change->visual_changed = true;
  player.body_model = SanitizeServerText(body_model);
  player.head_model = SanitizeServerText(head_model);
  if (player.body_model.size() > 64) {
    player.body_model.resize(64);
  }
  if (player.head_model.size() > 64) {
    player.head_model.resize(64);
  }
  player.body_texture = static_cast<std::int16_t>(std::clamp<int>(body_texture, 0, 255));
  player.head_texture = static_cast<std::int16_t>(std::clamp<int>(head_texture, 0, 255));
  player.teeth_texture = static_cast<std::int16_t>(std::clamp<int>(teeth_texture, 0, 255));
  player.skin_color = static_cast<std::int16_t>(std::clamp<int>(skin_color, 0, 255));

  PlayerVisualUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_VISUAL_UPDATE;
  packet.player_id = player.player_id;
  packet.body_model = player.body_model;
  packet.body_texture = player.body_texture;
  packet.head_model = player.head_model;
  packet.head_texture = player.head_texture;
  packet.teeth_texture = player.teeth_texture;
  packet.skin_color = player.skin_color;

  BroadcastToRelevant(player_manager_, player, packet, TrafficClass::Control);
  return true;
}

bool GameServer::SetCharacterInstance(PlayerId player_id, const std::string& instance) {
  auto actor = GetCharacter(player_id);
  const auto instance_name = SanitizeServerText(instance);
  if (!actor || instance_name.empty() || instance_name.size() > 255)
    return false;
  auto npc = npc_manager_.GetNpc(player_id);
  if (npc && !ClearNpcActions(player_id))
    return false;
  auto& character = actor->get();
  RestoreTransformation(character);
  const auto previous_instance = character.instance_change ? character.instance_change->previous_instance : character.instance;
  if (npc) {
    UnstreamNpc(npc->get());
    SetNpcHostPlayer(player_id, 0);
    character.RenewGeneration();
  }
  character.instance = instance_name;
  const auto revision = AdvanceActorCommand(character);
  character.instance_change.emplace();
  character.instance_change->request_revision = revision;
  character.instance_change->previous_instance = previous_instance;
  character.instance_change->previous_state = character.state;
  if (npc) {
    RefreshNpcStreaming(player_id);
  } else if (const auto connection = player_manager_.GetConnectionHandle(player_id)) {
    PlayerInstanceUpdatePacket packet;
    packet.packet_type = PT_PLAYER_INSTANCE_UPDATE;
    packet.player_id = player_id;
    packet.actor_generation = character.actor_generation;
    packet.command_revision = revision;
    packet.instance = instance_name;
    packet.initialize_defaults = true;
    SerializeAndSend(packet, TrafficClass::Control, *connection);
  }
  return true;
}

bool GameServer::SetCharacterNameColor(PlayerId player_id, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
  auto player_opt = GetCharacter(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerNameColor called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  player.name_color_r = r;
  player.name_color_g = g;
  player.name_color_b = b;

  BroadcastPlayerColorUpdate(player_manager_, player);
  return true;
}

bool GameServer::SetCharacterSkillWeapon(PlayerId player_id, std::int32_t skill_id, std::int32_t percentage) {
  if (!gmp::IsWeaponSkill(skill_id))
    return false;
  auto player_opt = GetCharacter(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerSkillWeapon called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  const auto clamped = std::clamp<std::int32_t>(percentage, 0, 100);
  player.weapon_skills[skill_id] = clamped;
  const int tier = gmp::WeaponSkillTier(clamped);
  player.talents[skill_id] = tier;
  if (player.instance_change) {
    player.instance_change->weapon_skills[skill_id] = clamped;
    player.instance_change->talents[skill_id] = tier;
  }
  BroadcastPlayerSkillWeaponUpdate(player_manager_, player, skill_id, clamped);
  BroadcastPlayerTalentUpdate(player_manager_, player, skill_id, tier);
  return true;
}

bool GameServer::SetCharacterTalent(PlayerId player_id, std::int32_t talent_id, std::int32_t talent_value) {
  auto player_opt = GetCharacter(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerTalent called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  const auto clamped = gmp::IsWeaponSkill(talent_id) ? std::clamp<std::int32_t>(talent_value, 0, 2) : std::max<std::int32_t>(0, talent_value);
  player.talents[talent_id] = clamped;
  if (player.instance_change) player.instance_change->talents[talent_id] = clamped;
  BroadcastPlayerTalentUpdate(player_manager_, player, talent_id, clamped);
  return true;
}

bool GameServer::SetCharacterFatness(PlayerId player_id, float fatness) {
  if (!std::isfinite(fatness)) {
    return false;
  }
  auto player_opt = GetCharacter(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerFatness called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  if (player.instance_change) player.instance_change->fatness_changed = true;
  player.fatness = fatness;
  BroadcastPlayerFatnessUpdate(player_manager_, player);
  return true;
}

bool GameServer::SetCharacterScale(PlayerId player_id, const glm::vec3& scale) {
  if (!IsFinitePosition(scale)) {
    return false;
  }
  auto player_opt = GetCharacter(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerScale called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  if (player.instance_change) player.instance_change->scale_changed = true;
  player.scale = scale;
  BroadcastPlayerScaleUpdate(player_manager_, player);
  return true;
}

bool GameServer::SetCharacterWeaponMode(PlayerId player_id, std::int32_t weapon_mode) {
  if (weapon_mode < 0 || weapon_mode > 7)
    return false;
  auto player_opt = GetCharacter(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerWeaponMode called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  if (player.instance_change)
    player.instance_change->weapon_mode_changed = true;
  const auto old_mode = player.state.weapon_mode;
  const auto clamped_mode = static_cast<std::uint8_t>(weapon_mode);
  player.state.weapon_mode = clamped_mode;
  ReconcileWeaponModeHands(player);
  const auto generation = player.actor_generation;
  AdvanceActorCommand(player);

  if (old_mode != player.state.weapon_mode) {
    EventManager::Instance().TriggerEvent(kEventOnPlayerWeaponModeChangeName,
                                          OnPlayerWeaponModeChangeEvent{player.player_id, old_mode, player.state.weapon_mode});
  }

  // A script callback can issue another mode command or destroy the actor.
  player_opt = GetCharacter(player_id);
  if (!player_opt || player_opt->get().actor_generation != generation)
    return false;
  auto& current = player_opt->get();
  ReconcileWeaponModeHands(current);
  AdvancePlayerStateSequence(current);

  auto packet = MakePlayerStateUpdatePacket(current);
  packet.state.correction_mask = kSyncCombat | kSyncEquipment;
  BroadcastToRelevant(player_manager_, current, packet, TrafficClass::Control);
  return true;
}

bool GameServer::ApplyCharacterOverlay(PlayerId player_id, const std::string& overlay) {
  auto player_opt = GetCharacter(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("applyPlayerOverlay called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  auto overlay_name = SanitizeServerText(overlay);
  if (overlay_name.size() > 255) {
    overlay_name.resize(255);
  }
  if (overlay_name.empty()) {
    return false;
  }

  if (player.instance_change) player.instance_change->overlays[overlay_name] = true;
  if (std::find(player.overlays.begin(), player.overlays.end(), overlay_name) == player.overlays.end()) {
    if (npc_manager_.GetNpc(player_id) && !ClearNpcActions(player_id)) {
      return false;
    }
    player.overlays.push_back(overlay_name);
    BroadcastPlayerOverlayUpdate(player_manager_, player, overlay_name, true);
  }

  return true;
}

bool GameServer::RemoveCharacterOverlay(PlayerId player_id, const std::string& overlay) {
  auto player_opt = GetCharacter(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("removePlayerOverlay called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  auto overlay_name = SanitizeServerText(overlay);
  if (overlay_name.size() > 255) {
    overlay_name.resize(255);
  }
  if (overlay_name.empty()) {
    return false;
  }

  auto it = std::find(player.overlays.begin(), player.overlays.end(), overlay_name);
  if (it == player.overlays.end()) {
    return false;
  }

  if (npc_manager_.GetNpc(player_id) && !ClearNpcActions(player_id)) {
    return false;
  }
  player.overlays.erase(it);
  if (player.instance_change) player.instance_change->overlays[overlay_name] = false;
  BroadcastPlayerOverlayUpdate(player_manager_, player, overlay_name, false);
  return true;
}

bool GameServer::PlayAnimation(PlayerId player_id, const std::string& animation) {
  if (!gmp::IsValidAnimationName(animation)) return false;
  const auto name = gmp::NormalizeScriptName(animation);
  if (npc_manager_.GetNpc(player_id)) return QueueNpcAnimation(player_id, name) != 0;
  const auto player = player_manager_.GetPlayer(player_id);
  if (!player) return false;
  PlayerAnimationPacket packet{};
  packet.packet_type = PT_PLAYER_ANI_PLAY;
  packet.player_id = player_id;
  packet.animation = name;
  BroadcastToRelevant(player_manager_, player->get(), packet, TrafficClass::Control);
  return true;
}

bool GameServer::StopAnimation(PlayerId player_id, const std::string& animation) {
  if (!animation.empty() && (!gmp::IsValidAnimationName(animation))) return false;
  const auto name = gmp::NormalizeScriptName(animation);
  if (const auto npc = npc_manager_.GetNpc(player_id)) {
    if (name.empty() || (!npc->get().actions.empty() && npc->get().actions.front().animation == name)) return ClearNpcActions(player_id);
    return false;
  }
  const auto player = player_manager_.GetPlayer(player_id);
  if (!player) return false;
  PlayerAnimationStopPacket packet{};
  packet.packet_type = PT_PLAYER_ANI_STOP;
  packet.player_id = player_id;
  packet.animation = name;
  BroadcastToRelevant(player_manager_, player->get(), packet, TrafficClass::Control);
  return true;
}

bool GameServer::PlayFaceAnimation(PlayerId player_id, const std::string& animation) {
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("playFaceAni called for unknown player id {}", player_id);
    return false;
  }

  auto animation_name = SanitizeServerText(animation);
  if (animation_name.size() > 255) {
    animation_name.resize(255);
  }
  if (animation_name.empty()) {
    return false;
  }

  auto& player = player_opt->get();
  PlayerFaceAnimationPacket packet{};
  packet.packet_type = PT_PLAYER_FACE_ANI_PLAY;
  packet.player_id = player.player_id;
  packet.animation = std::move(animation_name);

  BroadcastToRelevant(player_manager_, player, packet, TrafficClass::Control);
  return true;
}

bool GameServer::StopFaceAnimation(PlayerId player_id, const std::string& animation) {
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("stopFaceAni called for unknown player id {}", player_id);
    return false;
  }

  auto animation_name = SanitizeServerText(animation);
  if (animation_name.size() > 255) {
    animation_name.resize(255);
  }

  auto& player = player_opt->get();
  PlayerFaceAnimationStopPacket packet{};
  packet.packet_type = PT_PLAYER_FACE_ANI_STOP;
  packet.player_id = player.player_id;
  packet.animation = std::move(animation_name);

  BroadcastToRelevant(player_manager_, player, packet, TrafficClass::Control);
  return true;
}

bool GameServer::PlayGesticulation(PlayerId player_id) {
  auto player_opt = GetCharacter(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("playGesticulation called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  PlayerGesticulationPacket packet{};
  packet.packet_type = PT_PLAYER_GESTICULATION;
  packet.player_id = player.player_id;

  BroadcastToRelevant(player_manager_, player, packet, TrafficClass::Control);
  return true;
}

bool GameServer::GiveItem(PlayerId player_id, const std::string& instance, std::int32_t amount) {
  auto player_opt = GetCharacter(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("giveItem called for unknown player id {}", player_id);
    return false;
  }

  if (amount <= 0) {
    return false;
  }

  auto item_instance = ResolveItemInstance(instance);
  if (!item_instance.has_value()) {
    SPDLOG_WARN("giveItem called with unknown item instance '{}'", SanitizeServerText(instance));
    return false;
  }

  auto& player = player_opt->get();
  if (!AddInventoryItem(player, *item_instance, amount)) return false;
  if (player.instance_change) player.instance_change->inventory_deltas[*item_instance] += amount;
  if (++player.command_revision == 0) ++player.command_revision;
  AdvancePlayerStateSequence(player);

  GiveItemPacket packet{};
  packet.packet_type = PT_GIVEITEM;
  packet.player_id = player.player_id;
  packet.actor_generation = player.actor_generation;
  packet.state_sequence = player.state_sequence;
  packet.command_revision = player.command_revision;
  packet.item_instance = *item_instance;
  packet.item_amount = std::max<std::int32_t>(0, amount);

  BroadcastToRelevant(player_manager_, player, packet, TrafficClass::Control);
  return true;
}

bool GameServer::EquipItem(PlayerId player_id, const std::string& instance, std::int32_t slot_id) {
  if (slot_id < std::numeric_limits<std::int16_t>::min() || slot_id > std::numeric_limits<std::int16_t>::max()) return false;
  if (slot_id != EQUIP_SLOT_GENERIC) {
    const auto slot = PacketEquipmentSlot(static_cast<std::int16_t>(slot_id));
    return slot && slot_id == EquipmentSlotPacketId(*slot) && EquipItemInSlot(player_id, instance, *slot);
  }
  auto actor = GetCharacter(player_id);
  const auto item = ResolveItemInstance(instance);
  if (!actor || !item || actor->get().generic_equipment_requests.size() >= 64) return false;
  if (actor->get().InventoryCount(*item) == 0 && !GiveItem(player_id, *item, 1)) return false;
  actor->get().generic_equipment_requests.push_back(*item);
  ProcessGenericEquipment(player_id);
  return true;
}

bool GameServer::UnequipItem(PlayerId player_id, const std::string& instance, std::int32_t slot_id) {
  auto player_opt = GetCharacter(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("unequipItem called for unknown player id {}", player_id);
    return false;
  }

  auto item_instance = ResolveItemInstance(instance);
  if (!item_instance.has_value()) {
    SPDLOG_WARN("unequipItem called with unknown item instance '{}'", SanitizeServerText(instance));
    return false;
  }

  auto& player = player_opt->get();
  bool cancelled_request = false;
  if (slot_id == EQUIP_SLOT_GENERIC) {
    cancelled_request = std::erase(player.generic_equipment_requests, *item_instance) > 0;
    const auto pending = player.pending_equipment_commands.find(EQUIP_SLOT_GENERIC);
    if (pending != player.pending_equipment_commands.end() && pending->second.expected_instance == *item_instance)
      player.pending_equipment_commands.erase(pending);
  }
  std::vector<EquipmentSlot> matches;
  for (const auto slot : {EquipmentSlot::Armor, EquipmentSlot::Helmet, EquipmentSlot::Shield, EquipmentSlot::MeleeWeapon,
                         EquipmentSlot::RangedWeapon, EquipmentSlot::Amulet, EquipmentSlot::Belt,
                         EquipmentSlot::RingLeft, EquipmentSlot::RingRight}) {
    if ((slot_id == EQUIP_SLOT_GENERIC || slot_id == EquipmentSlotPacketId(slot)) &&
        EquipmentStateField(player.state, slot) == *item_instance) matches.push_back(slot);
  }
  if (cancelled_request) {
    AdvanceActorCommand(player);
    AdvancePlayerStateSequence(player);
    auto correction = MakePlayerStateUpdatePacket(player);
    correction.state.correction_mask = kSyncEquipment | kSyncCombat;
    BroadcastToRelevant(player_manager_, player, correction, TrafficClass::Control);
  }
  bool changed = cancelled_request;
  for (const auto slot : matches) changed = UnequipItemInSlot(player_id, slot) || changed;
  return changed;
}

bool GameServer::EquipArmor(PlayerId player_id, const std::string& instance) {
  return EquipItemInSlot(player_id, instance, EquipmentSlot::Armor);
}

bool GameServer::UnequipArmor(PlayerId player_id) {
  return UnequipItemInSlot(player_id, EquipmentSlot::Armor);
}

bool GameServer::EquipMeleeWeapon(PlayerId player_id, const std::string& instance) {
  return EquipItemInSlot(player_id, instance, EquipmentSlot::MeleeWeapon);
}

bool GameServer::UnequipMeleeWeapon(PlayerId player_id) {
  return UnequipItemInSlot(player_id, EquipmentSlot::MeleeWeapon);
}

bool GameServer::EquipRangedWeapon(PlayerId player_id, const std::string& instance) {
  return EquipItemInSlot(player_id, instance, EquipmentSlot::RangedWeapon);
}

bool GameServer::UnequipRangedWeapon(PlayerId player_id) {
  return UnequipItemInSlot(player_id, EquipmentSlot::RangedWeapon);
}

bool GameServer::EquipHelmet(PlayerId player_id, const std::string& instance) {
  return EquipItemInSlot(player_id, instance, EquipmentSlot::Helmet);
}

bool GameServer::UnequipHelmet(PlayerId player_id) {
  return UnequipItemInSlot(player_id, EquipmentSlot::Helmet);
}

bool GameServer::EquipShield(PlayerId player_id, const std::string& instance) {
  return EquipItemInSlot(player_id, instance, EquipmentSlot::Shield);
}

bool GameServer::UnequipShield(PlayerId player_id) {
  return UnequipItemInSlot(player_id, EquipmentSlot::Shield);
}

bool GameServer::EquipItemInSlot(PlayerId player_id, const std::string& instance, EquipmentSlot slot) {
  auto actor = GetCharacter(player_id);
  const auto item_instance = ResolveItemInstance(instance);
  if (!actor || !item_instance) return false;
  const auto instance_name = *item_instance;
  const auto previous = EquipmentStateField(actor->get().state, slot);
  const auto generation = actor->get().actor_generation;
  const auto revision = actor->get().command_revision;
  const auto packet_slot = EquipmentSlotPacketId(slot);
  if (previous != instance_name && !TriggerEquipmentEvent(player_id, slot, instance_name,
       !previous.empty() ? std::optional<std::string>(previous) : std::nullopt)) return false;
  actor = GetCharacter(player_id);
  if (!actor || actor->get().actor_generation != generation) return false;
  if (IsNewActorSequence(actor->get().equipment_command_revisions[packet_slot], revision)) return true;
  auto& player = actor->get();
  const auto other_ring = slot == EquipmentSlot::RingLeft ? player.state.equipped_ring_right_instance :
                          slot == EquipmentSlot::RingRight ? player.state.equipped_ring_left_instance : std::string{};
  const auto required_count = other_ring == instance_name ? 2 : 1;
  const auto missing_count = required_count - player.InventoryCount(*item_instance);
  // Script equip helpers grant a missing item; client requests still require ownership.
  if (missing_count > 0 && !GiveItem(player_id, *item_instance, missing_count)) return false;
  EquipmentStateField(player.state, slot) = instance_name;
  AdvanceActorCommand(player);
  AdvancePlayerStateSequence(player);
  const auto npc = npc_manager_.GetNpc(player_id);
  if (player_manager_.GetConnectionHandle(player_id) || (npc && npc->get().host_player_id != 0)) {
    PendingEquipmentStateField(player, slot) = instance_name;
    player.pending_equipment_commands[packet_slot] = {player.command_revision, instance_name, previous,
                                                     std::chrono::steady_clock::now() + std::chrono::seconds(5)};
  }
  player.equipment_command_revisions[packet_slot] = player.command_revision;
  EquipItemPacket packet{};
  packet.packet_type = PT_EQUIPITEM;
  packet.player_id = player_id;
  packet.item_instance = instance_name;
  packet.slot_id = packet_slot;
  packet.actor_generation = generation;
  packet.command_revision = player.command_revision;
  packet.state_sequence = player.state_sequence;
  BroadcastToRelevant(player_manager_, player, packet, TrafficClass::Control);
  return true;
}

bool GameServer::UnequipItemInSlot(PlayerId player_id, EquipmentSlot slot) {
  auto actor = GetCharacter(player_id);
  if (!actor) return false;
  const auto previous = EquipmentStateField(actor->get().state, slot);
  if (previous.empty()) return false;
  const auto generation = actor->get().actor_generation;
  const auto revision = actor->get().command_revision;
  const auto packet_slot = EquipmentSlotPacketId(slot);
  if (!TriggerEquipmentEvent(player_id, slot, std::nullopt, previous)) return false;
  actor = GetCharacter(player_id);
  if (!actor || actor->get().actor_generation != generation) return false;
  if (IsNewActorSequence(actor->get().equipment_command_revisions[packet_slot], revision)) return true;
  auto& player = actor->get();
  EquipmentStateField(player.state, slot).clear();
  AdvanceActorCommand(player);
  AdvancePlayerStateSequence(player);
  const auto npc = npc_manager_.GetNpc(player_id);
  if (player_manager_.GetConnectionHandle(player_id) || (npc && npc->get().host_player_id != 0)) {
    PendingEquipmentStateField(player, slot) = std::string{};
    player.pending_equipment_commands[packet_slot] = {player.command_revision, {}, previous,
                                                     std::chrono::steady_clock::now() + std::chrono::seconds(5)};
  }
  player.equipment_command_revisions[packet_slot] = player.command_revision;
  UnequipItemPacket packet{};
  packet.packet_type = PT_UNEQUIPITEM;
  packet.player_id = player_id;
  packet.item_instance = previous;
  packet.slot_id = packet_slot;
  packet.actor_generation = generation;
  packet.command_revision = player.command_revision;
  packet.state_sequence = player.state_sequence;
  BroadcastToRelevant(player_manager_, player, packet, TrafficClass::Control);
  return true;
}

std::int16_t GameServer::EquipmentSlotPacketId(EquipmentSlot slot) const {
  switch (slot) {
    case EquipmentSlot::Armor:
      return EQUIP_SLOT_ARMOR;
    case EquipmentSlot::Helmet:
      return EQUIP_SLOT_HELMET;
    case EquipmentSlot::Shield:
      return EQUIP_SLOT_SHIELD;
    case EquipmentSlot::MeleeWeapon:
      return EQUIP_SLOT_MELEE_WEAPON;
    case EquipmentSlot::RangedWeapon:
      return EQUIP_SLOT_RANGED_WEAPON;
    case EquipmentSlot::Amulet: return EQUIP_SLOT_AMULET;
    case EquipmentSlot::Belt: return EQUIP_SLOT_BELT;
    case EquipmentSlot::RingLeft: return EQUIP_SLOT_RING_LEFT;
    case EquipmentSlot::RingRight: return EQUIP_SLOT_RING_RIGHT;
  }

  return EQUIP_SLOT_GENERIC;
}

std::string& GameServer::EquipmentStateField(PlayerState& state, EquipmentSlot slot) const {
  switch (slot) {
    case EquipmentSlot::Armor:
      return state.equipped_armor_instance;
    case EquipmentSlot::Helmet:
      return state.equipped_helmet_instance;
    case EquipmentSlot::Shield:
      return state.equipped_shield_instance;
    case EquipmentSlot::MeleeWeapon:
      return state.melee_weapon_instance;
    case EquipmentSlot::RangedWeapon:
      return state.ranged_weapon_instance;
    case EquipmentSlot::Amulet: return state.equipped_amulet_instance;
    case EquipmentSlot::Belt: return state.equipped_belt_instance;
    case EquipmentSlot::RingLeft: return state.equipped_ring_left_instance;
    case EquipmentSlot::RingRight: return state.equipped_ring_right_instance;
  }

  return state.equipped_armor_instance;
}

std::optional<std::string>& GameServer::PendingEquipmentStateField(Character& player, EquipmentSlot slot) const {
  switch (slot) {
    case EquipmentSlot::Armor:
      return player.pending_equipped_armor_instance;
    case EquipmentSlot::Helmet:
      return player.pending_equipped_helmet_instance;
    case EquipmentSlot::Shield:
      return player.pending_equipped_shield_instance;
    case EquipmentSlot::MeleeWeapon:
      return player.pending_melee_weapon_instance;
    case EquipmentSlot::RangedWeapon:
      return player.pending_ranged_weapon_instance;
    case EquipmentSlot::Amulet: return player.pending_equipped_amulet_instance;
    case EquipmentSlot::Belt: return player.pending_equipped_belt_instance;
    case EquipmentSlot::RingLeft: return player.pending_equipped_ring_left_instance;
    case EquipmentSlot::RingRight: return player.pending_equipped_ring_right_instance;
  }

  return player.pending_equipped_armor_instance;
}

bool GameServer::TriggerEquipmentEvent(PlayerId player_id, EquipmentSlot slot,
                                      std::optional<std::string> item_instance,
                                      std::optional<std::string> previous_instance) {
  // A callback may use equipItem/unequipItem to implement the cancelled request.
  // Those server commands must not re-enter the request handler.
  if (!equipment_event_actors_.insert(player_id).second) return true;
  struct EventGuard {
    std::unordered_set<PlayerId>& actors;
    PlayerId id;
    ~EventGuard() { actors.erase(id); }
  } guard{equipment_event_actors_, player_id};
  EventManager::EventDispatchResult result;
  switch (slot) {
    case EquipmentSlot::Armor:
      result = EventManager::Instance().DispatchEvent(kEventOnPlayerEquipArmorName, OnPlayerEquipArmorEvent{player_id, item_instance, previous_instance}); break;
    case EquipmentSlot::Helmet:
      result = EventManager::Instance().DispatchEvent(kEventOnPlayerEquipHelmetName,
                                                      OnPlayerEquipHelmetEvent{player_id, item_instance, previous_instance}); break;
    case EquipmentSlot::Shield:
      result = EventManager::Instance().DispatchEvent(kEventOnPlayerEquipShieldName,
                                                      OnPlayerEquipShieldEvent{player_id, item_instance, previous_instance}); break;
    case EquipmentSlot::MeleeWeapon:
      result = EventManager::Instance().DispatchEvent(kEventOnPlayerEquipMeleeWeaponName,
                                                      OnPlayerEquipMeleeWeaponEvent{player_id, item_instance, previous_instance}); break;
    case EquipmentSlot::RangedWeapon:
      result = EventManager::Instance().DispatchEvent(kEventOnPlayerEquipRangedWeaponName,
                                                      OnPlayerEquipRangedWeaponEvent{player_id, item_instance, previous_instance}); break;
    case EquipmentSlot::Amulet:
      result = EventManager::Instance().DispatchEvent(kEventOnPlayerEquipAmuletName,
                                                      OnPlayerEquipAmuletEvent{player_id, item_instance, previous_instance}); break;
    case EquipmentSlot::Belt:
      result = EventManager::Instance().DispatchEvent(kEventOnPlayerEquipBeltName, OnPlayerEquipBeltEvent{player_id, item_instance, previous_instance}); break;
    case EquipmentSlot::RingLeft:
    case EquipmentSlot::RingRight:
      result = EventManager::Instance().DispatchEvent(
          kEventOnPlayerEquipRingName,
          OnPlayerEquipRingEvent{player_id, static_cast<std::uint8_t>(slot == EquipmentSlot::RingLeft ? 0 : 1),
                                item_instance, previous_instance}); break;
  }
  return !result.cancelled;
}

std::int32_t GameServer::HasItem(PlayerId player_id, const std::string& instance) const {
  auto player_opt = GetCharacter(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("hasItem called for unknown player id {}", player_id);
    return 0;
  }

  auto item_instance = ResolveItemInstance(instance);
  if (!item_instance.has_value()) {
    return 0;
  }

  const auto& player = player_opt->get();
  const auto it = player.inventory.find(*item_instance);
  if (it == player.inventory.end()) {
    return 0;
  }

  return std::max<std::int32_t>(0, it->second);
}

void GameServer::ValidatePlayerEquipment(Character& character, PlayerState& incoming) const {
  if (character.transformation) {
    incoming.left_hand_item_instance.clear();
    incoming.right_hand_item_instance.clear();
    for (const auto slot : {EquipmentSlot::Armor, EquipmentSlot::Helmet, EquipmentSlot::Shield, EquipmentSlot::MeleeWeapon,
                           EquipmentSlot::RangedWeapon, EquipmentSlot::Amulet, EquipmentSlot::Belt,
                           EquipmentSlot::RingLeft, EquipmentSlot::RingRight}) EquipmentStateField(incoming, slot).clear();
    incoming.active_spell_instance.clear();
    incoming.active_spell_nr = incoming.active_spell_level = 0;
    incoming.weapon_mode = gmp::kTransformationWeaponMode;
    incoming.spell_investing = false;
    return;
  }
  for (const auto slot : {EquipmentSlot::Armor, EquipmentSlot::Helmet, EquipmentSlot::Shield, EquipmentSlot::MeleeWeapon,
                         EquipmentSlot::RangedWeapon, EquipmentSlot::Amulet, EquipmentSlot::Belt,
                         EquipmentSlot::RingLeft, EquipmentSlot::RingRight}) {
    auto& field = EquipmentStateField(incoming, slot);
    if (character.pending_equipment_commands.contains(EquipmentSlotPacketId(slot)) ||
        (!field.empty() && character.InventoryCount(field) == 0)) {
      const auto& previous = EquipmentStateField(character.state, slot);
      field = character.InventoryCount(previous) > 0 ? previous : std::string{};
    }
  }
  if (!incoming.equipped_ring_left_instance.empty() && incoming.equipped_ring_left_instance == incoming.equipped_ring_right_instance &&
      character.InventoryCount(incoming.equipped_ring_left_instance) < 2) incoming.equipped_ring_right_instance.clear();
  int unlit = character.InventoryCount("ITLSTORCH");
  int burning = character.InventoryCount("ITLSTORCHBURNING");
  bool held = false;
  for (auto* hand : {&incoming.left_hand_item_instance, &incoming.right_hand_item_instance}) {
    if (*hand == "ITLSTORCHBURNING") {
      if (held || (burning <= 0 && unlit <= 0)) hand->clear();
      else { held = true; if (burning > 0) --burning; else --unlit; }
    } else if (*hand == "ITLSTORCH") {
      if (unlit > 0) --unlit;
      else hand->clear();
    } else if (!hand->empty() && character.InventoryCount(*hand) == 0) hand->clear();
  }
  if (character.InventoryCount(incoming.active_spell_instance) == 0) {
    incoming.active_spell_instance.clear();
    incoming.active_spell_nr = 0;
  }
  if (incoming.active_spell_instance.empty() || incoming.weapon_mode != 7) {
    incoming.active_spell_level = 0;
    incoming.spell_investing = false;
  }
}

std::optional<GameServer::EquipmentSlot> GameServer::PacketEquipmentSlot(std::int16_t slot_id) const {
  for (const auto slot : {EquipmentSlot::Armor, EquipmentSlot::Helmet, EquipmentSlot::Shield, EquipmentSlot::MeleeWeapon,
                         EquipmentSlot::RangedWeapon, EquipmentSlot::Amulet, EquipmentSlot::Belt,
                         EquipmentSlot::RingLeft, EquipmentSlot::RingRight}) {
    if (EquipmentSlotPacketId(slot) == slot_id) return slot;
  }
  return std::nullopt;
}

void GameServer::HandleEquipmentResult(Packet p) {
  EquipmentResultPacket packet{};
  if (!DeserializeClientPacket(p, packet, GetPacketIdentifier(p), "EquipmentResultPacket")) return;
  auto sender = GetIngamePlayerByConnection(p.id);
  const auto slot = PacketEquipmentSlot(packet.slot_id);
  if (!sender || (!slot && packet.slot_id != EQUIP_SLOT_GENERIC)) return;
  const auto actor_id = packet.actor_id ? packet.actor_id : sender->get().player_id;
  auto player = GetCharacter(actor_id);
  if (!player || packet.actor_generation != player->get().actor_generation) return;
  if (actor_id != sender->get().player_id) {
    const auto npc = npc_manager_.GetNpc(actor_id);
    if (!npc || npc->get().host_player_id != sender->get().player_id || npc->get().control_epoch != packet.control_epoch ||
        !IsEligibleNpcHost(npc->get(), sender->get().player_id)) return;
  }
  if (packet.slot_id == EQUIP_SLOT_GENERIC) {
    const auto pending = player->get().pending_equipment_commands.find(EQUIP_SLOT_GENERIC);
    if (pending == player->get().pending_equipment_commands.end() ||
        pending->second.command_revision != packet.command_revision || pending->second.expected_instance != packet.item_instance) return;
    const auto command = pending->second;
    const auto generation = player->get().actor_generation;
    player->get().pending_equipment_commands.erase(pending);
    if (!player->get().generic_equipment_requests.empty() && player->get().generic_equipment_requests.front() == command.expected_instance)
      player->get().generic_equipment_requests.erase(player->get().generic_equipment_requests.begin());
    const auto resolved = PacketEquipmentSlot(packet.observed_slot);
    if (packet.success && resolved && packet.observed_instance == command.expected_instance &&
        player->get().InventoryCount(command.expected_instance) > 0 &&
        !IsNewActorSequence(player->get().equipment_command_revisions[packet.observed_slot], command.command_revision)) {
      const auto previous = EquipmentStateField(player->get().state, *resolved);
      const bool allowed = previous == command.expected_instance || TriggerEquipmentEvent(player->get().player_id, *resolved,
          command.expected_instance, previous.empty() ? std::nullopt : std::optional<std::string>{previous});
      player = GetCharacter(actor_id);
      if (!player || player->get().actor_generation != generation) return;
      if (allowed && !IsNewActorSequence(player->get().equipment_command_revisions[packet.observed_slot], command.command_revision) &&
          player->get().InventoryCount(command.expected_instance) > 0) {
        EquipmentStateField(player->get().state, *resolved) = command.expected_instance;
        ValidatePlayerEquipment(player->get(), player->get().state);
      }
    }
    AdvanceActorCommand(player->get());
    AdvancePlayerStateSequence(player->get());
    auto correction = MakePlayerStateUpdatePacket(player->get());
    correction.state.correction_mask = kSyncEquipment | kSyncCombat;
    BroadcastToRelevant(player_manager_, player->get(), correction, TrafficClass::Control);
    return;
  }
  const auto pending = player->get().pending_equipment_commands.find(packet.slot_id);
  if (pending == player->get().pending_equipment_commands.end() ||
      pending->second.command_revision != packet.command_revision ||
      pending->second.expected_instance != packet.item_instance) return;
  CompleteEquipmentCommand(player->get().player_id, *slot,
                            packet.success && packet.observed_instance == pending->second.expected_instance);
}

void GameServer::SendEquipmentCorrection(Character& character, EquipmentSlot slot, const std::string& rejected_instance) {
  const auto item_instance = EquipmentStateField(character.state, slot);
  const auto revision = AdvanceActorCommand(character);
  const auto sequence = AdvancePlayerStateSequence(character);
  if (!item_instance.empty()) {
    EquipItemPacket packet{};
    packet.packet_type = PT_EQUIPITEM;
    packet.player_id = character.player_id;
    packet.item_instance = item_instance;
    packet.slot_id = EquipmentSlotPacketId(slot);
    packet.actor_generation = character.actor_generation;
    packet.command_revision = revision;
    packet.state_sequence = sequence;
    BroadcastToRelevant(player_manager_, character, packet, TrafficClass::Control);
  } else {
    UnequipItemPacket packet{};
    packet.packet_type = PT_UNEQUIPITEM;
    packet.player_id = character.player_id;
    packet.item_instance = rejected_instance;
    packet.slot_id = EquipmentSlotPacketId(slot);
    packet.actor_generation = character.actor_generation;
    packet.command_revision = revision;
    packet.state_sequence = sequence;
    BroadcastToRelevant(player_manager_, character, packet, TrafficClass::Control);
  }
}

void GameServer::CompleteEquipmentCommand(PlayerId player_id, EquipmentSlot slot, bool success) {
  auto actor = GetCharacter(player_id);
  if (!actor) return;
  auto& character = actor->get();
  const auto packet_slot = EquipmentSlotPacketId(slot);
  const auto pending = character.pending_equipment_commands.find(packet_slot);
  if (pending == character.pending_equipment_commands.end()) return;
  const auto command = pending->second;
  character.pending_equipment_commands.erase(pending);
  PendingEquipmentStateField(character, slot).reset();
  if (success) return;

  auto restored = character.InventoryCount(command.previous_instance) > 0 ? command.previous_instance : std::string{};
  if (!restored.empty() && ((slot == EquipmentSlot::RingLeft && character.state.equipped_ring_right_instance == restored) ||
                   (slot == EquipmentSlot::RingRight && character.state.equipped_ring_left_instance == restored)) &&
      character.InventoryCount(command.previous_instance) < 2) restored.clear();
  EquipmentStateField(character.state, slot) = restored;
  if (++character.command_revision == 0) ++character.command_revision;
  AdvancePlayerStateSequence(character);
  if (!restored.empty()) {
    EquipItemPacket packet{};
    packet.packet_type = PT_EQUIPITEM;
    packet.player_id = player_id;
    packet.item_instance = restored;
    packet.slot_id = packet_slot;
    packet.actor_generation = character.actor_generation;
    packet.command_revision = character.command_revision;
    packet.state_sequence = character.state_sequence;
    BroadcastToRelevant(player_manager_, character, packet, TrafficClass::Control);
  } else {
    UnequipItemPacket packet{};
    packet.packet_type = PT_UNEQUIPITEM;
    packet.player_id = player_id;
    packet.item_instance = !command.expected_instance.empty() ? command.expected_instance : command.previous_instance;
    packet.slot_id = packet_slot;
    packet.actor_generation = character.actor_generation;
    packet.command_revision = character.command_revision;
    packet.state_sequence = character.state_sequence;
    BroadcastToRelevant(player_manager_, character, packet, TrafficClass::Control);
  }
  const auto generation = character.actor_generation;
  pending_npc_events_.push_back([this, player_id, generation, slot, restored] {
    const auto actor = GetCharacter(player_id);
    if (!actor || actor->get().actor_generation != generation) return;
    TriggerEquipmentEvent(player_id, slot, !restored.empty() ? std::optional<std::string>(restored) : std::nullopt);
  });
}

void GameServer::ProcessGenericEquipment(PlayerId player_id) {
  auto actor = GetCharacter(player_id);
  if (!actor || actor->get().instance_change || actor->get().generic_equipment_requests.empty()) return;
  auto& current = actor->get();
  if (current.pending_equipment_commands.contains(EQUIP_SLOT_GENERIC)) return;
  const auto npc = npc_manager_.GetNpc(player_id);
  if (npc) {
    if (!npc->get().host_player_id || !IsEligibleNpcHost(npc->get(), npc->get().host_player_id)) return;
  } else {
    const auto player = player_manager_.GetPlayer(player_id);
    if (!player || !player->get().npc_world_ready) return;
  }
  const auto instance = current.generic_equipment_requests.front();
  if (current.InventoryCount(instance) == 0) {
    current.generic_equipment_requests.erase(current.generic_equipment_requests.begin());
    return;
  }
  AdvanceActorCommand(current);
  AdvancePlayerStateSequence(current);
  current.pending_equipment_commands[EQUIP_SLOT_GENERIC] = {
      current.command_revision, instance, {}, std::chrono::steady_clock::now() + std::chrono::seconds(5)};
  EquipItemPacket packet{};
  packet.packet_type = PT_EQUIPITEM;
  packet.player_id = player_id;
  packet.item_instance = instance;
  packet.slot_id = EQUIP_SLOT_GENERIC;
  packet.actor_generation = current.actor_generation;
  packet.command_revision = current.command_revision;
  packet.state_sequence = current.state_sequence;
  BroadcastToRelevant(player_manager_, current, packet, TrafficClass::Control);
}

void GameServer::ProcessEquipmentCommands() {
  const auto now = std::chrono::steady_clock::now();
  std::vector<std::pair<PlayerId, EquipmentSlot>> expired;
  std::vector<PlayerId> actors;
  for (const auto& [id, player] : player_manager_.GetAllPlayers()) {
    actors.push_back(id);
    for (const auto& [slot_id, command] : player.pending_equipment_commands) {
      if (player.instance_change || command.deadline > now) continue;
      if (const auto slot = PacketEquipmentSlot(slot_id)) expired.emplace_back(id, *slot);
    }
  }
  for (const auto id : npc_manager_.GetNpcIds()) {
    auto npc = npc_manager_.GetNpc(id);
    if (!npc) continue;
    actors.push_back(id);
    for (const auto& [slot_id, command] : npc->get().pending_equipment_commands) {
      if (npc->get().instance_change || command.deadline > now) continue;
      if (const auto slot = PacketEquipmentSlot(slot_id)) expired.emplace_back(id, *slot);
    }
  }
  for (const auto& [id, slot] : expired) CompleteEquipmentCommand(id, slot, false);
  for (const auto id : actors) {
    auto actor = GetCharacter(id);
    if (!actor) continue;
    const auto pending = actor->get().pending_equipment_commands.find(EQUIP_SLOT_GENERIC);
    if (pending != actor->get().pending_equipment_commands.end() && pending->second.deadline <= now && !actor->get().instance_change) {
      actor->get().pending_equipment_commands.erase(pending);
      if (!actor->get().generic_equipment_requests.empty()) actor->get().generic_equipment_requests.erase(actor->get().generic_equipment_requests.begin());
      AdvanceActorCommand(actor->get());
      AdvancePlayerStateSequence(actor->get());
      auto correction = MakePlayerStateUpdatePacket(actor->get());
      correction.state.correction_mask = kSyncEquipment | kSyncCombat;
      BroadcastToRelevant(player_manager_, actor->get(), correction, TrafficClass::Control);
    }
    ProcessGenericEquipment(id);
  }
}

void GameServer::ClearRemovedEquipment(Character& character, const std::string& item_instance, std::int32_t remaining_amount) {
  if (item_instance == "ITLSTORCH" || item_instance == "ITLSTORCHBURNING") {
    auto validated = character.state;
    ValidatePlayerEquipment(character, validated);
    if (validated.left_hand_item_instance != character.state.left_hand_item_instance ||
        validated.right_hand_item_instance != character.state.right_hand_item_instance) {
      character.state.left_hand_item_instance = validated.left_hand_item_instance;
      character.state.right_hand_item_instance = validated.right_hand_item_instance;
      AdvanceActorCommand(character);
      AdvancePlayerStateSequence(character);
      auto correction = MakePlayerStateUpdatePacket(character);
      correction.state.correction_mask = kSyncEquipment;
      BroadcastToRelevant(player_manager_, character, correction, TrafficClass::Control);
    }
  }
  // Removing the last owned copy also clears its presentation and pending command.
  std::int32_t kept_rings = 0;
  for (const auto slot : {EquipmentSlot::Armor, EquipmentSlot::Helmet, EquipmentSlot::Shield, EquipmentSlot::MeleeWeapon,
                         EquipmentSlot::RangedWeapon, EquipmentSlot::Amulet, EquipmentSlot::Belt,
                         EquipmentSlot::RingLeft, EquipmentSlot::RingRight}) {
    auto& field = EquipmentStateField(character.state, slot);
    if (field != item_instance) continue;
    const bool ring = slot == EquipmentSlot::RingLeft || slot == EquipmentSlot::RingRight;
    if (remaining_amount > 0 && (!ring || ++kept_rings <= remaining_amount)) continue;
    field.clear();
    const auto slot_id = EquipmentSlotPacketId(slot);
    PendingEquipmentStateField(character, slot).reset();
    character.pending_equipment_commands.erase(slot_id);
    if (++character.command_revision == 0) ++character.command_revision;
    AdvancePlayerStateSequence(character);
    UnequipItemPacket packet{};
    packet.packet_type = PT_UNEQUIPITEM;
    packet.player_id = character.player_id;
    packet.item_instance = item_instance;
    packet.slot_id = slot_id;
    packet.actor_generation = character.actor_generation;
    packet.command_revision = character.command_revision;
    packet.state_sequence = character.state_sequence;
    BroadcastToRelevant(player_manager_, character, packet, TrafficClass::Control);
    const auto id = character.player_id;
    const auto generation = character.actor_generation;
    pending_npc_events_.push_back([this, id, generation, slot] {
      const auto actor = GetCharacter(id);
      if (actor && actor->get().actor_generation == generation) TriggerEquipmentEvent(id, slot, std::nullopt);
    });
  }
}

bool GameServer::RemoveItem(PlayerId player_id, const std::string& instance, std::int32_t amount) {
  auto player_opt = GetCharacter(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("removeItem called for unknown player id {}", player_id);
    return false;
  }

  if (amount <= 0) {
    return false;
  }

  auto item_instance = ResolveItemInstance(instance);
  if (!item_instance.has_value()) {
    SPDLOG_WARN("removeItem called with unknown item instance '{}'", SanitizeServerText(instance));
    return false;
  }

  auto& player = player_opt->get();
  const auto actor_generation = player.actor_generation;
  const auto removed_amount = std::min(amount, player.InventoryCount(*item_instance));
  if (!RemoveInventoryItem(player, *item_instance, removed_amount)) return false;
  if (player.instance_change) player.instance_change->inventory_deltas[*item_instance] -= removed_amount;
  ClearRemovedEquipment(player, *item_instance, player.InventoryCount(*item_instance));
  // Unequip callbacks may remove or replace this character.
  player_opt = GetCharacter(player_id);
  if (!player_opt || player_opt->get().actor_generation != actor_generation) return false;
  auto& current = player_opt->get();
  if (++current.command_revision == 0) ++current.command_revision;
  AdvancePlayerStateSequence(current);

  RemoveItemPacket packet{};
  packet.packet_type = PT_REMOVEITEM;
  packet.player_id = player_id;
  packet.item_instance = *item_instance;
  packet.item_amount = removed_amount;
  packet.actor_generation = current.actor_generation;
  packet.command_revision = current.command_revision;
  packet.state_sequence = current.state_sequence;

  BroadcastToRelevant(player_manager_, current, packet, TrafficClass::Control);
  return true;
}

std::optional<glm::vec3> GameServer::GetCharacterPosition(PlayerId player_id) const {
  auto player_opt = GetCharacter(player_id);
  if (!player_opt.has_value()) {
    return std::nullopt;
  }

  const auto& player = player_opt->get();
  return player.state.position;
}

std::string GameServer::GetHostname() const {
  return config_.Get<std::string>("name");
}

std::uint32_t GameServer::GetMaxSlots() const {
  return static_cast<std::uint32_t>(config_.Get<std::int32_t>("slots"));
}

bool GameServer::SetServerWorld(const std::string& world) {
  auto sanitized_world = SanitizeWorldName(world);
  server_world_ = sanitized_world;
  config_.Set<std::string>("map", sanitized_world);
  return true;
}

std::string GameServer::GetServerWorld() const {
  return server_world_;
}

std::vector<GameServer::PlayerId> GameServer::FindNearbyPlayers(const glm::vec3& position, float radius, const std::string& world,
                                                                std::int32_t virtual_world) const {
  std::vector<PlayerId> nearby_players;
  if (radius < 0.0f) {
    return nearby_players;
  }

  const auto sanitized_world = SanitizeWorldName(world);
  const float radius_squared = radius * radius;
  nearby_players.reserve(player_manager_.GetPlayerCount());

  player_manager_.ForEachIngamePlayer([&](const Player& player) {
    if (!sanitized_world.empty() && player.world != sanitized_world) {
      return;
    }

    if (player.virtual_world != virtual_world) {
      return;
    }

    const auto delta = player.state.position - position;
    if (glm::dot(delta, delta) <= radius_squared) {
      nearby_players.push_back(player.player_id);
    }
  });

  return nearby_players;
}

std::vector<GameServer::PlayerId> GameServer::GetCharactersStreamedToPlayer(PlayerId player_id) const {
  std::vector<PlayerId> spawned_players;
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value() || !player_opt->get().is_ingame) {
    return spawned_players;
  }

  const auto& player = player_opt->get();
  spawned_players.reserve(player.spawned_players.size());
  spawned_players.insert(spawned_players.end(), player.spawned_players.begin(), player.spawned_players.end());

  return spawned_players;
}

std::vector<GameServer::PlayerId> GameServer::GetPlayersStreamingCharacter(PlayerId player_id) const {
  std::vector<PlayerId> streaming_players;
  auto player_opt = GetCharacter(player_id);
  if (!player_opt.has_value() || !player_opt->get().is_ingame) {
    return streaming_players;
  }

  const auto& player = player_opt->get();
  streaming_players.reserve(player.streamed_by_players.size());
  streaming_players.insert(streaming_players.end(), player.streamed_by_players.begin(), player.streamed_by_players.end());

  return streaming_players;
}

bool GameServer::SetStreamerRadius(std::int32_t radius) {
  if (radius < 0) {
    SPDLOG_WARN("setStreamerRadius called with invalid radius {}", radius);
    return false;
  }

  config_.Set<std::int32_t>("stream_radius", radius);
  return true;
}

std::int32_t GameServer::GetStreamerRadius() const {
  return config_.Get<std::int32_t>("stream_radius");
}

bool GameServer::SetStreamerHeight(std::int32_t height) {
  if (height < 0) {
    SPDLOG_WARN("setStreamerHeight called with invalid height {}", height);
    return false;
  }

  config_.Set<std::int32_t>("stream_height", height);
  return true;
}

std::int32_t GameServer::GetStreamerHeight() const {
  return config_.Get<std::int32_t>("stream_height");
}

bool GameServer::SetVoiceEnabled(bool enabled) {
  if (config_.Get<bool>("voice_enabled") == enabled) {
    return true;
  }

  config_.Set<bool>("voice_enabled", enabled);
  if (!enabled) {
    std::vector<PlayerId> active_players;
    active_players.reserve(active_voice_players_.size());
    for (const auto& [player_id, last_packet_at] : active_voice_players_) {
      (void)last_packet_at;
      active_players.push_back(player_id);
    }
    for (PlayerId player_id : active_players) {
      EndPlayerVoiceActivity(player_id);
    }
  }

  player_manager_.ForEachPlayer([this](Player& player) { SendVoiceConfiguration(player); });
  return true;
}

bool GameServer::SetVoiceRange(std::int32_t range) {
  if (range < 0) {
    SPDLOG_WARN("setVoiceRange called with invalid range {}", range);
    return false;
  }

  config_.Set<std::int32_t>("voice_range", range);
  player_manager_.ForEachPlayer([this, range](Player& player) {
    player.voice_range = static_cast<std::uint32_t>(range);
    if (range == 0 && IsVoiceChannelSpatial(player.voice_channel)) {
      EndPlayerVoiceActivity(player.player_id);
    }
    SendVoiceConfiguration(player);
  });
  return true;
}

bool GameServer::SetVoiceEnabledForPlayer(PlayerId player_id, bool enabled) {
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    return false;
  }

  Player& player = player_opt->get();
  const bool muted = !enabled;
  if (player.voice_muted == muted) {
    return true;
  }

  player.voice_muted = muted;
  if (muted) {
    EndPlayerVoiceActivity(player_id);
  }
  SendVoiceConfiguration(player);
  return true;
}

bool GameServer::GetVoiceEnabled() const {
  return config_.Get<bool>("voice_enabled");
}

std::int32_t GameServer::GetVoiceRange() const {
  return config_.Get<std::int32_t>("voice_range");
}

std::optional<bool> GameServer::GetVoiceEnabledForPlayer(PlayerId player_id) const {
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    return std::nullopt;
  }
  return !player_opt->get().voice_muted;
}

bool GameServer::SetTime(std::int32_t hour, std::int32_t min, std::int32_t day) {
  if (!clock_) {
    return false;
  }

  if (hour < 0 || hour > 23 || min < 0 || min > 59 || day < 0) {
    SPDLOG_WARN("setTime called with invalid parameters: day={}, hour={}, min={}", day, hour, min);
    return false;
  }

  auto current_time = clock_->GetTime();
  GothicClock::Time new_time{static_cast<std::uint16_t>(day == 0 ? current_time.day_ : day), static_cast<std::uint8_t>(hour),
                             static_cast<std::uint8_t>(min)};
  clock_->UpdateTime(new_time);

  EventManager::Instance().TriggerEvent(kEventOnClockUpdateName, OnClockUpdateEvent{new_time.day_, new_time.hour_, new_time.min_});

  BroadcastGameInfo();
  weather_.ResetLastSkyTime(new_time);
  last_weather_update_minute_ = -1;
  if (weather_.Update(new_time)) {
    BroadcastSkySettings();
  }
  return true;
}

GothicClock::Time GameServer::GetTime() const {
  if (!clock_) {
    return GothicClock::Time{};
  }

  return clock_->GetTime();
}

bool GameServer::SetDayLength(float day_length_ms) {
  if (!clock_) {
    return false;
  }

  if (!clock_->SetDayLengthMs(day_length_ms)) {
    return false;
  }

  BroadcastGameInfo();
  return true;
}

float GameServer::GetDayLength() const {
  if (!clock_) {
    return 0.0f;
  }

  return static_cast<float>(clock_->GetDayLengthMs());
}

bool GameServer::SetWeatherType(std::int32_t weather_type) {
  if (!weather_.SetWeatherType(weather_type)) {
    SPDLOG_WARN("Sky.weatherType assigned invalid weather type {}", weather_type);
    return false;
  }

  BroadcastSkySettings();
  return true;
}

std::int32_t GameServer::GetWeatherType() const {
  return weather_.GetWeatherType();
}

bool GameServer::SetRainStartTime(std::int32_t hour, std::int32_t min) {
  if (!weather_.SetRainStartTime(hour, min)) {
    SPDLOG_WARN("Sky.rainStartTime assigned invalid parameters: hour={}, min={}", hour, min);
    return false;
  }

  BroadcastSkySettings();
  return true;
}

std::pair<std::int32_t, std::int32_t> GameServer::GetRainStartTime() const {
  return weather_.GetRainStartTime();
}

bool GameServer::SetRainStopTime(std::int32_t hour, std::int32_t min) {
  if (!weather_.SetRainStopTime(hour, min)) {
    SPDLOG_WARN("Sky.rainStopTime assigned invalid parameters: hour={}, min={}", hour, min);
    return false;
  }

  BroadcastSkySettings();
  return true;
}

std::pair<std::int32_t, std::int32_t> GameServer::GetRainStopTime() const {
  return weather_.GetRainStopTime();
}

bool GameServer::SetWindScale(float wind_scale) {
  if (!weather_.SetWindScale(wind_scale)) {
    SPDLOG_WARN("Sky.windScale assigned non-finite value {}", wind_scale);
    return false;
  }

  BroadcastSkySettings();
  return true;
}

float GameServer::GetWindScale() const {
  return weather_.GetWindScale();
}

bool GameServer::SetDontRain(bool toggle) {
  weather_.SetDontRain(toggle);
  if (clock_) {
    weather_.Update(clock_->GetTime());
  }

  BroadcastSkySettings();
  return true;
}

bool GameServer::GetDontRain() const {
  return weather_.GetDontRain();
}

bool GameServer::SetWeatherDisabled(bool toggle) {
  weather_.SetDisabled(toggle);
  BroadcastSkySettings();
  return true;
}

bool GameServer::GetWeatherDisabled() const {
  return weather_.GetDisabled();
}

bool GameServer::KickPlayer(PlayerId player_id, const std::string& reason) {
  if (!g_transport) {
    SPDLOG_WARN("kickPlayer called before the server is initialized");
    return false;
  }

  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("kickPlayer called for unknown player id {}", player_id);
    return false;
  }

  const auto& player = player_opt->get();
  if (!reason.empty()) {
    SPDLOG_INFO("Kicking player {} ({}) for reason '{}'", player.player_id, player.name, reason);
  } else {
    SPDLOG_INFO("Kicking player {} ({})", player.player_id, player.name);
  }

  return transport_.CloseConnection(player.connection, CloseReason::Kicked, reason);
}

bool GameServer::BanPlayer(PlayerId player_id, const std::string& reason) {
  if (!g_transport) {
    SPDLOG_WARN("banPlayer called before the server is initialized");
    return false;
  }

  if (!ban_manager_) {
    SPDLOG_WARN("banPlayer called before ban manager is initialized");
    return false;
  }

  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("banPlayer called for unknown player id {}", player_id);
    return false;
  }

  const auto& player = player_opt->get();
  const std::string& ip = player.ip_address;
  if (ip.empty()) {
    SPDLOG_WARN("banPlayer could not resolve IP for player id {}", player_id);
    return false;
  }

  ban_manager_->AddBan(BanEntry{player.name, ip, FormatCurrentDateTime(), reason});
  if (!ban_manager_->Save()) {
    SPDLOG_ERROR("Ban for {} is active for this run but could not be persisted", ip);
  }
  transport_.CloseConnection(player.connection, CloseReason::Banned, reason);
  return true;
}

bool GameServer::IsPlayerConnected(PlayerId player_id) const {
  return player_manager_.HasPlayer(player_id);
}

bool GameServer::IsPlayerAdmin(PlayerId player_id) const {
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    return false;
  }

  return player_opt->get().is_admin;
}

bool GameServer::IsPlayerDead(PlayerId player_id) const {
  auto player_opt = GetCharacter(player_id);
  if (!player_opt.has_value()) {
    return false;
  }

  return player_opt->get().tod != 0;
}

bool GameServer::IsPlayerSpawned(PlayerId player_id) const {
  auto player_opt = GetCharacter(player_id);
  if (!player_opt.has_value()) {
    return false;
  }

  return player_opt->get().is_ingame != 0;
}

bool GameServer::IsPlayerUnconscious(PlayerId player_id) const {
  auto player_opt = GetCharacter(player_id);
  if (!player_opt.has_value()) {
    return false;
  }

  return (player_opt->get().flags & PlayerManager::PL_UNCONCIOUS) != 0;
}

bool GameServer::RespawnPlayer(PlayerId player_id) {
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("respawnPlayer called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  if (!player.is_ingame || player.tod == 0) {
    return false;
  }

  return RespawnPlayerInternal(player);
}

bool GameServer::SetPlayerRespawnTime(PlayerId player_id, std::int32_t respawn_time_ms) {
  if (respawn_time_ms < 0) return false;
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerRespawnTime called for unknown player id {}", player_id);
    return false;
  }

  player_opt->get().respawn_time_ms = respawn_time_ms;
  return true;
}

std::optional<std::int32_t> GameServer::GetPlayerRespawnTime(PlayerId player_id) const {
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    return std::nullopt;
  }

  if (player_opt->get().respawn_time_ms.has_value()) {
    return player_opt->get().respawn_time_ms.value();
  }

  auto default_respawn_seconds = config_.Get<std::int32_t>("respawn_time_seconds");
  if (default_respawn_seconds < 0) {
    return 0;
  }

  return static_cast<std::int32_t>(std::min<std::int64_t>(
      static_cast<std::int64_t>(default_respawn_seconds) * 1000, std::numeric_limits<std::int32_t>::max()));
}

std::uint32_t GameServer::GetPort() const {
  if (g_transport) {
    return g_transport->GetPort();
  }
  return 0;
}

std::string GameServer::GetPlayerIp(PlayerId player_id) const {
  const auto player = player_manager_.GetPlayer(player_id);
  return player ? player->get().ip_address : std::string{};
}
std::int32_t GameServer::GetPlayerPing(PlayerId player_id) const {
  if (!g_transport) {
    return -1;
  }

  auto connection_opt = player_manager_.GetConnectionHandle(player_id);
  if (!connection_opt.has_value()) {
    return -1;
  }

  return g_transport->GetPing(connection_opt.value());
}

std::string GameServer::GetPlayerMacAddress(PlayerId player_id) const {
  (void)player_id;
  return {};
}

std::string GameServer::GetPlayerIdentity(PlayerId player_id) const {
  const auto player = player_manager_.GetPlayer(player_id);
  return player ? player->get().identity : std::string{};
}
