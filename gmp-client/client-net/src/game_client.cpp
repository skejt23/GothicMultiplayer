
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
#include "default_character_appearance.h"

#include <bitsery/adapter/buffer.h>
#include <bitsery/bitsery.h>
#include <httplib.h>
#include <sodium.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <utility>

#include "net_enums.h"
#include "packets.h"
#include "shared/crypto_utils.h"

namespace gmp::client {

using namespace Net;

namespace {
bool IsValidNpcAnimation(const NpcAnimationPacket& animation, std::uint32_t max_slots) {
  return animation.npc_id > max_slots && animation.npc_id <= static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) &&
         animation.actor_generation != 0 && animation.revision != 0 && animation.animation.size() <= kMaxPlayerAnimationNameLength &&
         animation.animation.find('\0') == std::string::npos;
}

bool IsValidNpcControl(const NpcControlPacket& control, std::uint32_t max_slots) {
  const auto finite = [](const glm::vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
  };
  if (control.npc_id <= max_slots || control.npc_id > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) ||
      control.actor_generation == 0 || control.control_epoch == 0 || control.host_player_id > max_slots || !finite(control.position) || !finite(control.normal) ||
      control.animation.find('\0') != std::string::npos) {
    return false;
  }
  if (control.action_id == 0) {
    return control.animation.empty() && control.timeout_ms == 0;
  }
  return control.host_player_id != 0 && !control.animation.empty() && control.animation.size() <= kMaxPlayerAnimationNameLength &&
         control.timeout_ms > 0 && control.timeout_ms <= 30000;
}
}  // namespace

bool ParseServerEndpoint(std::string_view full_address, std::string& host, std::uint32_t& port) {
  const auto first_character = full_address.find_first_not_of(" \t\r\n");
  if (first_character == std::string_view::npos) {
    return false;
  }
  const auto last_character = full_address.find_last_not_of(" \t\r\n");
  full_address = full_address.substr(first_character, last_character - first_character + 1);

  host.assign(full_address.begin(), full_address.end());
  port = kDefaultServerPort;

  const size_t pos = host.find_last_of(':');
  if (pos != std::string::npos) {
    const std::string_view port_text(host.data() + pos + 1, host.size() - pos - 1);
    std::uint32_t parsed_port = 0;
    const auto [end, error] = std::from_chars(port_text.data(), port_text.data() + port_text.size(), parsed_port);
    if (error != std::errc{} || end != port_text.data() + port_text.size() || parsed_port == 0 ||
        parsed_port > 65535) {
      return false;
    }
    port = parsed_port;
    host.erase(pos);
  }

  if (host.empty() || host.find(':') != std::string::npos) {
    return false;
  }
  return std::none_of(host.begin(), host.end(), [](unsigned char character) { return std::isspace(character); });
}

static std::string GetConnectionCloseMessage(const Net::ConnectionEvent& event) {
  switch (event.reason) {
    case Net::CloseReason::Normal:
      return "Server closed the connection";
    case Net::CloseReason::ServerFull:
      return "Server is full";
    case Net::CloseReason::Banned:
      return "Connection banned";
    case Net::CloseReason::Shutdown:
      return "Server shut down";
    case Net::CloseReason::Kicked:
      return "Kicked from the server";
    default:
      return event.detail.empty() ? "Network connection failed" : event.detail;
  }
}

template <typename TContainer = std::vector<std::uint8_t>, typename Packet>
static bool SerializeAndSend(Net::ClientTransport& transport, const Packet& packet, Net::TrafficClass traffic) {
  TContainer buffer;
  auto written_size = bitsery::quickSerialization<bitsery::OutputBufferAdapter<TContainer>>(buffer, packet);
  if (!transport.Send(buffer.data(), static_cast<std::uint32_t>(written_size), traffic)) {
    SPDLOG_WARN("Failed to send gameplay packet {} ({} bytes, traffic class {})", buffer.front(), written_size,
                static_cast<int>(traffic));
    // Losing a reliable action would leave client and server state inconsistent.
    // The transport reports this failure from the next main-thread Pulse.
    if (traffic == Net::TrafficClass::Control) {
      transport.Disconnect(Net::CloseReason::Failure);
    }
    return false;
  }
  return true;
}

GameClient::GameClient(EventObserver& eventObserver, gmp::TaskScheduler& taskScheduler)
    : event_observer_(eventObserver), resource_downloader_(eventObserver, taskScheduler) {
  InitPacketHandlers();
  transport_.SetCallbacks([this](unsigned char* data, std::uint32_t size) { HandlePacket(data, size); },
                          [this](const Net::ConnectionEvent& event) { OnTransportEvent(event); });
}

GameClient::~GameClient() {
  Disconnect();
  resource_downloader_.StopDownload();
  transport_.SetCallbacks({}, {});
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
  packet_handlers_[PT_ITEM_GROUND_CREATE] = [this](Packet p) { OnItemGroundCreate(p); };
  packet_handlers_[PT_ITEM_GROUND_DESTROY] = [this](Packet p) { OnItemGroundDestroy(p); };
  packet_handlers_[PT_ITEM_GROUND_CLEAR] = [this](Packet p) { OnItemsGroundDestroy(p); };
  packet_handlers_[PT_GIVEITEM] = [this](Packet p) { OnGiveItem(p); };
  packet_handlers_[PT_EQUIPITEM] = [this](Packet p) { OnEquipItem(p); };
  packet_handlers_[PT_UNEQUIPITEM] = [this](Packet p) { OnUnequipItem(p); };
  packet_handlers_[PT_REMOVEITEM] = [this](Packet p) { OnRemoveItem(p); };
  packet_handlers_[PT_ACTOR_ACTION] = [this](Packet p) { OnActorAction(p); };
  packet_handlers_[PT_PLAYER_SUMMON] = [this](Packet p) { OnPlayerSummon(p); };
  packet_handlers_[PT_PLAYER_TELEPORT] = [this](Packet p) { OnPlayerTeleport(p); };
  packet_handlers_[PT_PLAYER_TRANSFORMATION] = [this](Packet p) { OnPlayerTransformation(p); };
  packet_handlers_[PT_ACTOR_DAMAGE] = [this](Packet p) { OnActorDamage(p); };
  packet_handlers_[PT_ITEM_TRANSACTION_RESULT] = [this](Packet p) { OnItemTransactionResult(p); };
  packet_handlers_[PT_MSG] = [this](Packet p) { OnMessage(p); };
  packet_handlers_[PT_PLAYER_SPAWN] = [this](Packet p) { OnCharacterSpawn(p); };
  packet_handlers_[PT_NPC_SPAWN] = [this](Packet p) { OnNpcSpawn(p); };
  packet_handlers_[PT_NPC_CONTROL] = [this](Packet p) { OnNpcControl(p); };
  packet_handlers_[PT_NPC_ANIMATION] = [this](Packet p) { OnNpcAnimation(p); };
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
  packet_handlers_[PT_PLAYER_ANI_PLAY] = [this](Packet p) { OnPlayerAnimationPlay(p); };
  packet_handlers_[PT_PLAYER_ANI_STOP] = [this](Packet p) { OnPlayerAnimationStop(p); };
  packet_handlers_[PT_PLAYER_FACE_ANI_PLAY] = [this](Packet p) { OnPlayerFaceAnimationPlay(p); };
  packet_handlers_[PT_PLAYER_FACE_ANI_STOP] = [this](Packet p) { OnPlayerFaceAnimationStop(p); };
  packet_handlers_[PT_PLAYER_GESTICULATION] = [this](Packet p) { OnPlayerGesticulation(p); };
  packet_handlers_[PT_PLAYER_ATTRIBUTE_UPDATE] = [this](Packet p) { OnPlayerAttributeUpdate(p); };
  packet_handlers_[PT_PLAYER_ATTRIBUTE_SNAPSHOT] = [this](Packet p) { OnPlayerAttributeSnapshot(p); };
  packet_handlers_[PT_PLAYER_WORLD_UPDATE] = [this](Packet p) { OnPlayerWorldUpdate(p); };
  packet_handlers_[PT_PLAYER_UNCONSCIOUS] = [this](Packet p) { OnPlayerUnconscious(p); };
  packet_handlers_[PT_PLAYER_STANDUP] = [this](Packet p) { OnPlayerStandUp(p); };
  packet_handlers_[PT_GAME_INFO] = [this](Packet p) { OnGameInfo(p); };
  packet_handlers_[PT_SKY_SETTINGS] = [this](Packet p) { OnSkySettings(p); };
  packet_handlers_[PT_PLAYER_PRESENCE] = [this](Packet p) { OnPlayerPresence(p); };
  packet_handlers_[PT_ADMIN_AUTH] = [this](Packet p) { OnAdminAuth(p); };
  packet_handlers_[PT_VOICE_CONFIG] = [this](Packet p) { OnVoiceConfiguration(p); };
  packet_handlers_[PT_VOICE] = [this](Packet p) { OnVoice(p); };
  packet_handlers_[PT_LEFT_GAME] = [this](Packet p) { OnLeftGame(p); };
  packet_handlers_[PT_LUA_EVENT] = [this](Packet p) { OnLuaEvent(p); };
}

void GameClient::ConnectAsync(std::string_view full_address) {
  if (GetConnectionState() == ConnectionState::Connecting || GetConnectionState() == ConnectionState::Connected ||
      transport_.IsConnected()) {
    Disconnect();
  }

  std::string host;
  std::uint32_t port = 0;
  if (!ParseServerEndpoint(full_address, host, port)) {
    const std::string error = "Invalid server endpoint";
    {
      std::lock_guard<std::mutex> lock(connection_mutex_);
      connection_state_ = ConnectionState::Failed;
      connection_error_ = error;
      connection_close_reason_ = Net::CloseReason::Failure;
      notify_connected_pending_ = false;
      notify_connection_failed_pending_ = false;
      notify_connection_lost_pending_ = false;
      notify_connection_closed_pending_ = false;
    }
    event_observer_.OnConnectionFailed(error);
    return;
  }

  {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    connection_state_ = ConnectionState::Connecting;
    connection_lost_ = false;
    is_in_game_ = false;
    outgoing_state_sequence_ = 0;
    connection_error_.clear();
    connection_close_reason_ = Net::CloseReason::None;
    server_ip_ = host;
    server_port_ = port;
    notify_connected_pending_ = false;
    notify_connection_failed_pending_ = false;
    notify_connection_lost_pending_ = false;
    notify_connection_closed_pending_ = false;
  }

  resource_downloader_.Reset();
  if (player_manager_.HasLocalPlayer() || !player_manager_.GetAllPlayers().empty()) {
    event_observer_.OnPlayersClearing();
  }
  player_manager_.Clear();
  online_players_.clear();
  pending_hit_batches_.clear();
  pending_local_correction_mask_ = 0;
  pending_equipment_verifications_.clear();
  pending_inventory_commands_.clear();
  ResetMobStateReplay();
  worlds_.clear();
  resource_downloader_.SetServerEndpoint(host, port);

  event_observer_.OnConnectionStarted();

  if (transport_.Connect(host.c_str(), port)) {
    return;
  }

  const std::string error = "Failed to start network connection";
  {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    connection_state_ = ConnectionState::Failed;
    connection_error_ = error;
    connection_close_reason_ = Net::CloseReason::Failure;
  }

  event_observer_.OnConnectionFailed(error);
}

void GameClient::Disconnect() {
  bool notify_disconnected = false;

  {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    notify_disconnected = connection_state_ == ConnectionState::Connected && !connection_lost_;
    if (connection_state_ == ConnectionState::Connecting || connection_state_ == ConnectionState::Connected) {
      connection_close_reason_ = Net::CloseReason::Normal;
      connection_error_.clear();
    }
    connection_state_ = ConnectionState::Disconnected;
    connection_lost_ = false;
    is_in_game_ = false;
    outgoing_state_sequence_ = 0;
    admin_authenticated_ = false;
    notify_connected_pending_ = false;
    notify_connection_failed_pending_ = false;
    notify_connection_lost_pending_ = false;
    notify_connection_closed_pending_ = false;
  }

  resource_downloader_.StopDownload();

  transport_.Disconnect();

  if (player_manager_.HasLocalPlayer() || !player_manager_.GetAllPlayers().empty()) {
    event_observer_.OnPlayersClearing();
  }
  player_manager_.Clear();
  online_players_.clear();
  pending_hit_batches_.clear();
  pending_local_correction_mask_ = 0;
  pending_equipment_verifications_.clear();
  pending_inventory_commands_.clear();
  ResetMobStateReplay();
  worlds_.clear();
  event_observer_.OnAdminAuthChanged(false);

  if (notify_disconnected) {
    event_observer_.OnDisconnected();
  }
}

bool GameClient::IsConnected() const {
  std::lock_guard<std::mutex> lock(connection_mutex_);
  return !connection_lost_ && connection_state_ == ConnectionState::Connected && transport_.IsConnected();
}

GameClient::ConnectionState GameClient::GetConnectionState() const {
  std::lock_guard<std::mutex> lock(connection_mutex_);
  return connection_state_;
}

std::string GameClient::GetConnectionError() const {
  std::lock_guard<std::mutex> lock(connection_mutex_);
  return connection_error_;
}

Net::CloseReason GameClient::GetConnectionCloseReason() const {
  std::lock_guard<std::mutex> lock(connection_mutex_);
  return connection_close_reason_;
}

int GameClient::GetPing() {
  return transport_.GetPing();
}

std::int32_t GameClient::GetPlayerPing(std::uint64_t player_id) {
  if (const auto* online = GetOnlinePlayer(player_id))
    return online->ping;
  if (player_manager_.HasLocalPlayer() && player_manager_.GetLocalPlayer().id() == player_id) {
    const auto ping = player_manager_.GetLocalPlayer().ping();
    return ping >= 0 ? ping : GetPing();
  }

  Player* player = player_manager_.GetPlayer(player_id);
  return player ? player->ping() : -1;
}

Net::NetworkStats GameClient::GetNetworkStats() const {
  if (!IsConnected()) {
    return {};
  }

  return transport_.GetNetworkStats();
}

void GameClient::HandleNetwork() {
  bool should_pulse = false;
  {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    should_pulse = connection_state_ == ConnectionState::Connecting || connection_state_ == ConnectionState::Connected;
  }

  if (should_pulse) {
    transport_.Pulse();
  }
  DispatchPendingConnectionNotifications();
}

std::vector<GameClient::ResourcePayload> GameClient::ConsumeDownloadedResources() {
  return resource_downloader_.ConsumeDownloadedResources();
}

std::vector<GameClient::AddonPayload> GameClient::ConsumeDownloadedAddons() {
  return resource_downloader_.ConsumeDownloadedAddons();
}

bool GameClient::HandlePacket(unsigned char* data, std::uint32_t size) {
  if (!data || size == 0) {
    return false;
  }
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

void GameClient::UpdatePlayerState(Player* player, const PlayerState& state, std::optional<std::uint32_t> command_revision) {
  if (!player)
    return;

  const auto mask = state.correction_mask;
  if (mask & kSyncPosition) {
    player->set_position(state.position);
    player->set_source_time_ms(state.source_time_ms);
  }
  if (mask & kSyncOrientation) player->set_rotation(state.nrot);
  if (mask & kSyncEquipment) {
    player->set_left_hand_item(state.left_hand_item_instance);
    player->set_right_hand_item(state.right_hand_item_instance);
    player->set_equipped_armor(state.equipped_armor_instance);
    player->set_equipped_helmet(state.equipped_helmet_instance);
    player->set_equipped_shield(state.equipped_shield_instance);
    player->set_equipped_amulet(state.equipped_amulet_instance);
    player->set_equipped_belt(state.equipped_belt_instance);
    player->set_equipped_ring_left(state.equipped_ring_left_instance);
    player->set_equipped_ring_right(state.equipped_ring_right_instance);
    player->set_melee_weapon(state.melee_weapon_instance);
    player->set_ranged_weapon(state.ranged_weapon_instance);
  }
  if (mask & kSyncPose) {
    player->set_animation_name(state.animation_name);
    player->set_head_direction(state.head_direction);
    player->set_body_state(state.body_state);
    player->set_walk_mode(state.walk_mode);
    player->set_look_angles(state.look_yaw, state.look_pitch);
    player->set_focus_target_id(state.focus_target_id);
  }
  if (mask & kSyncHealth) player->set_health(state.health_points);
  if (mask & kSyncMana) player->set_mana(state.mana_points);
  if (mask & kSyncLife) {
    player->set_life_state(state.life_state, command_revision);
    player->set_invulnerable(state.invulnerable);
  }
  if (mask & kSyncCombat) {
    player->set_weapon_mode(state.weapon_mode);
    player->set_active_spell(state.active_spell_nr);
    player->set_active_spell_instance(state.active_spell_instance);
    player->set_spell_investment(state.active_spell_level, state.spell_investing);
  }
}

Player* GameClient::FindActor(std::uint64_t id) {
  if (player_manager_.HasLocalPlayer() && player_manager_.GetLocalPlayer().id() == id)
    return &player_manager_.GetLocalPlayer();
  return player_manager_.GetPlayer(id);
}

Player* GameClient::FindActor(std::uint64_t id, std::uint64_t generation) {
  auto* actor = FindActor(id);
  return actor && actor->matches_generation(generation) ? actor : nullptr;
}

void GameClient::ApplyEquipmentSlot(Player& player, std::int16_t slot, const std::string& instance) {
  switch (slot) {
    case EQUIP_SLOT_ARMOR: player.set_equipped_armor(instance); break;
    case EQUIP_SLOT_MELEE_WEAPON: player.set_melee_weapon(instance); break;
    case EQUIP_SLOT_RANGED_WEAPON: player.set_ranged_weapon(instance); break;
    case EQUIP_SLOT_HELMET: player.set_equipped_helmet(instance); break;
    case EQUIP_SLOT_SHIELD: player.set_equipped_shield(instance); break;
    case EQUIP_SLOT_AMULET: player.set_equipped_amulet(instance); break;
    case EQUIP_SLOT_BELT: player.set_equipped_belt(instance); break;
    case EQUIP_SLOT_RING_LEFT: player.set_equipped_ring_left(instance); break;
    case EQUIP_SLOT_RING_RIGHT: player.set_equipped_ring_right(instance); break;
    default: break;
  }
}

void GameClient::JoinGame(const std::string& player_name, const std::string& character_name, 
                          const std::string& body_model, int body_texture, const std::string& head_model, int head_texture, int walk_style,
                          int teeth_texture, int skin_color, const std::string& walking_overlay) {
  JoinGamePacket packet;
  packet.packet_type = PT_JOIN_GAME;
  if (player_manager_.HasLocalPlayer()) {
    const auto& local = player_manager_.GetLocalPlayer();
    packet.position = local.position();
    packet.normal = local.rotation();
    if (!IsFiniteActorVector(packet.normal) || glm::dot(packet.normal, packet.normal) < 0.0001f)
      packet.normal = {0.0f, 0.0f, 1.0f};
    packet.left_hand_item_instance = local.left_hand_item();
    packet.right_hand_item_instance = local.right_hand_item();
    packet.equipped_armor_instance = local.equipped_armor();
    packet.equipped_helmet_instance = local.equipped_helmet();
    packet.equipped_shield_instance = local.equipped_shield();
    packet.equipped_amulet_instance = local.equipped_amulet();
    packet.equipped_belt_instance = local.equipped_belt();
    packet.equipped_ring_left_instance = local.equipped_ring_left();
    packet.equipped_ring_right_instance = local.equipped_ring_right();
    packet.animation_name = local.animation_name();
  }
  packet.body_model = body_model.empty() && player_manager_.HasLocalPlayer() ? player_manager_.GetLocalPlayer().body_model() : body_model;
  packet.body_texture = body_model.empty() && player_manager_.HasLocalPlayer() ? player_manager_.GetLocalPlayer().body_texture() : body_texture;
  packet.head_model = head_model.empty() && player_manager_.HasLocalPlayer() ? player_manager_.GetLocalPlayer().head_model() : head_model;
  packet.head_texture = head_model.empty() && player_manager_.HasLocalPlayer() ? player_manager_.GetLocalPlayer().head_texture() : head_texture;
  packet.teeth_texture = head_model.empty() && player_manager_.HasLocalPlayer() ? player_manager_.GetLocalPlayer().teeth_texture() : teeth_texture;
  packet.skin_color = body_model.empty() && player_manager_.HasLocalPlayer() ? player_manager_.GetLocalPlayer().skin_color() : skin_color;
  packet.walk_style = body_model.empty() && player_manager_.HasLocalPlayer() ? player_manager_.GetLocalPlayer().walk_style() : walk_style;
  packet.player_name = player_name;
  packet.walking_overlay = walking_overlay;
  if (player_manager_.HasLocalPlayer()) {
    auto& local = player_manager_.GetLocalPlayer();
    // The native hero already wears this overlay. Keep it in the cache so a
    // server's first spawn can remove it even when the server chooses no overlay.
    const auto previous_overlays = local.overlays();
    for (const auto& overlay : previous_overlays) {
      if (gmp::NormalizeCharacterWalkingOverlay(overlay)) local.remove_overlay(overlay);
    }
    if (!walking_overlay.empty()) local.add_overlay(walking_overlay);
  }
  if (player_manager_.HasLocalPlayer()) packet.actor_generation = player_manager_.GetLocalPlayer().actor_generation();

  SerializeAndSend(transport_, packet, TrafficClass::Control);
}

void GameClient::SendChatMessage(const std::string& msg) {
  MessagePacket packet;
  packet.packet_type = PT_MSG;
  packet.message = msg;
  packet.r = 255;
  packet.g = 255;
  packet.b = 255;
  SerializeAndSend(transport_, packet, TrafficClass::Control);
}

std::uint32_t GameClient::SendCastSpell(std::uint32_t target_id, std::uint16_t spell_id, const std::string& item_instance, std::uint32_t caster_id,
                                        std::int32_t mana_after, std::uint8_t spell_level, bool transformation_succeeded, bool scroll) {
  if (!player_manager_.HasLocalPlayer())
    return 0;
  auto& local = player_manager_.GetLocalPlayer();
  auto* caster = caster_id ? FindActor(caster_id) : &local;
  if (!caster || (caster != &local && (!caster->is_npc() || caster->npc_host_player_id() != local.id())))
    return 0;
  CastSpellPacket packet;
  packet.transformation_succeeded = transformation_succeeded;
  packet.scroll = scroll;
  const auto observed_mana = mana_after >= 0 ? mana_after : caster->mana();
  packet.mana_delta = static_cast<std::int32_t>(std::clamp<std::int64_t>(
      static_cast<std::int64_t>(observed_mana) - caster->mana(), std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()));
  packet.source_time_ms = ActorClockMilliseconds();
  packet.caster_id = static_cast<std::uint32_t>(caster->id());
  packet.command_revision = caster->command_revision();
  packet.control_epoch = caster->is_npc() ? caster->npc_control_epoch() : 0;
  packet.actor_generation = caster->actor_generation();
  packet.item_instance = !item_instance.empty() ? item_instance : caster->active_spell_instance();
  packet.action_id = ++outgoing_action_sequence_;
  if (packet.action_id == 0)
    packet.action_id = ++outgoing_action_sequence_;
  if (auto* target = FindActor(target_id))
    packet.target_generation = target->actor_generation();
  packet.spell_id = spell_id;
  packet.spell_level = std::min(spell_level, kMaxSpellInvestLevel);
  packet.packet_type = target_id ? PT_CASTSPELLONTARGET : PT_CASTSPELL;
  if (target_id) {
    packet.target_id = target_id;
  }
  if (!SerializeAndSend(transport_, packet, TrafficClass::Control))
    return 0;
  caster->set_mana(observed_mana);
  return packet.action_id;
}

void GameClient::SendTransformationReturn(std::uint32_t cast_id) {
  if (!is_in_game_ || !player_manager_.HasLocalPlayer() || !cast_id)
    return;
  const auto& local = player_manager_.GetLocalPlayer();
  PlayerTransformationPacket packet;
  packet.player_id = static_cast<std::uint32_t>(local.id());
  packet.actor_generation = local.actor_generation();
  packet.command_revision = local.command_revision();
  packet.cast_id = cast_id;
  SerializeAndSend(transport_, packet, TrafficClass::Control);
}

std::uint32_t GameClient::SendDropItem(const std::string& instance, std::int32_t amount, const glm::vec3& position,
                              const glm::vec3& rotation, bool physics_enabled) {
  if (!is_in_game_ || amount <= 0) return 0;
  DropItemPacket packet;
  packet.actor_generation = player_manager_.GetLocalPlayer().actor_generation();
  packet.request_id = ++outgoing_item_request_;
  if (packet.request_id == 0) packet.request_id = ++outgoing_item_request_;
  packet.packet_type = PT_DROPITEM;
  packet.item_instance = instance;
  packet.item_amount = amount;
  packet.position = position;
  packet.rotation = rotation;
  packet.physics_enabled = physics_enabled;
  return SerializeAndSend(transport_, packet, TrafficClass::Control) ? packet.request_id : 0;
}

std::uint32_t GameClient::SendTakeItem(const std::string& instance, std::int32_t amount,
                              std::optional<std::uint32_t> item_ground_id) {
  if (!is_in_game_ || amount <= 0) return 0;
  TakeItemPacket packet;
  packet.actor_generation = player_manager_.GetLocalPlayer().actor_generation();
  packet.request_id = ++outgoing_item_request_;
  if (packet.request_id == 0) packet.request_id = ++outgoing_item_request_;
  packet.packet_type = PT_TAKEITEM;
  packet.item_instance = instance;
  packet.item_amount = amount;
  packet.item_ground_id = item_ground_id;
  return SerializeAndSend(transport_, packet, TrafficClass::Control) ? packet.request_id : 0;
}

void GameClient::SendPlayerWorldEnter(const std::string& world_name, const std::vector<InventoryEntry>& inventory) {
  if (!player_manager_.HasLocalPlayer()) return;
  auto& local = player_manager_.GetLocalPlayer();
  for (const auto& command : pending_inventory_commands_) {
    if (command.give) event_observer_.OnItemGiven(local.id(), command.instance, command.amount);
    else event_observer_.OnItemRemoved(local.id(), command.instance, command.amount);
  }
  pending_inventory_commands_.clear();
  if (pending_local_correction_mask_) {
    auto correction = local.state();
    correction.correction_mask = pending_local_correction_mask_;
    pending_local_correction_mask_ = 0;
    event_observer_.OnPlayerStateUpdate(local.id(), correction);
  }
  // This call comes from the native world-ready boundary. Spawn metadata can
  // arrive earlier, while the old world's Vobs are still being unloaded.
  const auto ready_generation = local.actor_generation();
  world_objects_ready_ = true;
  DrainMobStateReplay();
  if (!world_objects_ready_ || !player_manager_.HasLocalPlayer() ||
      !player_manager_.GetLocalPlayer().matches_generation(ready_generation)) return;
  PlayerWorldEnterPacket packet;
  packet.actor_generation = player_manager_.GetLocalPlayer().actor_generation();
  packet.inventory = inventory;
  packet.packet_type = PT_PLAYER_WORLD_ENTER;
  packet.world_name = world_name;
  if (SerializeAndSend(transport_, packet, TrafficClass::Control)) {
    player_manager_.GetLocalPlayer().set_has_spawned(true);
    is_in_game_ = true;
    for (const auto& command : pending_equipment_verifications_) {
      if (command.equip) event_observer_.OnItemEquipped(local.id(), command.instance, command.slot, command.revision, false);
      else event_observer_.OnItemUnequipped(local.id(), command.instance, command.slot, command.revision, false);
    }
    pending_equipment_verifications_.clear();
  }
}

void GameClient::SendPlayerHit(std::uint32_t victim_id, std::int32_t damage, std::uint32_t damage_type, bool dont_kill,
                              std::optional<std::uint32_t> attacker_id, std::uint32_t control_epoch, bool finishing_move) {
  if (!player_manager_.HasLocalPlayer()) return;
  PlayerHitReportPacket packet;
  packet.actor_generation = player_manager_.GetLocalPlayer().actor_generation();
  packet.command_revision = player_manager_.GetLocalPlayer().command_revision();
  packet.attacker_id = attacker_id;
  packet.control_epoch = control_epoch;
  if (attacker_id) {
    auto* attacker = FindActor(*attacker_id);
    if (!attacker) return;
    packet.actor_generation = attacker->actor_generation();
    packet.command_revision = attacker->command_revision();
  }
  if (auto* victim = FindActor(victim_id)) packet.victim_generation = victim->actor_generation();
  packet.packet_type = PT_PLAYER_HIT;
  packet.victim_id = victim_id;
  packet.damage = damage;
  packet.damage_type = damage_type;
  packet.dont_kill = dont_kill;
  packet.finishing_move = finishing_move;
  if (!pending_hit_batches_.empty()) {
    if (pending_hit_batches_.back().size() < 64) pending_hit_batches_.back().push_back(packet);
  } else {
    SendCapturedHit(packet);
  }
}

void GameClient::BeginHitBatch() {
  pending_hit_batches_.emplace_back();
}

void GameClient::EndHitBatch(bool accepted) {
  if (pending_hit_batches_.empty()) return;
  auto hits = std::move(pending_hit_batches_.back());
  pending_hit_batches_.pop_back();
  if (!accepted) return;
  for (auto& hit : hits) {
    if (!pending_hit_batches_.empty()) {
      if (pending_hit_batches_.back().size() < 64) pending_hit_batches_.back().push_back(hit);
    } else {
      SendCapturedHit(hit);
    }
  }
}

void GameClient::SendCapturedHit(PlayerHitReportPacket packet) {
  if (!player_manager_.HasLocalPlayer()) return;
  const auto& local = player_manager_.GetLocalPlayer();
  const auto attacker_id = packet.attacker_id.value_or(static_cast<std::uint32_t>(local.id()));
  auto* attacker = FindActor(attacker_id, packet.actor_generation);
  if (!attacker || !attacker->has_spawned() || !FindActor(packet.victim_id, packet.victim_generation)) return;
  if (attacker != &local && (!attacker->is_npc() || attacker->npc_host_player_id() != local.id() ||
                             attacker->npc_control_epoch() != packet.control_epoch)) return;
  packet.attack_id = ++outgoing_action_sequence_;
  if (packet.attack_id == 0) packet.attack_id = ++outgoing_action_sequence_;
  SerializeAndSend(transport_, packet, TrafficClass::Control);
}

void GameClient::SendPlayerUnconscious(std::optional<std::uint32_t> attacker_id) {
  if (!player_manager_.HasLocalPlayer()) return;
  PlayerUnconsciousPacket packet;
  packet.actor_generation = player_manager_.GetLocalPlayer().actor_generation();
  packet.command_revision = player_manager_.GetLocalPlayer().command_revision();
  packet.packet_type = PT_PLAYER_UNCONSCIOUS;
  packet.player_id = 0;
  packet.attacker_id = attacker_id;
  SerializeAndSend(transport_, packet, TrafficClass::Control);
}

void GameClient::SendPlayerStandUp() {
  if (!player_manager_.HasLocalPlayer()) return;
  PlayerStandUpPacket packet;
  packet.actor_generation = player_manager_.GetLocalPlayer().actor_generation();
  packet.command_revision = player_manager_.GetLocalPlayer().command_revision();
  packet.packet_type = PT_PLAYER_STANDUP;
  packet.player_id = 0;
  SerializeAndSend(transport_, packet, TrafficClass::Control);
}

void GameClient::SendPlayerDeath(std::optional<std::uint32_t> killer_id) {
  if (!player_manager_.HasLocalPlayer()) return;
  PlayerDeathReportPacket packet;
  packet.actor_generation = player_manager_.GetLocalPlayer().actor_generation();
  packet.packet_type = PT_PLAYER_DIED;
  packet.killer_id = killer_id;
  SerializeAndSend(transport_, packet, TrafficClass::Control);
}

bool GameClient::SendVoiceFrame(std::uint32_t talkspurt_id, std::uint32_t sequence,
                                const std::vector<std::uint8_t>& encoded_data) {
  if (!is_in_game_ || encoded_data.empty() || encoded_data.size() > kMaxVoiceFrameSize || talkspurt_id == 0) {
    return false;
  }

  VoicePacket packet{};
  packet.packet_type = PT_VOICE;
  packet.talkspurt_id = talkspurt_id;
  packet.sequence = sequence;
  packet.voice_data = encoded_data;
  return SerializeAndSend(transport_, packet, TrafficClass::Voice);
}

bool GameClient::SendVoiceChannel(const std::string& channel) {
  if (!is_in_game_ || channel.empty() || channel.size() > kMaxVoiceChannelNameLength) {
    return false;
  }

  VoiceChannelPacket packet{};
  packet.packet_type = PT_VOICE_CHANNEL;
  packet.channel = channel;
  return SerializeAndSend(transport_, packet, TrafficClass::Control);
}

void GameClient::UpdatePlayerStats(const PlayerState& state) {
  if (!is_in_game_) {
    return;
  }

  PlayerStateUpdatePacket packet;
  packet.packet_type = PT_ACTUAL_STATISTICS;
  ++outgoing_state_sequence_;
  if (outgoing_state_sequence_ == 0) {
    ++outgoing_state_sequence_;
  }
  packet.state_sequence = outgoing_state_sequence_;
  packet.state = state;
  if (!packet.state.source_time_ms) packet.state.source_time_ms = ActorClockMilliseconds();
  auto& local = player_manager_.GetLocalPlayer();
  packet.actor_generation = local.actor_generation();
  packet.command_revision = local.command_revision();
  auto predicted = packet.state;
  // Recovery requests retry against the confirmed life state until the server
  // acknowledges them. A native pose cannot acknowledge its own recovery.
  predicted.correction_mask &= ~kSyncLife;
  UpdatePlayerState(&local, predicted);
  SerializeAndSend(transport_, packet, TrafficClass::State);
}

void GameClient::SendNativeTeleport(PlayerTeleportPacket packet) {
  if (!is_in_game_ || !player_manager_.HasLocalPlayer())
    return;
  const auto& local = player_manager_.GetLocalPlayer();
  auto* actor = FindActor(packet.player_id);
  if (!actor || !actor->has_spawned() || (actor != &local && (!actor->is_npc() || actor->npc_host_player_id() != local.id())))
    return;
  packet.actor_generation = actor->actor_generation();
  packet.command_revision = actor->command_revision();
  packet.control_epoch = actor->is_npc() ? actor->npc_control_epoch() : 0;
  packet.action_id = ++outgoing_action_sequence_;
  if (!packet.action_id)
    packet.action_id = ++outgoing_action_sequence_;
  SerializeAndSend(transport_, packet, TrafficClass::Control);
}

void GameClient::SendActorAction(ActorActionPacket packet) {
  if (!is_in_game_ || !player_manager_.HasLocalPlayer()) return;
  const auto& local = player_manager_.GetLocalPlayer();
  if (!packet.actor_id) packet.actor_id = static_cast<std::uint32_t>(local.id());
  auto* actor = FindActor(packet.actor_id);
  if (!actor || (actor != &local && (!actor->is_npc() || actor->npc_host_player_id() != local.id()))) return;
  packet.actor_generation = actor->actor_generation();
  packet.command_revision = actor->command_revision();
  packet.control_epoch = actor->is_npc() ? actor->npc_control_epoch() : 0;
  packet.action_id = ++outgoing_action_sequence_;
  if (packet.action_id == 0) packet.action_id = ++outgoing_action_sequence_;
  if (!packet.source_time_ms) packet.source_time_ms = ActorClockMilliseconds();
  if (packet.target_id) {
    if (auto* target = FindActor(*packet.target_id)) packet.target_generation = target->actor_generation();
  }
  if (SerializeAndSend(transport_, packet, TrafficClass::Control) && packet.kind == ActorActionKind::ItemUse) {
    // Predict the observed native mana so a cast before the acknowledgement
    // reports only its own change, rather than counting the item effect twice.
    actor->set_mana(static_cast<std::int32_t>(std::clamp<std::int64_t>(
        static_cast<std::int64_t>(actor->mana()) + packet.mana_delta, 0, actor->max_mana())));
  }
}

void GameClient::SendEquipmentResult(std::int16_t slot, const std::string& requested, bool success,
                                      const std::string& observed, std::uint32_t revision, std::uint32_t actor_id,
                                      std::uint32_t control_epoch, std::int16_t observed_slot) {
  if (!player_manager_.HasLocalPlayer()) return;
  EquipmentResultPacket packet;
  const auto& local = player_manager_.GetLocalPlayer();
  auto* actor = actor_id ? FindActor(actor_id) : &player_manager_.GetLocalPlayer();
  if (!actor || (actor != &local && (!actor->is_npc() || actor->npc_host_player_id() != local.id()))) return;
  if (actor->is_npc() && control_epoch && control_epoch != actor->npc_control_epoch()) return;
  packet.actor_id = static_cast<std::uint32_t>(actor->id());
  packet.actor_generation = actor->actor_generation();
  packet.control_epoch = actor->is_npc() ? actor->npc_control_epoch() : 0;
  packet.command_revision = revision;
  packet.slot_id = slot;
  packet.item_instance = requested;
  packet.observed_instance = observed;
  packet.observed_slot = observed_slot;
  packet.success = success;
  SerializeAndSend(transport_, packet, TrafficClass::Control);
}

bool GameClient::SendLuaEventToServer(const std::string& event_name, std::uint32_t source_element, const std::string& payload) {
  if (!IsConnected()) {
    SPDLOG_WARN("Cannot trigger server event '{}' while disconnected", event_name);
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
  return SerializeAndSend(transport_, packet, TrafficClass::Control);
}

void GameClient::SyncGameTime() {
  const std::uint8_t data = PT_GAME_INFO;
  if (!transport_.Send(&data, sizeof(data), TrafficClass::Control)) {
    SPDLOG_WARN("Failed to send game time request");
    transport_.Disconnect(Net::CloseReason::Failure);
  }
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
  if (packet.actor_generation == 0) {
    Disconnect();
    const std::string reason = "Invalid actor generation";
    {
      std::lock_guard<std::mutex> lock(connection_mutex_);
      connection_state_ = ConnectionState::Failed;
      connection_close_reason_ = Net::CloseReason::Failure;
      connection_error_ = reason;
    }
    event_observer_.OnConnectionFailed(reason);
    return;
  }
  if (player_manager_.HasLocalPlayer()) return;

  server_name_ = packet.server_name;
  max_slots_ = packet.max_slots;
  event_observer_.OnVoiceConfiguration(packet.voice_enabled != 0, packet.voice_transmit_enabled != 0,
                                       packet.voice_range);

  SPDLOG_INFO("Initial info received: map='{}', server='{}', base_path='{}', group='{}', resources={}, addon_vdfs={}", packet.map_name,
              server_name_.empty() ? "<unknown>" : server_name_,
              packet.resource_base_path.empty() ? "/public" : packet.resource_base_path,
              packet.downloader_group.empty() ? "<ip_port>" : packet.downloader_group, packet.client_resources.size(),
              packet.addon_vdfs.size());

  resource_downloader_.SetDownloadToken(packet.resource_token);
  resource_downloader_.SetBasePath(packet.resource_base_path.empty() ? "/public" : packet.resource_base_path);
  resource_downloader_.SetStoreGroup(packet.downloader_group);
  resource_downloader_.AnnounceResources(std::move(packet.client_resources));
  resource_downloader_.AnnounceAddons(std::move(packet.addon_vdfs));
  resource_downloader_.AnnounceAddonBundle(std::move(packet.addon_bundle));

  auto local_player = player_manager_.CreateLocalPlayer(packet.player_id);
  local_player->set_actor_generation(packet.actor_generation);
  outgoing_state_sequence_ = 0;
  outgoing_action_sequence_ = outgoing_item_request_ = last_item_result_ = 0;
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

  Player* player = FindActor(*packet.player_id, packet.actor_generation);
  // Connection metadata is not an engine actor. An unreliable movement packet
  // can overtake stream-in; it must not make that spawn snapshot look stale.
  const bool deferred_local = player && player_manager_.HasLocalPlayer() && player == &player_manager_.GetLocalPlayer() && !is_in_game_;
  if (!player || (!player->has_spawned() && !deferred_local)) {
    return;
  }
  auto applied = packet.state;
  applied.correction_mask = player->accept_components(packet.state.correction_mask, packet.state_sequence);
  if (!applied.correction_mask) return;
  UpdatePlayerState(player, applied, packet.command_revision);
  player->set_command_revision(packet.command_revision);
  if (deferred_local) pending_local_correction_mask_ |= applied.correction_mask;
  else event_observer_.OnPlayerStateUpdate(*packet.player_id, applied, packet.state.correction_mask != kSyncAll);
}

void GameClient::OnMapOnly(Packet p) {
  PlayerPositionUpdatePacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize PlayerPositionUpdatePacket");
    return;
  }

  if (!packet.player_id) {
    SPDLOG_ERROR("PlayerPositionUpdatePacket: Player id is null");
    return;
  }

  Player* player = FindActor(*packet.player_id, packet.actor_generation);
  const bool deferred_local = player && player_manager_.HasLocalPlayer() && player == &player_manager_.GetLocalPlayer() && !is_in_game_;
  if (!player || (!player->has_spawned() && !deferred_local)) {
    return;
  }
  if (!player->accept_components(kSyncPosition, packet.state_sequence)) {
    SPDLOG_TRACE("Ignoring stale PlayerPositionUpdatePacket for player {} with sequence {}", *packet.player_id, packet.state_sequence);
    return;
  }
  player->set_position(packet.position.x, packet.position.y, packet.position.z);
  player->set_source_time_ms(packet.source_time_ms);

  if (deferred_local) pending_local_correction_mask_ |= kSyncPosition;
  else if (packet.teleport) event_observer_.OnPlayerPositionUpdate(*packet.player_id, packet.position.x, packet.position.y, packet.position.z);
  else {
    auto pose = player->state();
    pose.correction_mask = kSyncPosition;
    event_observer_.OnPlayerStateUpdate(*packet.player_id, pose);
  }
  player->set_command_revision(packet.command_revision);
}

void GameClient::OnDoDie(Packet p) {
  PlayerDeathInfoPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize PlayerDeathInfoPacket");
    return;
  }

  Player* player = FindActor(packet.player_id, packet.actor_generation);
  const bool deferred_local = player && player_manager_.HasLocalPlayer() && player == &player_manager_.GetLocalPlayer() && !is_in_game_;
  if (!player || (!player->has_spawned() && !deferred_local) || !player->accept_lifecycle_sequence(packet.state_sequence)) {
    return;
  }
  const auto applied = player->accept_components(kSyncHealth | kSyncLife, packet.state_sequence, true);
  if (applied & kSyncHealth) player->set_health(0);
  if (applied & kSyncLife) player->set_lifecycle_state(PLAYER_LIFE_DEAD, packet.command_revision);
  if (deferred_local) pending_local_correction_mask_ |= applied;
  event_observer_.OnPlayerDied(packet.player_id, !deferred_local && (applied & kSyncLife) != 0);
  player->set_command_revision(packet.command_revision);
}

void GameClient::OnRespawn(Packet p) {
  PlayerRespawnInfoPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize PlayerRespawnInfoPacket");
    return;
  }

  Player* player = FindActor(packet.player_id, packet.actor_generation);
  const bool deferred_local = player && player_manager_.HasLocalPlayer() && player == &player_manager_.GetLocalPlayer() && !is_in_game_;
  if (!player || (!player->has_spawned() && !deferred_local) || !player->accept_lifecycle_sequence(packet.state_sequence)) {
    return;
  }
  const auto applied = player->accept_components(kSyncHealth | kSyncMana | kSyncLife, packet.state_sequence, true);
  if (applied & kSyncHealth) player->set_health(packet.health);
  if (applied & kSyncMana) player->set_mana(packet.mana);
  if (applied & kSyncLife) player->set_lifecycle_state(PLAYER_LIFE_ALIVE, packet.command_revision);
  if (deferred_local) pending_local_correction_mask_ |= applied;
  event_observer_.OnPlayerRespawned(packet.player_id, !deferred_local && (applied & kSyncLife) != 0);
  player->set_command_revision(packet.command_revision);
}

void GameClient::OnCastSpell(Packet p) {
  CastSpellPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize CastSpellPacket");
    return;
  }

  if (!packet.caster_id || packet.spell_id >= 100 || packet.spell_level > kMaxSpellInvestLevel) {
    SPDLOG_ERROR("Invalid CastSpellPacket caster, spell ID or effect level");
    return;
  }

  auto* actor = FindActor(*packet.caster_id, packet.actor_generation);
  if (!actor || !actor->has_spawned() || !actor->accepts_life_action_revision(packet.command_revision) ||
      (actor->is_npc() && actor->npc_control_epoch() != packet.control_epoch) ||
      !actor->accept_action_sequence(packet.action_id)) return;
  event_observer_.OnSpellCast(packet);
}

void GameClient::OnCastSpellOnTarget(Packet p) {
  CastSpellPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize CastSpellPacket");
    return;
  }

  if (!packet.caster_id || !packet.target_id || packet.spell_id >= 100 || packet.spell_level > kMaxSpellInvestLevel) {
    SPDLOG_ERROR("Invalid CastSpellOnTarget packet caster, target, spell ID or effect level");
    return;
  }

  auto* actor = FindActor(*packet.caster_id, packet.actor_generation);
  if (!actor || !actor->has_spawned() || !actor->accepts_life_action_revision(packet.command_revision) ||
      !FindActor(*packet.target_id, packet.target_generation) || (actor->is_npc() && actor->npc_control_epoch() != packet.control_epoch) ||
      !actor->accept_action_sequence(packet.action_id))
    return;
  event_observer_.OnSpellCast(packet);
}

void GameClient::OnDropItem(Packet p) {
  DropItemPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize DropItemPacket");
    return;
  }

  if (!packet.player_id) {
    SPDLOG_ERROR("Invalid DropItem packet. No player id.");
    return;
  }

  if (FindActor(*packet.player_id, packet.actor_generation))
    event_observer_.OnItemDropped(*packet.player_id, packet.item_instance, packet.item_amount);
}

void GameClient::OnTakeItem(Packet p) {
  TakeItemPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize TakeItemPacket");
    return;
  }

  if (!packet.player_id) {
    SPDLOG_ERROR("Invalid TakeItem packet. No player id.");
    return;
  }

  if (FindActor(*packet.player_id, packet.actor_generation)) event_observer_.OnItemTaken(*packet.player_id, packet.item_instance);
}

void GameClient::OnItemGroundCreate(Packet p) {
  ItemGroundCreatePacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize ItemGroundCreatePacket");
    return;
  }

  event_observer_.OnItemGroundCreate(packet.item_ground_id, packet.item_instance, packet.amount, packet.physics_enabled,
                                     packet.position, packet.rotation);
}

void GameClient::OnItemGroundDestroy(Packet p) {
  ItemGroundDestroyPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize ItemGroundDestroyPacket");
    return;
  }

  event_observer_.OnItemGroundDestroy(packet.item_ground_id);
}

void GameClient::OnItemsGroundDestroy(Packet p) {
  ItemGroundClearPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize ItemGroundClearPacket");
    return;
  }

  event_observer_.OnItemsGroundDestroy();
}

void GameClient::OnGiveItem(Packet p) {
  GiveItemPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize GiveItemPacket");
    return;
  }

  auto* actor = FindActor(packet.player_id, packet.actor_generation);
  if (!actor || !gmp::IsValidScriptName(packet.item_instance) || packet.item_amount <= 0 ||
      !actor->accept_inventory_sequence(packet.item_instance, packet.state_sequence)) return;
  actor->apply_inventory_delta(packet.item_instance, packet.item_amount);
  if (player_manager_.HasLocalPlayer() && actor == &player_manager_.GetLocalPlayer() && !is_in_game_)
    pending_inventory_commands_.push_back({true, packet.item_instance, packet.item_amount});
  else event_observer_.OnItemGiven(packet.player_id, packet.item_instance, packet.item_amount);
  actor->set_command_revision(packet.command_revision);
}

void GameClient::OnEquipItem(Packet p) {
  EquipItemPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize EquipItemPacket");
    return;
  }

  auto* actor = FindActor(packet.player_id, packet.actor_generation);
  if (!actor) return;
  const bool apply = actor->accept_components(kSyncEquipment, packet.state_sequence, true) != 0;
  if (apply) ApplyEquipmentSlot(*actor, packet.slot_id, packet.item_instance);
  if (player_manager_.HasLocalPlayer() && actor == &player_manager_.GetLocalPlayer() && !is_in_game_) {
    if (apply) pending_local_correction_mask_ |= kSyncEquipment;
    pending_equipment_verifications_.push_back({true, packet.item_instance, packet.slot_id, packet.command_revision});
  } else event_observer_.OnItemEquipped(packet.player_id, packet.item_instance, packet.slot_id, packet.command_revision, apply);
  actor->set_command_revision(packet.command_revision);
}

void GameClient::OnUnequipItem(Packet p) {
  UnequipItemPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize UnequipItemPacket");
    return;
  }

  auto* actor = FindActor(packet.player_id, packet.actor_generation);
  if (!actor) return;
  const bool apply = actor->accept_components(kSyncEquipment, packet.state_sequence, true) != 0;
  if (apply) ApplyEquipmentSlot(*actor, packet.slot_id, {});
  if (player_manager_.HasLocalPlayer() && actor == &player_manager_.GetLocalPlayer() && !is_in_game_) {
    if (apply) pending_local_correction_mask_ |= kSyncEquipment;
    pending_equipment_verifications_.push_back({false, packet.item_instance, packet.slot_id, packet.command_revision});
  } else event_observer_.OnItemUnequipped(packet.player_id, packet.item_instance, packet.slot_id, packet.command_revision, apply);
  actor->set_command_revision(packet.command_revision);
}

void GameClient::OnRemoveItem(Packet p) {
  RemoveItemPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize RemoveItemPacket");
    return;
  }

  auto* actor = FindActor(packet.player_id, packet.actor_generation);
  if (!actor || !gmp::IsValidScriptName(packet.item_instance) || packet.item_amount <= 0 ||
      !actor->accept_inventory_sequence(packet.item_instance, packet.state_sequence)) return;
  actor->apply_inventory_delta(packet.item_instance, -packet.item_amount);
  if (player_manager_.HasLocalPlayer() && actor == &player_manager_.GetLocalPlayer() && !is_in_game_)
    pending_inventory_commands_.push_back({false, packet.item_instance, packet.item_amount});
  else event_observer_.OnItemRemoved(packet.player_id, packet.item_instance, packet.item_amount);
  actor->set_command_revision(packet.command_revision);
}

void GameClient::OnActorDamage(Packet p) {
  ActorDamagePacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  const auto decoded = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (decoded.first != bitsery::ReaderError::NoError || !decoded.second || packet.damage <= 0)
    return;
  auto* victim = FindActor(packet.victim_id, packet.victim_generation);
  if (!victim || !victim->has_spawned() || victim->life_state() != PLAYER_LIFE_ALIVE ||
      !victim->accepts_life_action_revision(packet.command_revision) || !victim->accept_damage_sequence(packet.state_sequence))
    return;
  if (packet.attacker_id && !FindActor(*packet.attacker_id, packet.attacker_generation))
    packet.attacker_id.reset();
  event_observer_.OnActorDamage(packet);
}

void GameClient::OnPlayerSummon(Packet p) {
  PlayerSummonPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  const auto decoded = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (decoded.first != bitsery::ReaderError::NoError || !decoded.second || packet.spell_id >= 100 || packet.summon_instance.empty())
    return;
  auto* caster = FindActor(packet.caster_id, packet.caster_generation);
  auto* npc = FindActor(packet.npc_id, packet.npc_generation);
  if (!caster || !caster->has_spawned() || !npc || !npc->has_spawned() || !npc->is_npc() || npc->instance() != packet.summon_instance ||
      !npc->accept_summon_notification())
    return;
  event_observer_.OnPlayerSummon(packet);
}

void GameClient::OnPlayerTeleport(Packet p) {
  PlayerTeleportPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  const auto decoded = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (decoded.first != bitsery::ReaderError::NoError || !decoded.second || !IsFiniteActorVector(packet.position) ||
      !IsFiniteActorVector(packet.normal) || glm::dot(packet.normal, packet.normal) < 0.0001f || glm::dot(packet.normal, packet.normal) > 4.0f ||
      packet.destination.empty())
    return;
  auto* actor = FindActor(packet.player_id, packet.actor_generation);
  if (!actor || !actor->has_spawned() || !actor->accepts_life_action_revision(packet.command_revision) ||
      (actor->is_npc() && actor->npc_control_epoch() != packet.control_epoch) || !actor->accept_action_sequence(packet.action_id))
    return;
  const auto mask = actor->accept_components(kSyncPosition | kSyncOrientation, packet.state_sequence);
  if (mask & kSyncPosition)
    actor->set_position(packet.position.x, packet.position.y, packet.position.z);
  if (mask & kSyncOrientation)
    actor->set_rotation(packet.normal);
  if (mask)
    actor->set_source_time_ms(packet.source_time_ms);
  actor->set_command_revision(packet.command_revision);
  event_observer_.OnPlayerTeleport(packet, mask);
}

void GameClient::OnPlayerTransformation(Packet p) {
  PlayerTransformationPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  const auto decoded = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (decoded.first != bitsery::ReaderError::NoError || !decoded.second || (packet.spell_id != 0 && packet.instance.empty()) ||
      (packet.spell_id != 0 && (packet.spell_id < 47 || packet.spell_id > 58)) || packet.max_health < 1 || packet.max_mana < 0 ||
      packet.strength < 0 || packet.dexterity < 0 || !IsFiniteActorVector(packet.state.position) || !IsFiniteActorVector(packet.state.nrot))
    return;
  auto* actor = FindActor(packet.player_id, packet.actor_generation);
  if (!actor || !actor->has_spawned() || !actor->accepts_life_action_revision(packet.command_revision) ||
      !actor->accept_transformation_sequence(packet.state_sequence))
    return;
  actor->set_instance(packet.instance);
  actor->set_transformation(packet.spell_id, packet.spell_id ? packet.cast_id : 0);
  actor->set_life_command_revision(packet.command_revision);
  const std::pair<PlayerAttributeId, std::int32_t> attributes[] = {
      {ATTR_MAX_HEALTH, packet.max_health}, {ATTR_MAX_MANA, packet.max_mana}, {ATTR_STRENGTH, packet.strength}, {ATTR_DEXTERITY, packet.dexterity}};
  for (const auto& [attribute, value] : attributes) {
    if (actor->accept_attribute_sequence(attribute, packet.state_sequence))
      ApplyAttribute(*actor, attribute, value);
  }
  packet.state.correction_mask = actor->accept_components(kSyncAll, packet.state_sequence);
  UpdatePlayerState(actor, packet.state, packet.command_revision);
  actor->set_command_revision(packet.command_revision);
  event_observer_.OnPlayerTransformation(packet, packet.state.correction_mask);
}

void GameClient::OnActorAction(Packet p) {
  ActorActionPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  const auto decoded = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (decoded.first != bitsery::ReaderError::NoError || !decoded.second) return;
  // Persistent world objects outlive the actor who last used them. The server
  // sends their absolute state on ready/stream-in with no actor identity.
  if (packet.kind == ActorActionKind::MobState) {
    if (!player_manager_.HasLocalPlayer() ||
        !player_manager_.GetLocalPlayer().matches_generation(packet.target_generation)) return;
    if (world_objects_ready_) event_observer_.OnActorAction(packet);
    else {
      const auto pending = std::find_if(pending_mob_states_.begin(), pending_mob_states_.end(), [&](const auto& existing) {
        return existing.name == packet.name && glm::distance(existing.position, packet.position) < 50.0f;
      });
      if (pending != pending_mob_states_.end()) *pending = packet;
      else if (pending_mob_states_.size() < 4096) pending_mob_states_.push_back(packet);
    }
    return;
  }
  auto* actor = FindActor(packet.actor_id, packet.actor_generation);
  if (!actor || !actor->has_spawned() || !actor->accepts_life_action_revision(packet.command_revision) ||
      (actor->is_npc() && actor->npc_control_epoch() != packet.control_epoch) ||
      !actor->accept_action_sequence(packet.action_id)) return;
  if (packet.target_id && !FindActor(*packet.target_id, packet.target_generation)) packet.target_id.reset();
  event_observer_.OnActorAction(packet);
}

void GameClient::DrainMobStateReplay() {
  auto pending = std::move(pending_mob_states_);
  pending_mob_states_.clear();
  for (const auto& packet : pending) {
    if (!world_objects_ready_ || !player_manager_.HasLocalPlayer() ||
        !player_manager_.GetLocalPlayer().matches_generation(packet.target_generation)) continue;
    event_observer_.OnActorAction(packet);
  }
}

void GameClient::OnItemTransactionResult(Packet p) {
  ItemTransactionResultPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  const auto decoded = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (decoded.first != bitsery::ReaderError::NoError || !decoded.second || !player_manager_.HasLocalPlayer()) return;
  auto& local = player_manager_.GetLocalPlayer();
  auto* actor = packet.actor_id ? FindActor(packet.actor_id, packet.actor_generation) : &local;
  if (!actor || !actor->matches_generation(packet.actor_generation) || !gmp::IsValidScriptName(packet.item_instance) || packet.resulting_amount < 0) return;
  if (packet.request_id) {
    if (actor != &local) return;
    if (last_item_result_ && !IsNewActorSequence(packet.request_id, last_item_result_)) return;
    last_item_result_ = packet.request_id;
  }
  const bool fresh_inventory = actor->accept_inventory_sequence(packet.item_instance, packet.state_sequence);
  if (!fresh_inventory && !packet.request_id) return;
  if (fresh_inventory) actor->set_inventory_count(packet.item_instance, packet.resulting_amount);
  event_observer_.OnActorItemTransactionResult(static_cast<std::uint32_t>(actor->id()), packet.request_id,
                                         packet.item_instance, actor->inventory_count(packet.item_instance),
                                         packet.accepted, packet.item_ground_id);
}

void GameClient::OnMessage(Packet p) {
  MessagePacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize MessagePacket");
    return;
  }

  Player* sender = packet.sender ? player_manager_.GetPlayer(*packet.sender) : nullptr;
  std::string sender_name = sender ? sender->name() : "";

  if (packet.sender) {
    SPDLOG_INFO("Message from player {} ({}): {}", sender_name, *packet.sender, packet.message);
  } else {
    SPDLOG_INFO("Server chat message: {}", packet.message);
  }

  event_observer_.OnPlayerMessage(packet.sender, packet.r, packet.g, packet.b, packet.message);
}

void GameClient::OnCharacterSpawn(Packet p) {
  PlayerSpawnPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (state.first != bitsery::ReaderError::NoError || !state.second) {
    SPDLOG_ERROR("Failed to deserialize PlayerSpawnPacket");
    return;
  }

  SPDLOG_INFO("PlayerSpawn packet: {}", packet);

  ApplyPlayerSpawn(packet);
}

void GameClient::ApplyNativeInstanceDefaults(const PlayerSpawnPacket& defaults) {
  auto* actor = FindActor(defaults.player_id, defaults.actor_generation);
  if (actor) {
    actor->reset_inventory_snapshot();
    ApplyPlayerSpawn(defaults, actor->is_npc(), true);
  }
}

void GameClient::SendInstanceReady(const PlayerSpawnPacket& defaults, bool success) {
  auto* actor = FindActor(defaults.player_id, defaults.actor_generation);
  if (!IsConnected() || !player_manager_.HasLocalPlayer() || !actor || !actor->instance_change_revision())
    return;
  const auto local_id = player_manager_.GetLocalPlayer().id();
  if (actor->id() != local_id && (!actor->is_npc() || actor->npc_host_player_id() != local_id))
    return;
  PlayerInstanceReadyPacket packet;
  packet.request_revision = actor->instance_change_revision();
  packet.control_epoch = actor->is_npc() ? actor->npc_control_epoch() : 0;
  packet.success = success;
  packet.defaults = defaults;
  SerializeAndSend(transport_, packet, TrafficClass::Control);
}

void GameClient::ApplyPlayerSpawn(const PlayerSpawnPacket& packet, bool is_npc, bool native_defaults) {
  if (packet.actor_generation == 0 || !IsFiniteActorVector(packet.position) || !IsFiniteActorVector(packet.normal)) return;
  std::unordered_set<std::string> inventory_instances;
  for (const auto& entry : packet.inventory) {
    if (!gmp::IsValidScriptName(entry.item_instance) || entry.amount <= 0 || !inventory_instances.insert(entry.item_instance).second) return;
  }
  const bool local = player_manager_.HasLocalPlayer() && player_manager_.GetLocalPlayer().id() == packet.player_id;
  auto* player = FindActor(packet.player_id);
  if (player && player->actor_generation() > packet.actor_generation) return;
  if (player && player->is_npc() != is_npc) return;
  if (!player) player = player_manager_.CreatePlayer(packet.player_id);
  const bool changed_generation = player->actor_generation() != packet.actor_generation;
  const bool was_spawned = player->has_spawned() && !changed_generation;
  if (changed_generation && player->has_spawned() && !local) {
    event_observer_.OnPlayerLeft(player->id(), player->name());
  }
  player->set_actor_generation(packet.actor_generation);
  player->set_instance_change_revision(packet.instance_change_revision);
  player->set_command_revision(packet.command_revision);
  if (local && changed_generation) {
    pending_hit_batches_.clear();
    pending_local_correction_mask_ = 0;
    pending_equipment_verifications_.clear();
    pending_inventory_commands_.clear();
    ResetMobStateReplay();
  }
  player->set_is_npc(is_npc);
  if ((local || is_npc) && player->begin_inventory_snapshot(packet.state_sequence)) {
    for (const auto& entry : packet.inventory) {
      player->set_inventory_count(entry.item_instance, entry.amount);
    }
  }
  player->set_name(packet.player_name);
  player->set_instance(packet.instance);
  player->set_transformation(packet.transformation_spell_id, packet.transformation_cast_id);
  player->set_body_model(packet.body_model);
  player->set_body_texture(packet.body_texture);
  player->set_head_model(packet.head_model);
  player->set_head_texture(packet.head_texture);
  player->set_teeth_texture(packet.teeth_texture);
  player->set_skin_color(packet.skin_color);
  player->set_walk_style(packet.walk_style);
  player->set_name_color(packet.name_color_r, packet.name_color_g, packet.name_color_b);
  player->set_fatness(packet.fatness);
  player->set_scale(packet.scale);

  PlayerState actor_state;
  actor_state.source_time_ms = packet.source_time_ms;
  actor_state.position = packet.position;
  actor_state.nrot = packet.normal;
  actor_state.left_hand_item_instance = packet.left_hand_item_instance;
  actor_state.right_hand_item_instance = packet.right_hand_item_instance;
  actor_state.equipped_armor_instance = packet.equipped_armor_instance;
  actor_state.equipped_helmet_instance = packet.equipped_helmet_instance;
  actor_state.equipped_shield_instance = packet.equipped_shield_instance;
  actor_state.equipped_amulet_instance = packet.equipped_amulet_instance;
  actor_state.equipped_belt_instance = packet.equipped_belt_instance;
  actor_state.equipped_ring_left_instance = packet.equipped_ring_left_instance;
  actor_state.equipped_ring_right_instance = packet.equipped_ring_right_instance;
  actor_state.melee_weapon_instance = packet.melee_weapon_instance;
  actor_state.ranged_weapon_instance = packet.ranged_weapon_instance;
  actor_state.weapon_mode = packet.weapon_mode;
  actor_state.active_spell_nr = packet.active_spell_nr;
  actor_state.active_spell_instance = packet.active_spell_instance;
  actor_state.active_spell_level = packet.active_spell_level;
  actor_state.spell_investing = packet.spell_investing;
  actor_state.head_direction = packet.head_direction;
  actor_state.body_state = packet.body_state;
  actor_state.walk_mode = packet.walk_mode;
  actor_state.look_yaw = packet.look_yaw;
  actor_state.look_pitch = packet.look_pitch;
  actor_state.focus_target_id = packet.focus_target_id;
  actor_state.animation_name = packet.animation_name;
  actor_state.health_points = packet.health;
  actor_state.mana_points = packet.mana;
  actor_state.life_state = packet.life_state;
  actor_state.invulnerable = packet.invulnerable;
  actor_state.correction_mask = native_defaults ? kSyncAll : player->accept_components(kSyncAll, packet.state_sequence, !was_spawned);
  UpdatePlayerState(player, actor_state, packet.command_revision);

  const std::pair<PlayerAttributeId, std::int32_t> attributes[] = {
      {ATTR_STRENGTH, packet.strength}, {ATTR_DEXTERITY, packet.dexterity}, {ATTR_LEVEL, packet.level},
      {ATTR_EXP, packet.exp}, {ATTR_NEXT_LEVEL_EXP, packet.next_level_exp}, {ATTR_LEARN_POINTS, packet.learn_points},
      {ATTR_MAX_HEALTH, packet.max_health}, {ATTR_MAX_MANA, packet.max_mana}, {ATTR_HEALTH, player->health()}, {ATTR_MANA, player->mana()}};
  std::vector<std::pair<PlayerAttributeId, std::int32_t>> accepted_attributes;
  for (const auto& entry : attributes) {
    if (native_defaults || player->accept_attribute_sequence(entry.first, packet.state_sequence)) {
      ApplyAttribute(*player, entry.first, entry.second);
      accepted_attributes.push_back(entry);
    }
  }
  const auto old_overlays = player->overlays();
  player->clear_snapshot_collections();
  for (const auto& entry : packet.weapon_skills) player->set_weapon_skill(entry.skill_id, entry.percentage);
  for (const auto& entry : packet.talents) player->set_talent(entry.talent_id, entry.value);
  for (const auto& overlay : packet.overlays) player->add_overlay(overlay);
  player->set_has_spawned(true);
  if (native_defaults)
    return;
  if (local) {
    is_in_game_ = true;
    if (!was_spawned) event_observer_.OnLocalPlayerSpawned(*player);
  } else if (!was_spawned) {
    event_observer_.OnPlayerSpawned(*player);
  }
  if (!packet.instance.empty()) event_observer_.OnPlayerInstanceUpdate(packet.player_id, packet.instance);
  if (player->instance_change_revision())
    return;
  event_observer_.OnPlayerColorUpdate(packet.player_id, packet.name_color_r, packet.name_color_g, packet.name_color_b);
  if (!packet.body_model.empty() || !packet.head_model.empty()) {
    event_observer_.OnPlayerVisualUpdate(packet.player_id, packet.body_model, packet.body_texture, packet.head_model,
                                         packet.head_texture, packet.teeth_texture, packet.skin_color);
  }
  event_observer_.OnPlayerFatnessUpdate(packet.player_id, packet.fatness);
  event_observer_.OnPlayerScaleUpdate(packet.player_id, packet.scale);
  for (const auto& entry : accepted_attributes) event_observer_.OnPlayerAttributeUpdate(packet.player_id, entry.first, entry.second);
  for (const auto& entry : packet.weapon_skills) event_observer_.OnPlayerSkillWeaponUpdate(packet.player_id, entry.skill_id, entry.percentage);
  for (const auto& entry : packet.talents) event_observer_.OnPlayerTalentUpdate(packet.player_id, entry.talent_id, entry.value);
  for (const auto& overlay : old_overlays) {
    if (std::find(packet.overlays.begin(), packet.overlays.end(), overlay) == packet.overlays.end())
      event_observer_.OnPlayerOverlayUpdate(packet.player_id, overlay, false);
  }
  for (const auto& overlay : packet.overlays) event_observer_.OnPlayerOverlayUpdate(packet.player_id, overlay, true);
  if (local || is_npc) event_observer_.OnActorInventorySnapshot(packet.player_id, packet.inventory);
  // Construction and equipment scripts can temporarily change cached values.
  // Reapply accepted components after all native snapshot callbacks complete.
  UpdatePlayerState(player, actor_state, packet.command_revision);
  event_observer_.OnPlayerSpawnSnapshotApplied(*player, !was_spawned);
  player->set_command_revision(packet.command_revision);
}
void GameClient::OnNpcSpawn(Packet p) {
  NpcSpawnPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  const auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (state.first != bitsery::ReaderError::NoError || !state.second || !IsValidNpcControl(packet.control, max_slots_) ||
      packet.actor.packet_type != PT_PLAYER_SPAWN || packet.control.packet_type != PT_NPC_CONTROL ||
      packet.actor.player_id != packet.control.npc_id || packet.animation.npc_id != packet.actor.player_id ||
      packet.actor.actor_generation != packet.control.actor_generation || packet.actor.actor_generation != packet.animation.actor_generation ||
      packet.animation.packet_type != PT_NPC_ANIMATION || !IsValidNpcAnimation(packet.animation, max_slots_)) {
    SPDLOG_WARN("Rejected invalid NpcSpawnPacket");
    return;
  }
  const auto* existing = player_manager_.GetPlayer(packet.actor.player_id);
  if (existing && (!existing->is_npc() || existing->actor_generation() > packet.actor.actor_generation ||
      (existing->matches_generation(packet.actor.actor_generation) && packet.control.control_epoch < existing->npc_control_epoch()))) {
    return;
  }
  ApplyPlayerSpawn(packet.actor, true);
  ApplyNpcControl(packet.control);
  ApplyNpcAnimation(packet.animation);
}

void GameClient::OnNpcAnimation(Packet p) {
  NpcAnimationPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  const auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (state.first != bitsery::ReaderError::NoError || !state.second || !IsValidNpcAnimation(packet, max_slots_)) {
    SPDLOG_WARN("Rejected invalid NpcAnimationPacket");
    return;
  }
  ApplyNpcAnimation(packet);
}

void GameClient::ApplyNpcAnimation(const NpcAnimationPacket& animation) {
  auto* npc = FindActor(animation.npc_id, animation.actor_generation);
  if (!npc || !npc->is_npc() || !npc->has_spawned() || !IsNewActorSequence(animation.revision, npc->npc_animation_revision())) {
    return;
  }
  npc->set_npc_animation_revision(animation.revision);
  event_observer_.OnNpcAnimation(animation);
}

void GameClient::OnNpcControl(Packet p) {
  NpcControlPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  const auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (state.first != bitsery::ReaderError::NoError || !state.second || !IsValidNpcControl(packet, max_slots_)) {
    SPDLOG_WARN("Rejected invalid NpcControlPacket");
    return;
  }
  ApplyNpcControl(packet);
}

void GameClient::ApplyNpcControl(const NpcControlPacket& control) {
  auto* npc = FindActor(control.npc_id, control.actor_generation);
  if (!npc || !npc->is_npc() || !npc->has_spawned() || !IsNewActorSequence(control.control_epoch, npc->npc_control_epoch())) {
    return;
  }
  npc->set_npc_control(control.control_epoch, control.host_player_id, control.action_id);
  auto applied_control = control;
  const auto accepted = npc->accept_components(kSyncPosition | kSyncOrientation, control.state_sequence, true);
  if (accepted & kSyncPosition) {
    npc->set_position(control.position);
    if (control.source_time_ms != 0) npc->set_source_time_ms(control.source_time_ms);
    else applied_control.source_time_ms = npc->source_time_ms();
  } else {
    // Queue/host generations and transform revisions are independent. A valid
    // new action must not rewind movement already received by this viewer.
    applied_control.position = npc->position();
    applied_control.source_time_ms = npc->source_time_ms();
  }
  if (accepted & kSyncOrientation) npc->set_rotation(control.normal);
  else applied_control.normal = npc->rotation();
  event_observer_.OnNpcControl(applied_control);
}

void GameClient::SendNpcActionResult(std::uint32_t npc_id, std::uint32_t epoch, std::uint32_t action_id, bool success) {
  auto* npc = player_manager_.GetPlayer(npc_id);
  if (!IsConnected() || !player_manager_.HasLocalPlayer() || !npc || !npc->is_npc() || action_id == 0 ||
      npc->npc_host_player_id() != player_manager_.GetLocalPlayer().id() || npc->npc_control_epoch() != epoch ||
      npc->npc_action_id() != action_id) {
    return;
  }
  NpcActionResultPacket packet;
  packet.actor_generation = npc->actor_generation();
  packet.npc_id = npc_id;
  packet.control_epoch = epoch;
  packet.action_id = action_id;
  packet.success = success ? 1 : 0;
  SerializeAndSend(transport_, packet, TrafficClass::Control);
}

void GameClient::OnJoinGame(Packet p) {
  JoinGamePacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize JoinGamePacket");
    return;
  }

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
  if (packet.actor_generation == 0 || (player->actor_generation() && player->actor_generation() != packet.actor_generation)) return;
  player->set_actor_generation(packet.actor_generation);
  const bool was_joined = player->has_joined();
  if (was_joined) return;
  player->set_name(packet.player_name);
  player->set_position(packet.position.x, packet.position.y, packet.position.z);
  player->set_rotation(packet.normal);
  player->set_left_hand_item(packet.left_hand_item_instance);
  player->set_right_hand_item(packet.right_hand_item_instance);
  player->set_equipped_armor(packet.equipped_armor_instance);
  player->set_equipped_helmet(packet.equipped_helmet_instance);
  player->set_equipped_shield(packet.equipped_shield_instance);
  player->set_equipped_amulet(packet.equipped_amulet_instance);
  player->set_equipped_belt(packet.equipped_belt_instance);
  player->set_equipped_ring_left(packet.equipped_ring_left_instance);
  player->set_equipped_ring_right(packet.equipped_ring_right_instance);
  player->set_animation_name(packet.animation_name);
  player->set_body_model(packet.body_model);
  player->set_body_texture(packet.body_texture);
  player->set_head_model(packet.head_model);
  player->set_head_texture(packet.head_texture);
  player->set_teeth_texture(packet.teeth_texture);
  player->set_skin_color(packet.skin_color);
  player->set_walk_style(packet.walk_style);
  if (!was_joined) {
    if (!packet.walking_overlay.empty()) player->add_overlay(packet.walking_overlay);
    player->set_has_joined(true);
    event_observer_.OnPlayerJoined(*player);
  }
}

void GameClient::OnPlayerNameUpdate(Packet p) {
  PlayerNameUpdatePacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize PlayerNameUpdatePacket");
    return;
  }

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
  if (state.first != bitsery::ReaderError::NoError || !state.second || packet.instance.empty()) {
    SPDLOG_ERROR("Failed to deserialize PlayerInstanceUpdatePacket");
    return;
  }

  SPDLOG_DEBUG("PlayerInstanceUpdatePacket: {}", packet);
  Player* player = FindActor(packet.player_id, packet.actor_generation);
  if (!player || (packet.command_revision != player->command_revision() && !IsNewActorSequence(packet.command_revision, player->command_revision())))
    return;
  player->set_instance(packet.instance);
  player->set_command_revision(packet.command_revision);
  if (packet.initialize_defaults)
    player->set_instance_change_revision(packet.command_revision);
  event_observer_.OnPlayerInstanceUpdate(packet.player_id, packet.instance);
}

void GameClient::OnPlayerColorUpdate(Packet p) {
  PlayerColorUpdatePacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize PlayerColorUpdatePacket");
    return;
  }

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
  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize PlayerSkillWeaponUpdatePacket");
    return;
  }

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
  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize PlayerTalentUpdatePacket");
    return;
  }

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
  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize PlayerVisualUpdatePacket");
    return;
  }

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
    player->set_teeth_texture(packet.teeth_texture);
    player->set_skin_color(packet.skin_color);
  }

  event_observer_.OnPlayerVisualUpdate(packet.player_id, packet.body_model, packet.body_texture, packet.head_model, packet.head_texture,
                                       packet.teeth_texture, packet.skin_color);
}

void GameClient::OnPlayerFatnessUpdate(Packet p) {
  PlayerFatnessUpdatePacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize PlayerFatnessUpdatePacket");
    return;
  }

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
  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize PlayerScaleUpdatePacket");
    return;
  }

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
  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize PlayerOverlayUpdatePacket");
    return;
  }

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

void GameClient::OnPlayerAnimationPlay(Packet p) {
  PlayerAnimationPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize PlayerAnimationPacket");
    return;
  }

  SPDLOG_DEBUG("PlayerAnimationPacket: {}", packet);

  event_observer_.OnPlayerAnimationPlay(packet.player_id, packet.animation);
}

void GameClient::OnPlayerAnimationStop(Packet p) {
  PlayerAnimationStopPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize PlayerAnimationStopPacket");
    return;
  }

  SPDLOG_DEBUG("PlayerAnimationStopPacket: {}", packet);

  event_observer_.OnPlayerAnimationStop(packet.player_id, packet.animation);
}

void GameClient::OnPlayerFaceAnimationPlay(Packet p) {
  PlayerFaceAnimationPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize PlayerFaceAnimationPacket");
    return;
  }

  SPDLOG_DEBUG("PlayerFaceAnimationPacket: {}", packet);

  event_observer_.OnPlayerFaceAnimationPlay(packet.player_id, packet.animation);
}

void GameClient::OnPlayerFaceAnimationStop(Packet p) {
  PlayerFaceAnimationStopPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize PlayerFaceAnimationStopPacket");
    return;
  }

  SPDLOG_DEBUG("PlayerFaceAnimationStopPacket: {}", packet);

  event_observer_.OnPlayerFaceAnimationStop(packet.player_id, packet.animation);
}

void GameClient::OnPlayerGesticulation(Packet p) {
  PlayerGesticulationPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize PlayerGesticulationPacket");
    return;
  }

  SPDLOG_DEBUG("PlayerGesticulationPacket: {}", packet);

  event_observer_.OnPlayerGesticulation(packet.player_id);
}

void GameClient::ApplyAttribute(Player& player, PlayerAttributeId id, std::int32_t value) {
  switch (id) {
    case ATTR_STRENGTH: player.set_strength(value); break;
    case ATTR_DEXTERITY: player.set_dexterity(value); break;
    case ATTR_LEVEL: player.set_level(value); break;
    case ATTR_EXP: player.set_exp(value); break;
    case ATTR_NEXT_LEVEL_EXP: player.set_next_level_exp(value); break;
    case ATTR_LEARN_POINTS: player.set_learn_points(value); break;
    case ATTR_HEALTH: player.set_health(value); break;
    case ATTR_MAX_HEALTH: player.set_max_health(value); break;
    case ATTR_MANA: player.set_mana(value); break;
    case ATTR_MAX_MANA: player.set_max_mana(value); break;
  }
}

void GameClient::OnPlayerAttributeUpdate(Packet p) {
  PlayerAttributeUpdatePacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  const auto decoded = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (decoded.first != bitsery::ReaderError::NoError || !decoded.second) return;
  auto* player = FindActor(packet.player_id, packet.actor_generation);
  if (!player) return;
  if (player->accept_attribute_sequence(packet.attribute_id, packet.state_sequence)) {
    ApplyAttribute(*player, packet.attribute_id, packet.value);
    const bool deferred_local = player_manager_.HasLocalPlayer() && player == &player_manager_.GetLocalPlayer() && !is_in_game_;
    if (deferred_local) {
      if (packet.attribute_id == ATTR_HEALTH) pending_local_correction_mask_ |= kSyncHealth;
      if (packet.attribute_id == ATTR_MANA) pending_local_correction_mask_ |= kSyncMana;
    } else event_observer_.OnPlayerAttributeUpdate(packet.player_id, packet.attribute_id, packet.value);
  }
  player->set_command_revision(packet.command_revision);
}

void GameClient::OnPlayerAttributeSnapshot(Packet p) {
  PlayerAttributeSnapshotPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  const auto decoded = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (decoded.first != bitsery::ReaderError::NoError || !decoded.second) return;
  auto* player = FindActor(packet.player_id, packet.actor_generation);
  if (!player) return;
  const std::pair<PlayerAttributeId, std::int32_t> attributes[] = {
      {ATTR_STRENGTH, packet.strength}, {ATTR_DEXTERITY, packet.dexterity}, {ATTR_LEVEL, packet.level},
      {ATTR_EXP, packet.exp}, {ATTR_NEXT_LEVEL_EXP, packet.next_level_exp}, {ATTR_LEARN_POINTS, packet.learn_points},
      {ATTR_MAX_HEALTH, packet.max_health}, {ATTR_MAX_MANA, packet.max_mana}, {ATTR_HEALTH, packet.health}, {ATTR_MANA, packet.mana}};
  for (const auto& entry : attributes) {
    if (!player->accept_attribute_sequence(entry.first, packet.state_sequence)) continue;
    ApplyAttribute(*player, entry.first, entry.second);
    const bool deferred_local = player_manager_.HasLocalPlayer() && player == &player_manager_.GetLocalPlayer() && !is_in_game_;
    if (deferred_local) {
      if (entry.first == ATTR_HEALTH) pending_local_correction_mask_ |= kSyncHealth;
      if (entry.first == ATTR_MANA) pending_local_correction_mask_ |= kSyncMana;
    } else event_observer_.OnPlayerAttributeUpdate(packet.player_id, entry.first, entry.second);
  }
  player->set_command_revision(packet.command_revision);
}
void GameClient::OnPlayerWorldUpdate(Packet p) {
  PlayerWorldUpdatePacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize PlayerWorldUpdatePacket");
    return;
  }

  SPDLOG_DEBUG("PlayerWorldUpdatePacket: {}", packet);
  auto* player = FindActor(packet.player_id);
  if (!player || packet.actor_generation == 0 || player->actor_generation() > packet.actor_generation) return;
  if (player_manager_.HasLocalPlayer() && player == &player_manager_.GetLocalPlayer() &&
      !player->matches_generation(packet.actor_generation)) {
    pending_hit_batches_.clear();
    pending_local_correction_mask_ = 0;
    pending_equipment_verifications_.clear();
    pending_inventory_commands_.clear();
  }
  player->set_actor_generation(packet.actor_generation);
  player->set_command_revision(packet.command_revision);
  // Old native snapshots cannot enter the new world before its spawn arrives.
  player->set_has_spawned(false);
  if (player_manager_.HasLocalPlayer() && player == &player_manager_.GetLocalPlayer()) {
    is_in_game_ = false;
    ResetMobStateReplay();
  }
  event_observer_.OnPlayerWorldUpdate(packet.player_id, packet.world_name, packet.start_point);
}

void GameClient::OnPlayerUnconscious(Packet p) {
  PlayerUnconsciousPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (!state.second) {
    SPDLOG_WARN("Failed to deserialize PlayerUnconsciousPacket");
    return;
  }
  if (packet.player_id == 0) {
    SPDLOG_WARN("PlayerUnconsciousPacket missing player id");
    return;
  }

  Player* player = FindActor(packet.player_id, packet.actor_generation);
  const bool deferred_local = player && player_manager_.HasLocalPlayer() && player == &player_manager_.GetLocalPlayer() && !is_in_game_;
  if (!player || (!player->has_spawned() && !deferred_local) || !player->accept_lifecycle_sequence(packet.state_sequence)) {
    return;
  }
  const auto applied = player->accept_components(kSyncHealth | kSyncLife, packet.state_sequence, true);
  if (applied & kSyncHealth) player->set_health(1);
  if (applied & kSyncLife) player->set_lifecycle_state(PLAYER_LIFE_UNCONSCIOUS, packet.command_revision);

  std::optional<std::uint64_t> attacker_id;
  if (packet.attacker_id.has_value()) {
    attacker_id = packet.attacker_id.value();
  }
  if (deferred_local) pending_local_correction_mask_ |= applied;
  event_observer_.OnPlayerUnconscious(packet.player_id, attacker_id, !deferred_local && (applied & kSyncLife) != 0);
  player->set_command_revision(packet.command_revision);
}

void GameClient::OnPlayerStandUp(Packet p) {
  PlayerStandUpPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (!state.second) {
    SPDLOG_WARN("Failed to deserialize PlayerStandUpPacket");
    return;
  }
  if (packet.player_id == 0) {
    SPDLOG_WARN("PlayerStandUpPacket missing player id");
    return;
  }

  Player* player = FindActor(packet.player_id, packet.actor_generation);
  const bool deferred_local = player && player_manager_.HasLocalPlayer() && player == &player_manager_.GetLocalPlayer() && !is_in_game_;
  if (!player || (!player->has_spawned() && !deferred_local) || !player->accept_lifecycle_sequence(packet.state_sequence)) {
    return;
  }
  const auto applied = player->accept_components(kSyncHealth | kSyncLife, packet.state_sequence, true);
  if ((applied & kSyncHealth) && player->health() <= 0) {
    player->set_health(1);
  }
  if (applied & kSyncLife) player->set_lifecycle_state(PLAYER_LIFE_ALIVE, packet.command_revision);

  if (deferred_local) pending_local_correction_mask_ |= applied;
  event_observer_.OnPlayerStandUp(packet.player_id, !deferred_local && (applied & kSyncLife) != 0);
  player->set_command_revision(packet.command_revision);
}

void GameClient::OnGameInfo(Packet p) {
  GameInfoPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize GameInfoPacket");
    return;
  }

  event_observer_.OnGameInfoReceived(packet.raw_game_time, packet.day_length_ms, packet.flags);
}

void GameClient::OnSkySettings(Packet p) {
  SkySettingsPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize SkySettingsPacket");
    return;
  }

  event_observer_.OnSkySettingsReceived(packet.flags, packet.weather_type, packet.rain_start_hour, packet.rain_start_min,
                                        packet.rain_stop_hour, packet.rain_stop_min, packet.wind_scale, packet.dont_rain != 0,
                                        packet.rain_weight, packet.render_lightning != 0);
}

void GameClient::OnPlayerPresence(Packet p) {
  PlayerPresencePacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize PlayerPresencePacket");
    return;
  }

  std::map<std::uint32_t, PlayerPresenceEntry> online;
  for (const auto& entry : packet.players) {
    if (entry.player_id == 0 || !std::isfinite(entry.position.x) || !std::isfinite(entry.position.y) || !std::isfinite(entry.position.z) ||
        !online.emplace(entry.player_id, entry).second)
      return;
  }
  online_players_ = std::move(online);

  for (const auto& entry : packet.players) {
    Player* player = nullptr;
    if (player_manager_.HasLocalPlayer() && player_manager_.GetLocalPlayer().id() == entry.player_id) {
      player = &player_manager_.GetLocalPlayer();
    } else {
      player = player_manager_.GetPlayer(entry.player_id);
    }

    if (!player) {
      continue;
    }

    if (player->ping() != entry.ping) {
      player->set_ping(entry.ping);
      event_observer_.OnPlayerPingUpdate(entry.player_id, entry.ping);
    }
  }
}

void GameClient::OnLeftGame(Packet p) {
  DisconnectionInfoPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize DisconnectionInfoPacket");
    return;
  }

  // PT_LEFT_GAME also means stream-out. Only the global presence snapshot
  // removes an online user; actor visibility cannot remove a roster entry.
  if (player_manager_.HasLocalPlayer() && player_manager_.GetLocalPlayer().id() == packet.disconnected_id) {
    auto& local_player = player_manager_.GetLocalPlayer();
    local_player.set_has_spawned(false);
    local_player.set_enabled(false);
    is_in_game_ = false;
    ResetMobStateReplay();
    event_observer_.OnPlayerLeft(packet.disconnected_id, local_player.name());
    return;
  }

  // Get player name before removing
  Player* player = player_manager_.GetPlayer(packet.disconnected_id);
  std::string player_name = player ? player->name() : "";

  event_observer_.OnPlayerLeft(packet.disconnected_id, player_name);

  // Remove from player manager
  player_manager_.RemovePlayer(packet.disconnected_id);
}

void GameClient::OnAdminAuth(Packet p) {
  AdminAuthPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize AdminAuthPacket");
    return;
  }

  admin_authenticated_ = packet.authenticated != 0;
  event_observer_.OnAdminAuthChanged(admin_authenticated_);
}

void GameClient::OnVoiceConfiguration(Packet p) {
  VoiceConfigurationPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (!state.second || packet.enabled > 1 || packet.transmit_enabled > 1) {
    SPDLOG_WARN("Failed to deserialize VoiceConfigurationPacket");
    return;
  }

  event_observer_.OnVoiceConfiguration(packet.enabled != 0, packet.transmit_enabled != 0, packet.range);
}

void GameClient::OnVoice(Packet p) {
  VoicePacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  if (!state.second) {
    SPDLOG_WARN("Failed to deserialize VoicePacket");
    return;
  }
  if (packet.player_id == 0 || packet.talkspurt_id == 0 || packet.spatial > 1 || packet.voice_data.empty() ||
      packet.voice_data.size() > kMaxVoiceFrameSize) {
    SPDLOG_WARN("Rejected invalid VoicePacket");
    return;
  }
  if (player_manager_.HasLocalPlayer() && player_manager_.GetLocalPlayer().id() == packet.player_id) {
    return;
  }

  event_observer_.OnVoiceFrame(packet.player_id, packet.talkspurt_id, packet.sequence, packet.spatial != 0,
                               packet.range, packet.voice_data);
}

void GameClient::OnLuaEvent(Packet p) {
  LuaEventPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  if (!state.second) {
    SPDLOG_ERROR("Failed to deserialize LuaEventPacket, error code: {}", static_cast<int>(state.first));
    return;
  }

  std::string payload(packet.payload.begin(), packet.payload.end());
  event_observer_.OnLuaEvent(packet.event_name, packet.source_element, payload);
}

void GameClient::OnTransportEvent(const Net::ConnectionEvent& event) {
  if (event.kind == Net::ConnectionEvent::Kind::Connected) {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    if (connection_state_ != ConnectionState::Connecting) {
      return;
    }
    SPDLOG_INFO("Server accepted connection");
    connection_state_ = ConnectionState::Connected;
    connection_lost_ = false;
    connection_error_.clear();
    connection_close_reason_ = Net::CloseReason::None;
    notify_connected_pending_ = true;
    return;
  }

  const auto error = GetConnectionCloseMessage(event);
  SPDLOG_WARN("Connection ended: {} (reason {}, transport: {})", error, static_cast<int>(event.reason), event.detail);
  {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    if (connection_state_ != ConnectionState::Connecting && connection_state_ != ConnectionState::Connected) {
      return;
    }
    const bool connecting = connection_state_ == ConnectionState::Connecting;
    const bool failed = event.kind == Net::ConnectionEvent::Kind::Failure;
    connection_state_ = connecting ? ConnectionState::Failed : ConnectionState::Disconnected;
    connection_lost_ = failed && !connecting;
    connection_close_reason_ = event.reason;
    connection_error_ = error;
    is_in_game_ = false;
    outgoing_state_sequence_ = 0;
    admin_authenticated_ = false;
    notify_connected_pending_ = false;
    notify_connection_failed_pending_ = connecting;
    notify_connection_lost_pending_ = !connecting && failed;
    notify_connection_closed_pending_ = !connecting && !failed;
  }

  resource_downloader_.StopDownload();
  if (player_manager_.HasLocalPlayer() || !player_manager_.GetAllPlayers().empty()) {
    event_observer_.OnPlayersClearing();
  }
  player_manager_.Clear();
  online_players_.clear();
  ResetMobStateReplay();
  worlds_.clear();
  event_observer_.OnAdminAuthChanged(false);
}

void GameClient::DispatchPendingConnectionNotifications() {
  bool notify_connected = false;
  bool notify_connection_failed = false;
  bool notify_connection_lost = false;
  bool notify_connection_closed = false;
  std::string connection_error;

  {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    notify_connected = notify_connected_pending_;
    notify_connection_failed = notify_connection_failed_pending_;
    notify_connection_lost = notify_connection_lost_pending_;
    notify_connection_closed = notify_connection_closed_pending_;
    connection_error = connection_error_;

    notify_connected_pending_ = false;
    notify_connection_failed_pending_ = false;
    notify_connection_lost_pending_ = false;
    notify_connection_closed_pending_ = false;
  }

  if (notify_connected) {
    event_observer_.OnConnected();
  }
  if (notify_connection_failed) {
    event_observer_.OnConnectionFailed(connection_error);
  }
  if (notify_connection_closed) {
    event_observer_.OnConnectionClosed(connection_error);
  }
  if (notify_connection_lost) {
    event_observer_.OnConnectionLost();
  }
}

}  // namespace gmp::client
