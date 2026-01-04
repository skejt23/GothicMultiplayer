
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

#include "game_client.hpp"

#include <bitsery/adapter/buffer.h>
#include <bitsery/bitsery.h>
#include <httplib.h>
#include <sodium.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <dylib.hpp>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "net_enums.h"
#include "packets.h"
#include "shared/crypto_utils.h"
#include "znet_client.h"

namespace gmp::client {

using namespace Net;

Net::NetClient* g_netclient = nullptr;

template <typename TContainer = std::vector<std::uint8_t>, typename Packet>
static void SerializeAndSend(const Packet& packet, Net::PacketPriority priority, Net::PacketReliability reliable) {
  TContainer buffer;
  auto written_size = bitsery::quickSerialization<bitsery::OutputBufferAdapter<TContainer>>(buffer, packet);
  g_netclient->SendPacket(buffer.data(), written_size, reliable, priority);
}

GameClient::GameClient(EventObserver& eventObserver, gmp::TaskScheduler& taskScheduler)
    : event_observer_(eventObserver), task_scheduler_(taskScheduler), resource_downloader_(eventObserver, taskScheduler) {
  assert(g_netclient != nullptr);
  InitPacketHandlers();
  g_netclient->AddPacketHandler(*this);
}

GameClient::~GameClient() {
  Disconnect();
  assert(g_netclient != nullptr);

  // Clean up connection thread if still running
  {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    if (connection_thread_.joinable()) {
      connection_thread_.join();
    }
  }

  resource_downloader_.StopDownload();

  g_netclient->RemovePacketHandler(*this);
}

void GameClient::InitPacketHandlers() {
  packet_handlers_[PT_INITIAL_INFO] = [this](Packet p) { OnInitialInfo(p); };
  packet_handlers_[PT_ACTUAL_STATISTICS] = [this](Packet p) { OnActualStatistics(p); };
  packet_handlers_[PT_MAP_ONLY] = [this](Packet p) { OnMapOnly(p); };
  packet_handlers_[PT_DODIE] = [this](Packet p) { OnDoDie(p); };
  packet_handlers_[PT_RESPAWN] = [this](Packet p) { OnRespawn(p); };
  packet_handlers_[PT_CASTSPELL] = [this](Packet p) { OnCastSpell(p); };
  packet_handlers_[PT_CASTSPELLONTARGET] = [this](Packet p) { OnCastSpellOnTarget(p); };
  packet_handlers_[PT_DROPITEM] = [this](Packet p) { OnDropItem(p); };
  packet_handlers_[PT_TAKEITEM] = [this](Packet p) { OnTakeItem(p); };
  packet_handlers_[PT_GIVEITEM] = [this](Packet p) { OnGiveItem(p); };
  packet_handlers_[PT_EQUIPITEM] = [this](Packet p) { OnEquipItem(p); };
  packet_handlers_[PT_UNEQUIPITEM] = [this](Packet p) { OnUnequipItem(p); };
  packet_handlers_[PT_MSG] = [this](Packet p) { OnMessage(p); };
  packet_handlers_[PT_EXISTING_PLAYERS] = [this](Packet p) { OnExistingPlayers(p); };
  packet_handlers_[PT_PLAYER_SPAWN] = [this](Packet p) { OnPlayerSpawn(p); };
  packet_handlers_[PT_JOIN_GAME] = [this](Packet p) { OnJoinGame(p); };
  packet_handlers_[PT_PLAYER_NAME_UPDATE] = [this](Packet p) { OnPlayerNameUpdate(p); };
  packet_handlers_[PT_PLAYER_INSTANCE_UPDATE] = [this](Packet p) { OnPlayerInstanceUpdate(p); };
  packet_handlers_[PT_PLAYER_COLOR_UPDATE] = [this](Packet p) { OnPlayerColorUpdate(p); };
  packet_handlers_[PT_PLAYER_SKILL_WEAPON_UPDATE] = [this](Packet p) { OnPlayerSkillWeaponUpdate(p); };
  packet_handlers_[PT_PLAYER_TALENT_UPDATE] = [this](Packet p) { OnPlayerTalentUpdate(p); };
  packet_handlers_[PT_PLAYER_VISUAL_UPDATE] = [this](Packet p) { OnPlayerVisualUpdate(p); };
  packet_handlers_[PT_PLAYER_FATNESS_UPDATE] = [this](Packet p) { OnPlayerFatnessUpdate(p); };
  packet_handlers_[PT_PLAYER_SCALE_UPDATE] = [this](Packet p) { OnPlayerScaleUpdate(p); };
  packet_handlers_[PT_PLAYER_OVERLAY_UPDATE] = [this](Packet p) { OnPlayerOverlayUpdate(p); };
  packet_handlers_[PT_PLAYER_ATTRIBUTE_UPDATE] = [this](Packet p) { OnPlayerAttributeUpdate(p); };
  packet_handlers_[PT_PLAYER_ATTRIBUTE_SNAPSHOT] = [this](Packet p) { OnPlayerAttributeSnapshot(p); };
  packet_handlers_[PT_PLAYER_WORLD_UPDATE] = [this](Packet p) { OnPlayerWorldUpdate(p); };
  packet_handlers_[PT_GAME_INFO] = [this](Packet p) { OnGameInfo(p); };
  packet_handlers_[PT_LEFT_GAME] = [this](Packet p) { OnLeftGame(p); };
  packet_handlers_[Net::ID_DISCONNECTION_NOTIFICATION] = [this](Packet p) { OnDisconnectOrLostConnection(p); };
  packet_handlers_[Net::ID_CONNECTION_LOST] = [this](Packet p) { OnDisconnectOrLostConnection(p); };
}

void GameClient::ConnectAsync(std::string_view full_address) {
  {
    std::lock_guard<std::mutex> lock(connection_mutex_);

    // Prevent multiple simultaneous connection attempts
    if (connection_state_ == ConnectionState::Connecting) {
      SPDLOG_WARN("Connection already in progress");
      return;
    }

    // Join previous thread if exists
    if (connection_thread_.joinable()) {
      connection_thread_.join();
    }

    connection_state_ = ConnectionState::Connecting;
    connection_error_.clear();
  }

  resource_downloader_.Reset();

  // Queue the OnConnectionStarted callback for main thread execution
  task_scheduler_.ScheduleOnMainThread([this]() { event_observer_.OnConnectionStarted(); });

  // Launch connection thread
  connection_thread_ = std::thread([this, addr = std::string(full_address)]() {
    SPDLOG_INFO("Connection thread started for: {}", addr);
    bool success = ConnectInternal(addr);

    {
      std::lock_guard<std::mutex> lock(connection_mutex_);
      connection_state_ = success ? ConnectionState::Connected : ConnectionState::Failed;
    }

    // Queue callbacks for main thread execution instead of calling directly
    if (success) {
      SPDLOG_INFO("Connection successful");
      task_scheduler_.ScheduleOnMainThread([this]() { event_observer_.OnConnected(); });
    } else {
      SPDLOG_ERROR("Connection failed: {}", connection_error_);
      // Capture error string by value to avoid race condition
      std::string error = connection_error_;
      task_scheduler_.ScheduleOnMainThread([this, error]() { event_observer_.OnConnectionFailed(error); });
    }
  });
}

bool GameClient::ConnectInternal(std::string_view full_address) {
  // Extract port number from IP address if present
  std::string host(full_address);
  int port = 0xDEAD;
  size_t pos = host.find_last_of(':');
  if (pos != std::string::npos) {
    std::string portStr = host.substr(pos + 1);
    std::istringstream iss(portStr);
    iss >> port;
    host.erase(pos);
  }

  if (!g_netclient->Connect(host.c_str(), port)) {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    connection_error_ = "Failed to establish network connection";
    return false;
  }

  server_ip_ = host;
  server_port_ = port;
  resource_downloader_.SetServerEndpoint(host, port);
  return true;
}

void GameClient::Disconnect() {
  bool was_connected = IsConnected();

  // Join connection thread if it's still running
  {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    if (connection_thread_.joinable()) {
      connection_thread_.join();
    }
    connection_state_ = ConnectionState::Disconnected;
  }

  if (was_connected) {
    is_in_game_ = false;
    g_netclient->Disconnect();
    event_observer_.OnDisconnected();
  }
}

bool GameClient::IsConnected() const {
  std::lock_guard<std::mutex> lock(connection_mutex_);
  return !connection_lost_ && connection_state_ == ConnectionState::Connected && g_netclient->IsConnected();
}

GameClient::ConnectionState GameClient::GetConnectionState() const {
  std::lock_guard<std::mutex> lock(connection_mutex_);
  return connection_state_;
}

std::string GameClient::GetConnectionError() const {
  std::lock_guard<std::mutex> lock(connection_mutex_);
  return connection_error_;
}

int GameClient::GetPing() {
  return g_netclient->GetPing();
}

void GameClient::HandleNetwork() {
  {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    // Don't pulse if still connecting
    if (connection_state_ == ConnectionState::Connecting) {
      return;
    }
  }

  if (IsConnected()) {
    g_netclient->Pulse();
  }
}

std::vector<GameClient::ResourcePayload> GameClient::ConsumeDownloadedResources() {
  return resource_downloader_.ConsumeDownloadedResources();
}

bool GameClient::HandlePacket(unsigned char* data, std::uint32_t size) {
  try {
    SPDLOG_TRACE("Received packet: {}", (int)data[0]);
    auto it = packet_handlers_.find((int)data[0]);
    if (it != packet_handlers_.end()) {
      it->second(Packet{data, size});
    } else {
      SPDLOG_WARN("No handler for packet type: {}", (int)data[0]);
    }
  } catch (std::exception& ex) {
    SPDLOG_ERROR("Exception thrown while handling packet: {}", ex.what());
  }
  return true;
}

// ============================================================================
// Send Methods
// ============================================================================

void GameClient::UpdatePlayerState(Player* player, const PlayerState& state) {
  if (!player)
    return;

  player->set_position(state.position.x, state.position.y, state.position.z);
  player->set_rotation(glm::vec3(state.nrot.x, state.nrot.y, state.nrot.z));
  player->set_left_hand_item(state.left_hand_item_instance);
  player->set_right_hand_item(state.right_hand_item_instance);
  player->set_equipped_armor(state.equipped_armor_instance);
  player->set_animation(state.animation);
  player->set_health(state.health_points);
  player->set_mana(state.mana_points);
  player->set_weapon_mode(state.weapon_mode);
  player->set_active_spell(state.active_spell_nr);
  player->set_head_direction(state.head_direction);
  player->set_melee_weapon(state.melee_weapon_instance);
  player->set_ranged_weapon(state.ranged_weapon_instance);
}

void GameClient::JoinGame(const std::string& player_name, const std::string& character_name, 
                          const std::string& body_model, int body_texture, const std::string& head_model, int head_texture, int walk_style) {
  JoinGamePacket packet;
  packet.packet_type = PT_JOIN_GAME;
  // Position, rotation, and items will need to be filled by the caller or from the local player state
  packet.body_model = body_model;
  packet.body_texture = body_texture;
  packet.head_model = head_model;
  packet.head_texture = head_texture;
  packet.walk_style = walk_style;
  packet.player_name = player_name;

  SerializeAndSend(packet, IMMEDIATE_PRIORITY, RELIABLE_ORDERED);
}

void GameClient::SendChatMessage(const std::string& msg) {
  MessagePacket packet;
  packet.packet_type = PT_MSG;
  packet.message = msg;
  packet.r = 255;
  packet.g = 255;
  packet.b = 255;
  SerializeAndSend(packet, MEDIUM_PRIORITY, RELIABLE);
}

void GameClient::SendCastSpell(std::uint32_t target_id, std::uint16_t spell_id) {
  CastSpellPacket packet;
  packet.spell_id = spell_id;
  packet.packet_type = target_id ? PT_CASTSPELLONTARGET : PT_CASTSPELL;
  if (target_id) {
    packet.target_id = target_id;
  }
  SerializeAndSend(packet, HIGH_PRIORITY, RELIABLE);
}

void GameClient::SendDropItem(std::uint16_t instance, std::uint16_t amount) {
  DropItemPacket packet;
  packet.packet_type = PT_DROPITEM;
  packet.item_instance = instance;
  packet.item_amount = amount;
  SerializeAndSend(packet, HIGH_PRIORITY, RELIABLE);
}

void GameClient::SendTakeItem(std::uint16_t instance) {
  TakeItemPacket packet;
  packet.packet_type = PT_TAKEITEM;
  packet.item_instance = instance;
  SerializeAndSend(packet, HIGH_PRIORITY, RELIABLE);
}

void GameClient::UpdatePlayerStats(const PlayerState& state) {
  PlayerStateUpdatePacket packet;
  packet.packet_type = PT_ACTUAL_STATISTICS;
  packet.state = state;
  SerializeAndSend(packet, IMMEDIATE_PRIORITY, RELIABLE_ORDERED);
}

void GameClient::SyncGameTime() {
  std::uint8_t data[2] = {PT_GAME_INFO, 0};
  g_netclient->SendPacket(data, 1, RELIABLE, IMMEDIATE_PRIORITY);
}

// ============================================================================
// Packet Handlers
// ============================================================================

void GameClient::OnInitialInfo(Packet p) {
  InitialInfoPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize InitialInfoPacket, error code: {}", static_cast<int>(state.first));
    return;
  }

  server_name_ = packet.server_name;
  max_slots_ = packet.max_slots;

  SPDLOG_INFO("Initial info received: map='{}', server='{}', base_path='{}', resources={}", packet.map_name,
              server_name_.empty() ? "<unknown>" : server_name_,
              packet.resource_base_path.empty() ? "/public" : packet.resource_base_path, packet.client_resources.size());

  resource_downloader_.SetDownloadToken(packet.resource_token);
  resource_downloader_.SetBasePath(packet.resource_base_path.empty() ? "/public" : packet.resource_base_path);
  resource_downloader_.AnnounceResources(std::move(packet.client_resources));

  auto local_player = player_manager_.CreateLocalPlayer(packet.player_id);
  worlds_.clear();
  worlds_.emplace_back(packet.map_name);

  event_observer_.OnMapChange(packet.map_name);
  event_observer_.OnLocalPlayerJoined(*local_player);

  resource_downloader_.BeginDownload();
}

void GameClient::OnActualStatistics(Packet p) {
  PlayerStateUpdatePacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize PlayerStateUpdatePacket");
    return;
  }

  SPDLOG_TRACE("PlayerStateUpdatePacket: {}", packet);

  if (!packet.player_id) {
    SPDLOG_ERROR("PlayerStateUpdatePacket: Player id is null");
    return;
  }

  // Update the Player object with new state
  Player* player = player_manager_.GetPlayer(*packet.player_id);
  if (player) {
    UpdatePlayerState(player, packet.state);
  }

  event_observer_.OnPlayerStateUpdate(*packet.player_id, packet.state);
}

void GameClient::OnMapOnly(Packet p) {
  PlayerPositionUpdatePacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  if (!packet.player_id) {
    SPDLOG_ERROR("PlayerPositionUpdatePacket: Player id is null");
    return;
  }

  event_observer_.OnPlayerPositionUpdate(*packet.player_id, packet.position.x, packet.position.y, packet.position.z);
}

void GameClient::OnDoDie(Packet p) {
  PlayerDeathInfoPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  event_observer_.OnPlayerDied(packet.player_id);
}

void GameClient::OnRespawn(Packet p) {
  PlayerRespawnInfoPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  event_observer_.OnPlayerRespawned(packet.player_id);
}

void GameClient::OnCastSpell(Packet p) {
  CastSpellPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  if (!packet.caster_id) {
    SPDLOG_ERROR("CastSpellPacket Caster ID is null");
    return;
  }

  event_observer_.OnSpellCast(*packet.caster_id, packet.spell_id);
}

void GameClient::OnCastSpellOnTarget(Packet p) {
  CastSpellPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  if (!packet.caster_id || !packet.target_id) {
    SPDLOG_ERROR("Invalid CastSpellOnTarget packet. No caster or target id.");
    return;
  }

  event_observer_.OnSpellCastOnTarget(*packet.caster_id, *packet.target_id, packet.spell_id);
}

void GameClient::OnDropItem(Packet p) {
  DropItemPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  if (!packet.player_id) {
    SPDLOG_ERROR("Invalid DropItem packet. No player id.");
    return;
  }

  event_observer_.OnItemDropped(*packet.player_id, packet.item_instance, packet.item_amount);
}

void GameClient::OnTakeItem(Packet p) {
  TakeItemPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  if (!packet.player_id) {
    SPDLOG_ERROR("Invalid TakeItem packet. No player id.");
    return;
  }

  event_observer_.OnItemTaken(*packet.player_id, packet.item_instance);
}

void GameClient::OnGiveItem(Packet p) {
  GiveItemPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  event_observer_.OnItemGiven(packet.player_id, packet.item_instance, packet.item_amount);
}

void GameClient::OnEquipItem(Packet p) {
  EquipItemPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  event_observer_.OnItemEquipped(packet.player_id, packet.item_instance, packet.slot_id);
}

void GameClient::OnUnequipItem(Packet p) {
  UnequipItemPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  event_observer_.OnItemUnequipped(packet.player_id, packet.item_instance);
}

void GameClient::OnMessage(Packet p) {
  MessagePacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  Player* sender = packet.sender ? player_manager_.GetPlayer(*packet.sender) : nullptr;
  std::string sender_name = sender ? sender->name() : "";

  if (packet.sender) {
    SPDLOG_INFO("Message from player {} ({}): {}", sender_name, *packet.sender, packet.message);
  } else {
    SPDLOG_INFO("Server chat message: {}", packet.message);
  }

  event_observer_.OnPlayerMessage(packet.sender, packet.r, packet.g, packet.b, packet.message);
}

void GameClient::OnExistingPlayers(Packet p) {
  ExistingPlayersPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  for (const auto& existing_player : packet.existing_players) {
    SPDLOG_INFO("ExistingPlayerPacket packet: {}", existing_player);

    // Create Player object and populate it
    Player* player = player_manager_.CreatePlayer(existing_player.player_id);
    player->set_name(existing_player.player_name);
    player->set_instance(existing_player.instance);
    player->set_name_color(existing_player.name_color_r, existing_player.name_color_g, existing_player.name_color_b);
    player->set_position(existing_player.position.x, existing_player.position.y, existing_player.position.z);
    player->set_left_hand_item(existing_player.left_hand_item_instance);
    player->set_right_hand_item(existing_player.right_hand_item_instance);
    player->set_equipped_armor(existing_player.equipped_armor_instance);
    player->set_body_model(existing_player.body_model);
    player->set_body_texture(existing_player.body_texture);
    player->set_head_model(existing_player.head_model);
    player->set_head_texture(existing_player.head_texture);
    player->set_walk_style(existing_player.walk_style);

    player->set_strength(existing_player.strength);
    player->set_dexterity(existing_player.dexterity);
    player->set_level(existing_player.level);
    player->set_exp(existing_player.exp);
    player->set_next_level_exp(existing_player.next_level_exp);
    player->set_learn_points(existing_player.learn_points);
    player->set_max_health(static_cast<std::int16_t>(existing_player.max_health));
    player->set_max_mana(static_cast<std::int16_t>(existing_player.max_mana));
    player->set_health(static_cast<std::int16_t>(existing_player.health));
    player->set_mana(static_cast<std::int16_t>(existing_player.mana));

    player->set_fatness(existing_player.fatness);
    player->set_scale(existing_player.scale);

    for (const auto& entry : existing_player.weapon_skills) {
      player->set_weapon_skill(entry.skill_id, entry.percentage);
    }
    for (const auto& entry : existing_player.talents) {
      player->set_talent(entry.talent_id, entry.value);
    }
    for (const auto& overlay : existing_player.overlays) {
      player->add_overlay(overlay);
    }

    player->set_has_joined(true);
    player->set_has_spawned(true);

    event_observer_.OnPlayerJoined(*player);
    event_observer_.OnPlayerSpawned(*player);

    // Mirror the update callbacks that used to arrive as separate packets.
    event_observer_.OnPlayerInstanceUpdate(existing_player.player_id, existing_player.instance);
    event_observer_.OnPlayerColorUpdate(existing_player.player_id, existing_player.name_color_r, existing_player.name_color_g, existing_player.name_color_b);
    if (!existing_player.body_model.empty() || !existing_player.head_model.empty()) {
      event_observer_.OnPlayerVisualUpdate(existing_player.player_id, existing_player.body_model, existing_player.body_texture, existing_player.head_model,
                                           existing_player.head_texture);
    }
    event_observer_.OnPlayerFatnessUpdate(existing_player.player_id, existing_player.fatness);
    event_observer_.OnPlayerScaleUpdate(existing_player.player_id, existing_player.scale);

    event_observer_.OnPlayerAttributeUpdate(existing_player.player_id, ATTR_STRENGTH, existing_player.strength);
    event_observer_.OnPlayerAttributeUpdate(existing_player.player_id, ATTR_DEXTERITY, existing_player.dexterity);
    event_observer_.OnPlayerAttributeUpdate(existing_player.player_id, ATTR_LEVEL, existing_player.level);
    event_observer_.OnPlayerAttributeUpdate(existing_player.player_id, ATTR_EXP, existing_player.exp);
    event_observer_.OnPlayerAttributeUpdate(existing_player.player_id, ATTR_NEXT_LEVEL_EXP, existing_player.next_level_exp);
    event_observer_.OnPlayerAttributeUpdate(existing_player.player_id, ATTR_LEARN_POINTS, existing_player.learn_points);
    event_observer_.OnPlayerAttributeUpdate(existing_player.player_id, ATTR_MAX_HEALTH, existing_player.max_health);
    event_observer_.OnPlayerAttributeUpdate(existing_player.player_id, ATTR_MAX_MANA, existing_player.max_mana);
    event_observer_.OnPlayerAttributeUpdate(existing_player.player_id, ATTR_HEALTH, existing_player.health);
    event_observer_.OnPlayerAttributeUpdate(existing_player.player_id, ATTR_MANA, existing_player.mana);

    for (const auto& entry : existing_player.weapon_skills) {
      event_observer_.OnPlayerSkillWeaponUpdate(existing_player.player_id, entry.skill_id, entry.percentage);
    }
    for (const auto& entry : existing_player.talents) {
      event_observer_.OnPlayerTalentUpdate(existing_player.player_id, entry.talent_id, entry.value);
    }
    for (const auto& overlay : existing_player.overlays) {
      event_observer_.OnPlayerOverlayUpdate(existing_player.player_id, overlay, true);
    }
  }
}

void GameClient::OnPlayerSpawn(Packet p) {
  PlayerSpawnPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  SPDLOG_INFO("PlayerSpawn packet: {}", packet);

  const bool has_local_player = player_manager_.HasLocalPlayer();
  const bool is_local_spawn = has_local_player && (player_manager_.GetLocalPlayer().id() == static_cast<std::uint64_t>(packet.player_id));

  const auto emit_snapshot_callbacks = [&](std::uint32_t player_id) {
    event_observer_.OnPlayerInstanceUpdate(player_id, packet.instance);
    event_observer_.OnPlayerColorUpdate(player_id, packet.name_color_r, packet.name_color_g, packet.name_color_b);
    if (!packet.body_model.empty() || !packet.head_model.empty()) {
      event_observer_.OnPlayerVisualUpdate(player_id, packet.body_model, packet.body_texture, packet.head_model, packet.head_texture);
    }
    event_observer_.OnPlayerFatnessUpdate(player_id, packet.fatness);
    event_observer_.OnPlayerScaleUpdate(player_id, packet.scale);

    event_observer_.OnPlayerAttributeUpdate(player_id, ATTR_STRENGTH, packet.strength);
    event_observer_.OnPlayerAttributeUpdate(player_id, ATTR_DEXTERITY, packet.dexterity);
    event_observer_.OnPlayerAttributeUpdate(player_id, ATTR_LEVEL, packet.level);
    event_observer_.OnPlayerAttributeUpdate(player_id, ATTR_EXP, packet.exp);
    event_observer_.OnPlayerAttributeUpdate(player_id, ATTR_NEXT_LEVEL_EXP, packet.next_level_exp);
    event_observer_.OnPlayerAttributeUpdate(player_id, ATTR_LEARN_POINTS, packet.learn_points);
    event_observer_.OnPlayerAttributeUpdate(player_id, ATTR_MAX_HEALTH, packet.max_health);
    event_observer_.OnPlayerAttributeUpdate(player_id, ATTR_MAX_MANA, packet.max_mana);
    event_observer_.OnPlayerAttributeUpdate(player_id, ATTR_HEALTH, packet.health);
    event_observer_.OnPlayerAttributeUpdate(player_id, ATTR_MANA, packet.mana);

    for (const auto& entry : packet.weapon_skills) {
      event_observer_.OnPlayerSkillWeaponUpdate(player_id, entry.skill_id, entry.percentage);
    }
    for (const auto& entry : packet.talents) {
      event_observer_.OnPlayerTalentUpdate(player_id, entry.talent_id, entry.value);
    }
    for (const auto& overlay : packet.overlays) {
      event_observer_.OnPlayerOverlayUpdate(player_id, overlay, true);
    }
  };

  if (is_local_spawn) {
    auto& local_player = player_manager_.GetLocalPlayer();
    local_player.set_name(packet.player_name);
    local_player.set_position(packet.position.x, packet.position.y, packet.position.z);
    local_player.set_rotation(packet.normal);
    local_player.set_left_hand_item(packet.left_hand_item_instance);
    local_player.set_right_hand_item(packet.right_hand_item_instance);
    local_player.set_equipped_armor(packet.equipped_armor_instance);
    local_player.set_animation(packet.animation);
    local_player.set_body_model(packet.body_model);
    local_player.set_body_texture(packet.body_texture);
    local_player.set_head_model(packet.head_model);
    local_player.set_head_texture(packet.head_texture);
    local_player.set_walk_style(packet.walk_style);

    local_player.set_instance(packet.instance);
    local_player.set_name_color(packet.name_color_r, packet.name_color_g, packet.name_color_b);
    local_player.set_strength(packet.strength);
    local_player.set_dexterity(packet.dexterity);
    local_player.set_level(packet.level);
    local_player.set_exp(packet.exp);
    local_player.set_next_level_exp(packet.next_level_exp);
    local_player.set_learn_points(packet.learn_points);
    local_player.set_max_health(packet.max_health);
    local_player.set_max_mana(packet.max_mana);
    local_player.set_health(static_cast<std::int16_t>(packet.health));
    local_player.set_mana(static_cast<std::int16_t>(packet.mana));
    local_player.set_fatness(packet.fatness);
    local_player.set_scale(packet.scale);
    for (const auto& entry : packet.weapon_skills) {
      local_player.set_weapon_skill(entry.skill_id, entry.percentage);
    }
    for (const auto& entry : packet.talents) {
      local_player.set_talent(entry.talent_id, entry.value);
    }
    for (const auto& overlay : packet.overlays) {
      local_player.add_overlay(overlay);
    }

    local_player.set_has_spawned(true);
    is_in_game_ = true;

    event_observer_.OnLocalPlayerSpawned(local_player);
    emit_snapshot_callbacks(packet.player_id);
    return;
  }

  Player* player = player_manager_.GetPlayer(packet.player_id);
  if (!player) {
    player = player_manager_.CreatePlayer(packet.player_id);
  }

  const bool was_spawned = player->has_spawned();

  player->set_name(packet.player_name);
  player->set_position(packet.position.x, packet.position.y, packet.position.z);
  player->set_rotation(packet.normal);
  player->set_left_hand_item(packet.left_hand_item_instance);
  player->set_right_hand_item(packet.right_hand_item_instance);
  player->set_equipped_armor(packet.equipped_armor_instance);
  player->set_animation(packet.animation);
  player->set_body_model(packet.body_model);
  player->set_body_texture(packet.body_texture);
  player->set_head_model(packet.head_model);
  player->set_head_texture(packet.head_texture);
  player->set_walk_style(packet.walk_style);

  player->set_instance(packet.instance);
  player->set_name_color(packet.name_color_r, packet.name_color_g, packet.name_color_b);
  player->set_strength(packet.strength);
  player->set_dexterity(packet.dexterity);
  player->set_level(packet.level);
  player->set_exp(packet.exp);
  player->set_next_level_exp(packet.next_level_exp);
  player->set_learn_points(packet.learn_points);
  player->set_max_health(packet.max_health);
  player->set_max_mana(packet.max_mana);
  player->set_health(static_cast<std::int16_t>(packet.health));
  player->set_mana(static_cast<std::int16_t>(packet.mana));
  player->set_fatness(packet.fatness);
  player->set_scale(packet.scale);
  for (const auto& entry : packet.weapon_skills) {
    player->set_weapon_skill(entry.skill_id, entry.percentage);
  }
  for (const auto& entry : packet.talents) {
    player->set_talent(entry.talent_id, entry.value);
  }
  for (const auto& overlay : packet.overlays) {
    player->add_overlay(overlay);
  }

  player->set_has_spawned(true);

  if (!was_spawned) {
    event_observer_.OnPlayerSpawned(*player);
  }

  emit_snapshot_callbacks(packet.player_id);
}

void GameClient::OnJoinGame(Packet p) {
  JoinGamePacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  SPDLOG_INFO("JoinGame packet: {}", packet);

  if (!packet.player_id.has_value()) {
    SPDLOG_WARN("JoinGame packet missing player id, ignoring");
    return;
  }

  Player* player = player_manager_.GetPlayer(*packet.player_id);
  if (!player) {
    if (player_manager_.HasLocalPlayer() && player_manager_.GetLocalPlayer().id() == static_cast<std::uint64_t>(*packet.player_id)) {
      SPDLOG_DEBUG("Received JoinGame packet for local player");
      return;
    }
    player = player_manager_.CreatePlayer(*packet.player_id);
  }
  const bool was_joined = player->has_joined();
  player->set_name(packet.player_name);
  player->set_position(packet.position.x, packet.position.y, packet.position.z);
  player->set_rotation(packet.normal);
  player->set_left_hand_item(packet.left_hand_item_instance);
  player->set_right_hand_item(packet.right_hand_item_instance);
  player->set_equipped_armor(packet.equipped_armor_instance);
  player->set_body_model(packet.body_model);
  player->set_body_texture(packet.body_texture);
  player->set_head_model(packet.head_model);
  player->set_head_texture(packet.head_texture);
  player->set_walk_style(packet.walk_style);
  if (!was_joined) {
    player->set_has_joined(true);
    event_observer_.OnPlayerJoined(*player);
  }
}

void GameClient::OnPlayerNameUpdate(Packet p) {
  PlayerNameUpdatePacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  SPDLOG_DEBUG("PlayerNameUpdatePacket: {}", packet);

  Player* player = player_manager_.GetPlayer(packet.player_id);
  if (!player && player_manager_.HasLocalPlayer() && player_manager_.GetLocalPlayer().id() == packet.player_id) {
    player = &player_manager_.GetLocalPlayer();
  }

  if (player) {
    player->set_name(packet.name);
  }

  event_observer_.OnPlayerNameUpdate(packet.player_id, packet.name);
}

void GameClient::OnPlayerInstanceUpdate(Packet p) {
  PlayerInstanceUpdatePacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  SPDLOG_DEBUG("PlayerInstanceUpdatePacket: {}", packet);

  Player* player = player_manager_.GetPlayer(packet.player_id);
  if (!player && player_manager_.HasLocalPlayer() && player_manager_.GetLocalPlayer().id() == packet.player_id) {
    player = &player_manager_.GetLocalPlayer();
  }

  if (player) {
    player->set_instance(packet.instance);
  }

  event_observer_.OnPlayerInstanceUpdate(packet.player_id, packet.instance);
}

void GameClient::OnPlayerColorUpdate(Packet p) {
  PlayerColorUpdatePacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  SPDLOG_DEBUG("PlayerColorUpdatePacket: {}", packet);

  Player* player = player_manager_.GetPlayer(packet.player_id);
  if (!player && player_manager_.HasLocalPlayer() && player_manager_.GetLocalPlayer().id() == packet.player_id) {
    player = &player_manager_.GetLocalPlayer();
  }

  if (player) {
    player->set_name_color(packet.r, packet.g, packet.b);
  }

  event_observer_.OnPlayerColorUpdate(packet.player_id, packet.r, packet.g, packet.b);
}

void GameClient::OnPlayerSkillWeaponUpdate(Packet p) {
  PlayerSkillWeaponUpdatePacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  SPDLOG_DEBUG("PlayerSkillWeaponUpdatePacket: {}", packet);

  Player* player = player_manager_.GetPlayer(packet.player_id);
  if (!player && player_manager_.HasLocalPlayer() && player_manager_.GetLocalPlayer().id() == packet.player_id) {
    player = &player_manager_.GetLocalPlayer();
  }

  if (player) {
    player->set_weapon_skill(packet.skill_id, packet.percentage);
  }

  event_observer_.OnPlayerSkillWeaponUpdate(packet.player_id, packet.skill_id, packet.percentage);
}

void GameClient::OnPlayerTalentUpdate(Packet p) {
  PlayerTalentUpdatePacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  SPDLOG_DEBUG("PlayerTalentUpdatePacket: {}", packet);

  Player* player = player_manager_.GetPlayer(packet.player_id);
  if (!player && player_manager_.HasLocalPlayer() && player_manager_.GetLocalPlayer().id() == packet.player_id) {
    player = &player_manager_.GetLocalPlayer();
  }

  if (player) {
    player->set_talent(packet.talent_id, packet.talent_value);
  }

  event_observer_.OnPlayerTalentUpdate(packet.player_id, packet.talent_id, packet.talent_value);
}

void GameClient::OnPlayerVisualUpdate(Packet p) {
  PlayerVisualUpdatePacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  SPDLOG_DEBUG("PlayerVisualUpdatePacket: {}", packet);

  Player* player = player_manager_.GetPlayer(packet.player_id);
  if (!player && player_manager_.HasLocalPlayer() && player_manager_.GetLocalPlayer().id() == packet.player_id) {
    player = &player_manager_.GetLocalPlayer();
  }

  if (player) {
    player->set_body_model(packet.body_model);
    player->set_body_texture(packet.body_texture);
    player->set_head_model(packet.head_model);
    player->set_head_texture(packet.head_texture);
  }

  event_observer_.OnPlayerVisualUpdate(packet.player_id, packet.body_model, packet.body_texture, packet.head_model, packet.head_texture);
}

void GameClient::OnPlayerFatnessUpdate(Packet p) {
  PlayerFatnessUpdatePacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  SPDLOG_DEBUG("PlayerFatnessUpdatePacket: {}", packet);

  Player* player = player_manager_.GetPlayer(packet.player_id);
  if (!player && player_manager_.HasLocalPlayer() && player_manager_.GetLocalPlayer().id() == packet.player_id) {
    player = &player_manager_.GetLocalPlayer();
  }

  if (player) {
    player->set_fatness(packet.fatness);
  }

  event_observer_.OnPlayerFatnessUpdate(packet.player_id, packet.fatness);
}

void GameClient::OnPlayerScaleUpdate(Packet p) {
  PlayerScaleUpdatePacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  SPDLOG_DEBUG("PlayerScaleUpdatePacket: {}", packet);

  Player* player = player_manager_.GetPlayer(packet.player_id);
  if (!player && player_manager_.HasLocalPlayer() && player_manager_.GetLocalPlayer().id() == packet.player_id) {
    player = &player_manager_.GetLocalPlayer();
  }

  if (player) {
    player->set_scale(packet.scale);
  }

  event_observer_.OnPlayerScaleUpdate(packet.player_id, packet.scale);
}

void GameClient::OnPlayerOverlayUpdate(Packet p) {
  PlayerOverlayUpdatePacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  SPDLOG_DEBUG("PlayerOverlayUpdatePacket: {}", packet);

  Player* player = player_manager_.GetPlayer(packet.player_id);
  if (!player && player_manager_.HasLocalPlayer() && player_manager_.GetLocalPlayer().id() == packet.player_id) {
    player = &player_manager_.GetLocalPlayer();
  }

  if (player) {
    if (packet.apply) {
      player->add_overlay(packet.overlay);
    } else {
      player->remove_overlay(packet.overlay);
    }
  }

  event_observer_.OnPlayerOverlayUpdate(packet.player_id, packet.overlay, packet.apply != 0);
}

void GameClient::OnPlayerAttributeUpdate(Packet p) {
  PlayerAttributeUpdatePacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  SPDLOG_DEBUG("PlayerAttributeUpdatePacket: {}", packet);

  Player* player = player_manager_.GetPlayer(packet.player_id);
  if (!player && player_manager_.HasLocalPlayer() && player_manager_.GetLocalPlayer().id() == packet.player_id) {
    player = &player_manager_.GetLocalPlayer();
  }

  if (player) {
    switch (packet.attribute_id) {
      case ATTR_STRENGTH:
        player->set_strength(packet.value);
        break;
      case ATTR_DEXTERITY:
        player->set_dexterity(packet.value);
        break;
      case ATTR_LEVEL:
        player->set_level(packet.value);
        break;
      case ATTR_EXP:
        player->set_exp(packet.value);
        break;
      case ATTR_NEXT_LEVEL_EXP:
        player->set_next_level_exp(packet.value);
        break;
      case ATTR_LEARN_POINTS:
        player->set_learn_points(packet.value);
        break;
      case ATTR_HEALTH:
        player->set_health(static_cast<std::int16_t>(packet.value));
        break;
      case ATTR_MAX_HEALTH:
        player->set_max_health(static_cast<std::int16_t>(packet.value));
        break;
      case ATTR_MANA:
        player->set_mana(static_cast<std::int16_t>(packet.value));
        break;
      case ATTR_MAX_MANA:
        player->set_max_mana(static_cast<std::int16_t>(packet.value));
        break;
      default:
        break;
    }
  }

  event_observer_.OnPlayerAttributeUpdate(packet.player_id, packet.attribute_id, packet.value);
}

void GameClient::OnPlayerAttributeSnapshot(Packet p) {
  PlayerAttributeSnapshotPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  SPDLOG_DEBUG("PlayerAttributeSnapshotPacket: {}", packet);

  Player* player = player_manager_.GetPlayer(packet.player_id);
  if (!player && player_manager_.HasLocalPlayer() && player_manager_.GetLocalPlayer().id() == packet.player_id) {
    player = &player_manager_.GetLocalPlayer();
  }

  if (player) {
    player->set_strength(packet.strength);
    player->set_dexterity(packet.dexterity);
    player->set_level(packet.level);
    player->set_exp(packet.exp);
    player->set_next_level_exp(packet.next_level_exp);
    player->set_learn_points(packet.learn_points);
    player->set_max_health(static_cast<std::int16_t>(packet.max_health));
    player->set_max_mana(static_cast<std::int16_t>(packet.max_mana));
    player->set_health(static_cast<std::int16_t>(packet.health));
    player->set_mana(static_cast<std::int16_t>(packet.mana));
  }

  event_observer_.OnPlayerAttributeUpdate(packet.player_id, ATTR_STRENGTH, packet.strength);
  event_observer_.OnPlayerAttributeUpdate(packet.player_id, ATTR_DEXTERITY, packet.dexterity);
  event_observer_.OnPlayerAttributeUpdate(packet.player_id, ATTR_LEVEL, packet.level);
  event_observer_.OnPlayerAttributeUpdate(packet.player_id, ATTR_EXP, packet.exp);
  event_observer_.OnPlayerAttributeUpdate(packet.player_id, ATTR_NEXT_LEVEL_EXP, packet.next_level_exp);
  event_observer_.OnPlayerAttributeUpdate(packet.player_id, ATTR_LEARN_POINTS, packet.learn_points);
  event_observer_.OnPlayerAttributeUpdate(packet.player_id, ATTR_MAX_HEALTH, packet.max_health);
  event_observer_.OnPlayerAttributeUpdate(packet.player_id, ATTR_MAX_MANA, packet.max_mana);
  event_observer_.OnPlayerAttributeUpdate(packet.player_id, ATTR_HEALTH, packet.health);
  event_observer_.OnPlayerAttributeUpdate(packet.player_id, ATTR_MANA, packet.mana);
}

void GameClient::OnPlayerWorldUpdate(Packet p) {
  PlayerWorldUpdatePacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  SPDLOG_DEBUG("PlayerWorldUpdatePacket: {}", packet);

  event_observer_.OnPlayerWorldUpdate(packet.player_id, packet.world_name, packet.start_point);
}

void GameClient::OnGameInfo(Packet p) {
  GameInfoPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  event_observer_.OnGameInfoReceived(packet.raw_game_time, packet.flags);
}

void GameClient::OnLeftGame(Packet p) {
  DisconnectionInfoPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  // Get player name before removing
  Player* player = player_manager_.GetPlayer(packet.disconnected_id);
  std::string player_name = player ? player->name() : "";

  event_observer_.OnPlayerLeft(packet.disconnected_id, player_name);

  // Remove from player manager
  player_manager_.RemovePlayer(packet.disconnected_id);
}

void GameClient::OnDisconnectOrLostConnection(Packet p) {
  SPDLOG_WARN("OnDisconnectOrLostConnection, code: {}", p.data[0]);
  connection_lost_ = true;
  is_in_game_ = false;
  event_observer_.OnConnectionLost();
}

void LoadNetworkLibrary() {
  try {
    static dylib lib("znet");
    auto create_net_client_func = lib.get_function<Net::NetClient*()>("CreateNetClient");
    g_netclient = create_net_client_func();
  } catch (std::exception& ex) {
    SPDLOG_ERROR("LoadNetworkLibrary error: {}", ex.what());
    // If loading the network library fails, then GMP will not work.
    std::abort();
  }
  SPDLOG_DEBUG("znet dynamic library loaded: {}", (void*)g_netclient);
}

}  // namespace gmp::client
