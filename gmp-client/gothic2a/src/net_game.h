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

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "CSyncFuncs.h"
#include "HooksManager.h"
#include "VoiceCapture.h"
#include "VoicePlayback.h"
#include "ZenGin/zGothicAPI.h"
#include "client_resources/client_resource_runtime.h"
#include "content/content_transition_manager.h"
#include "event_observer.hpp"
#include "game_client.hpp"
#include "gothic2a_player.hpp"
#include "gothic_task_scheduler.h"

struct MD5Sum {
  BYTE data[16];
};

union STime {
  std::uint32_t time;
  struct {
    unsigned short day;
    unsigned char hour, min;
  };
};

class NetGame : public CSyncFuncs, public gmp::client::EventObserver {
public:
  struct ConnectionProgressDisplay {
    bool visible{false};
    bool failed{false};
    bool failure_expired{false};
    int percent{0};
    std::string message;
    std::string banner;
  };

  void HandleNetwork();
  bool IsConnected();
  bool Connect(std::string_view full_address);
  void JoinGame();
  std::uint32_t SendDropItem(std::int32_t instance, std::int32_t amount, const glm::vec3& position, const glm::vec3& rotation, bool physics_enabled);
  std::uint32_t SendTakeItem(std::int32_t instance, std::int32_t amount, std::optional<std::uint32_t> item_ground_id);
  void SendAnimationAction(const std::string& name, bool started, std::uint64_t source_time_ms);
  void SendShotPresentation(std::uint8_t weapon_mode, const glm::vec3& origin, const glm::vec3& heading,
                           std::optional<std::uint32_t> target_id, std::uint32_t actor_id = 0);
  void SendItemUsePresentation(std::int32_t instance, const std::string& scheme, std::int32_t quantity,
                               std::int32_t health_delta, std::int32_t mana_delta, std::uint32_t actor_id = 0);
  void SendMobState(const std::string& name, const glm::vec3& position, std::int32_t target_state);
  bool CanReportActorDamage(oCNpc* attacker);
  void SendNativeTeleport(oCNpc* npc, const std::string& destination);
  void OnNativeTransformation(oCNpc* previous, oCNpc* current, std::uint16_t spell, bool entering);
  void SetNativeTransformationCast(std::uint32_t cast_id) { native_transformation_cast_ = cast_id; }
  bool HasNativeTransformation() const { return native_transformation_spell_ != 0; }
  bool ClearNativeTransformation();
  void SendActorHit(oCNpc* attacker, std::uint32_t victim_id, std::int32_t damage, std::uint32_t damage_type, bool dont_kill,
                    bool finishing_move = false);
  bool ReportFinishingMove(oCNpc* attacker, oCNpc* victim, bool send_report = true);
  void SendPlayerWorldEnter(const std::string& world_name);
  void SendCastSpell(oCNpc* Target, short SpellId, std::int32_t item_instance = 0, std::uint32_t caster_id = 0);
  void SendMessage(const char* msg);
  void SendPlayerHit(std::uint32_t victim_id, std::int32_t damage, std::uint32_t damage_type, bool dont_kill);
  void SendPlayerUnconscious(std::optional<std::uint32_t> attacker_id);
  void SendPlayerStandUp();
  void SendPlayerDeath(std::optional<std::uint32_t> killer_id);
  void UpdatePlayerStats(int anim);
  void SyncGameTime();
  void Disconnect();
  bool ConsumeBaseWorldReloadRequest();
  bool FinalizeDownloadedContent(std::string& error_message);
  ConnectionProgressDisplay GetConnectionProgressDisplay() const;
  void ClearConnectionProgressDisplay();
  void RestoreHealth();
  void SetDayLengthMs(float day_length_ms);
  float GetDayLengthMs() const;

  bool IsVoiceChatAvailable() const;
  bool IsVoiceChatEnabled() const;
  void SetVoiceChatScriptEnabled(bool enabled);
  float GetVoiceChatRange() const;
  bool SetVoiceChatRange(float range);
  float GetVoiceChatOutputVolume() const;
  void SetVoiceChatOutputVolume(float volume);
  int GetVoiceChatPushToTalkKey() const;
  bool SetVoiceChatPushToTalkKey(int key);
  std::vector<std::string> GetVoiceChatInputDevices() const;
  std::string GetVoiceChatInputDevice() const;
  bool SetVoiceChatInputDevice(const std::string& device_name);
  std::vector<std::string> GetVoiceChatOutputDevices() const;
  std::string GetVoiceChatOutputDevice() const;
  bool SetVoiceChatOutputDevice(const std::string& device_name);
  const std::string& GetVoiceChatChannel() const;
  bool SetVoiceChatChannel(const std::string& channel);
  bool IsVoiceChatTransmitting() const;
  bool IsPlayerVoiceTalking(std::uint64_t player_id) const;
  bool IsPlayerVoiceMuted(std::uint64_t player_id) const;
  bool SetPlayerVoiceMuted(std::uint64_t player_id, bool muted);
  float GetPlayerVoiceVolume(std::uint64_t player_id) const;
  bool SetPlayerVoiceVolume(std::uint64_t player_id, float volume);

  // Task scheduler hook - called from render hook
  static void __stdcall ProcessTaskScheduler();

  void Shutdown();

  static NetGame& Instance() {
    static NetGame instance;
    return instance;
  }

  std::vector<Gothic2APlayer*> players;
  int HeroLastHp;
  zSTRING map;
  bool IsInGame{false};
  short mp_restore{0};
  int ForceHideMap{0};
  bool IsReadyToJoin{false};
  std::unique_ptr<gmp::GothicTaskScheduler> task_scheduler;
  std::unique_ptr<gmp::client::GameClient> game_client;
  std::unique_ptr<ClientResourceRuntime> resource_runtime;
  std::unique_ptr<gmp::gothic::ContentTransitionManager> content_transition_manager;

  // EventObserver interface implementation
  void OnConnectionStarted() override;
  void OnConnected() override;
  void OnConnectionFailed(const std::string& error) override;
  void OnDisconnected() override;
  void OnConnectionLost() override;
  void OnConnectionClosed(const std::string& reason) override;
  void OnAdminAuthChanged(bool authenticated) override;
  void OnResourceDownloadPhase(const std::string& phase) override;
  void OnResourceDownloadProgress(const std::string& resource_name, std::uint64_t downloaded_bytes, std::uint64_t total_bytes) override;
  void OnResourceDownloadFailed(const std::string& reason) override;
  void OnResourcesReady() override;
  void OnMapChange(const std::string& map_name) override;
  void OnGameInfoReceived(std::uint32_t raw_game_time, float day_length_ms, std::uint8_t flags) override;
  void OnSkySettingsReceived(std::uint8_t flags, std::int32_t weather_type, std::int16_t rain_start_hour, std::int16_t rain_start_min,
                             std::int16_t rain_stop_hour, std::int16_t rain_stop_min, float wind_scale, bool dont_rain, float rain_weight,
                             bool render_lightning) override;
  void OnVoiceConfiguration(bool enabled, bool transmit_enabled, std::uint32_t range) override;
  void OnVoiceFrame(std::uint32_t player_id, std::uint32_t talkspurt_id, std::uint32_t sequence,
                    bool spatial, std::uint32_t range, const std::vector<std::uint8_t>& encoded_data) override;
  void OnLocalPlayerJoined(gmp::client::Player& player) override;
  void OnLocalPlayerSpawned(gmp::client::Player& player) override;
  void OnPlayerJoined(gmp::client::Player& player) override;
  void OnPlayerSpawned(gmp::client::Player& player) override;
  void OnPlayersClearing() override;
  void OnPlayerSpawnSnapshotApplied(gmp::client::Player& player, bool new_spawn) override;
  void OnNpcControl(const NpcControlPacket& control) override;
  void OnNpcAnimation(const NpcAnimationPacket& animation) override;
  void OnPlayerLeft(std::uint64_t player_id, const std::string& player_name) override;
  void OnPlayerStateUpdate(std::uint64_t player_id, const PlayerState& state) override;
  void OnPlayerStateUpdate(std::uint64_t player_id, const PlayerState& state, bool authoritative_correction) override;
  void OnPlayerPositionUpdate(std::uint64_t player_id, float x, float y, float z) override;
  void OnPlayerNameUpdate(std::uint64_t player_id, const std::string& name) override;
  void OnPlayerInstanceUpdate(std::uint64_t player_id, const std::string& instance) override;
  void ChangePlayerInstance(std::uint64_t player_id, const std::string& instance, bool reset_defaults);
  PlayerSpawnPacket CaptureInstanceDefaults(Gothic2APlayer& actor);
  void OnPlayerColorUpdate(std::uint64_t player_id, std::uint8_t r, std::uint8_t g, std::uint8_t b) override;
  void OnPlayerSkillWeaponUpdate(std::uint64_t player_id, std::int32_t skill_id, std::int32_t percentage) override;
  void OnPlayerTalentUpdate(std::uint64_t player_id, std::int32_t talent_id, std::int32_t talent_value) override;
  void OnPlayerVisualUpdate(std::uint64_t player_id, const std::string& body_model, std::int16_t body_texture, const std::string& head_model,
                            std::int16_t head_texture, std::int16_t teeth_texture, std::int16_t skin_color) override;
  void OnPlayerFatnessUpdate(std::uint64_t player_id, float fatness) override;
  void OnPlayerScaleUpdate(std::uint64_t player_id, const glm::vec3& scale) override;
  void OnPlayerOverlayUpdate(std::uint64_t player_id, const std::string& overlay, bool apply) override;
  void OnPlayerAnimationPlay(std::uint64_t player_id, const std::string& animation) override;
  void OnPlayerAnimationStop(std::uint64_t player_id, const std::string& animation) override;
  void OnPlayerFaceAnimationPlay(std::uint64_t player_id, const std::string& animation) override;
  void OnPlayerFaceAnimationStop(std::uint64_t player_id, const std::string& animation) override;
  void OnPlayerGesticulation(std::uint64_t player_id) override;
  void OnPlayerAttributeUpdate(std::uint64_t player_id, PlayerAttributeId attribute_id, std::int32_t value) override;
  void OnPlayerWorldUpdate(std::uint64_t player_id, const std::string& world_name, const std::string& start_point) override;
  void OnPlayerDied(std::uint64_t player_id) override;
  void OnPlayerDied(std::uint64_t player_id, bool apply_state) override;
  void OnPlayerRespawned(std::uint64_t player_id) override;
  void OnPlayerRespawned(std::uint64_t player_id, bool apply_state) override;
  void OnPlayerUnconscious(std::uint64_t player_id, std::optional<std::uint64_t> attacker_id) override;
  void OnPlayerUnconscious(std::uint64_t player_id, std::optional<std::uint64_t> attacker_id, bool apply_state) override;
  void OnPlayerStandUp(std::uint64_t player_id) override;
  void OnPlayerStandUp(std::uint64_t player_id, bool apply_state) override;
  void OnPlayerPingUpdate(std::uint64_t player_id, std::int32_t ping) override;
  void OnItemDropped(std::uint64_t player_id, const std::string& item_instance, std::int32_t amount) override;
  void OnActorAction(const ActorActionPacket& action) override;
  void OnActorDamage(const ActorDamagePacket& damage) override;
  void OnPlayerSummon(const PlayerSummonPacket& packet) override;
  void OnPlayerTeleport(const PlayerTeleportPacket& packet, std::uint16_t correction_mask) override;
  void OnPlayerTransformation(const PlayerTransformationPacket& packet, std::uint16_t correction_mask) override;
  void OnActorItemTransactionResult(std::uint32_t actor_id, std::uint32_t request_id, const std::string& item_instance, std::int32_t resulting_amount,
                                bool accepted, std::optional<std::uint32_t> item_ground_id) override;
  void OnActorInventorySnapshot(std::uint64_t actor_id, const std::vector<InventoryEntry>& inventory) override;
  void OnItemTaken(std::uint64_t player_id, const std::string& item_instance) override;
  void OnItemGroundCreate(std::uint32_t item_ground_id, const std::string& item_instance, std::int32_t amount, bool physics_enabled,
                          const glm::vec3& position, const glm::vec3& rotation) override;
  void OnItemGroundDestroy(std::uint32_t item_ground_id) override;
  void OnItemsGroundDestroy() override;
  void OnItemGiven(std::uint64_t player_id, const std::string& item_instance, std::int32_t amount) override;
  void OnItemEquipped(std::uint64_t player_id, const std::string& item_instance, std::int16_t slot_id) override;
  void OnItemEquipped(std::uint64_t player_id, const std::string& item_instance, std::int16_t slot_id,
                       std::uint32_t revision, bool apply_state) override;
  void OnItemUnequipped(std::uint64_t player_id, const std::string& item_instance) override;
  void OnItemUnequipped(std::uint64_t player_id, const std::string& item_instance, std::int16_t slot_id,
                         std::uint32_t revision, bool apply_state) override;
  void OnItemRemoved(std::uint64_t player_id, const std::string& item_instance, std::int32_t amount) override;
  void OnSpellCast(std::uint64_t caster_id, std::uint16_t spell_id) override;
  void OnSpellCast(const CastSpellPacket& packet) override;
  void OnSpellCast(std::uint64_t caster_id, std::uint16_t spell_id, std::uint64_t source_time_ms,
                    std::uint64_t actor_generation, std::uint32_t control_epoch) override;
  void OnSpellCastOnTarget(std::uint64_t caster_id, std::uint64_t target_id, std::uint16_t spell_id) override;
  void OnSpellCastOnTarget(std::uint64_t caster_id, std::uint64_t target_id, std::uint16_t spell_id,
                            std::uint64_t source_time_ms, std::uint64_t actor_generation,
                            std::uint64_t target_generation, std::uint32_t control_epoch) override;
  void OnPlayerMessage(std::optional<std::uint64_t> sender_id, std::uint8_t r, std::uint8_t g, std::uint8_t b, const std::string& message) override;
  void OnLuaEvent(const std::string& event_name, std::uint32_t source_element, const std::string& payload) override;

private:
  NetGame();
  time_t last_mp_regen;

  struct GameTimeSnapshot {
    int day;
    int hour;
    int min;
  };

  struct PendingLocalSpawnPosition {
    std::uint64_t player_id;
    zVEC3 position;
    int remaining_frames;
  };

  struct NpcActionRuntime {
    NpcControlPacket control;
    std::chrono::steady_clock::time_point deadline{};
    oCNpc* npc{nullptr};
    zCModel* model{nullptr};
    zCModelAni* animation{nullptr};
    int animation_id{-1};
    bool finished{false};
    std::chrono::steady_clock::time_point expected_finish{};
    float last_progress{0.0f};
  };

  void ProcessNpcActions();
  void StopNpcActionAnimation(NpcActionRuntime& action);
  void FinishNpcAction(NpcActionRuntime& action, bool success);
  void CancelNpcAction(std::uint64_t npc_id);
  void CancelAllNpcActions();
  void InterruptNpcAction(std::uint64_t npc_id);
  std::unordered_map<std::uint32_t, NpcActionRuntime> npc_actions_;

  struct NpcAnimationRuntime {
    NpcAnimationPacket state;
    oCNpc* npc{nullptr};
    zCModel* model{nullptr};
    zCModelAni* animation{nullptr};
    int animation_id{-1};
    bool applied{false};
    bool looping{false};
  };
  void StopNpcPersistentAnimation(std::uint64_t npc_id);
  void ProcessNpcPersistentAnimation(Gothic2APlayer& actor);
  std::unordered_map<std::uint32_t, NpcAnimationRuntime> npc_animations_;
  struct PendingInventoryMutation { std::int32_t instance; std::int32_t delta; };
  std::unordered_map<std::uint32_t, PendingInventoryMutation> pending_inventory_mutations_;
  std::uint64_t pending_inventory_generation_{0};
  std::uint64_t stand_up_report_generation_{0};
  std::uint64_t stand_up_report_after_ms_{0};
  std::unordered_map<std::uint64_t, Gothic2APlayer*> actors_by_id_;
  std::unordered_map<oCNpc*, Gothic2APlayer*> actors_by_npc_;
  std::unordered_map<oCNpc*, std::unordered_set<oCItem*>> proxy_presentation_items_;
  void PruneProxyPresentationItems(Gothic2APlayer& actor, const PlayerState& state);
  void ReleaseProxyPresentationItems(oCNpc* npc);

  Gothic2APlayer* GetPlayerById(std::uint64_t player_id);
  void SpawnRemotePlayer(gmp::client::Player& new_player);
  void ApplyActorState(Gothic2APlayer& actor, const PlayerState& state, bool snapshot, bool presentation_due = false,
                       bool weapon_mode_command = false);
  void ApplyServerStats(Gothic2APlayer& actor);
  bool IsActorInstancePending(Gothic2APlayer& actor) const;
  std::uint16_t native_transformation_spell_{0};
  std::uint32_t native_transformation_cast_{0};
  std::uint64_t native_transformation_generation_{0};
  glm::vec3 native_transformation_origin_{0.0f};
  void ApplyServerInventory(Gothic2APlayer& actor, bool restore_missing = false);
  void ApplyPlayerLifeState(std::uint64_t player_id, std::uint8_t life_state, std::optional<std::uint64_t> actor_id, bool trigger_event);
  void ApplyPendingLocalSpawnPosition();
  void UpdateClientEventState();
  void ActivateDownloadedContent(std::uint64_t generation);
  void ShowConnectionProgressError(const std::string& message);
  void StartVoiceChat();
  void StopVoiceChat();
  void UpdateVoiceChat();
  void SetVoiceTransmitting(bool transmitting);
  void MarkPlayerVoiceActive(std::uint32_t player_id);
  void EndPlayerVoiceActivity(std::uint32_t player_id);
  void UpdatePlayerVoiceActivity();
  void ClearPlayerVoiceActivity();
  void ResetVoiceChatScriptState();

  std::optional<GameTimeSnapshot> last_game_time_;
  std::optional<PendingLocalSpawnPosition> pending_local_spawn_position_;
  std::string last_world_name_;
  std::string server_requested_map_;
  float day_length_ms_{0.0f};
  std::uint64_t content_activation_generation_{0};
  bool base_world_reload_required_{false};
  bool connection_progress_visible_{false};
  bool connection_progress_failed_{false};
  int connection_progress_percent_{0};
  std::string connection_progress_message_;
  std::string connection_progress_banner_;
  std::vector<gmp::client::GameClient::ResourcePayload> pending_resource_payloads_;
  std::chrono::steady_clock::time_point connection_progress_error_until_{};
  gmp::voice::VoiceCapture voice_capture_;
  gmp::voice::VoicePlayback voice_playback_;
  bool server_voice_enabled_{false};
  bool server_voice_transmit_enabled_{false};
  float voice_range_{0.0f};
  bool script_voice_enabled_{true};
  std::optional<float> script_voice_range_;
  std::optional<int> script_voice_push_to_talk_key_;
  float script_voice_output_volume_{1.0f};
  std::string voice_channel_{"default"};
  std::unordered_set<std::uint32_t> muted_voice_players_;
  std::unordered_map<std::uint32_t, float> voice_player_volumes_;
  std::unordered_map<std::uint32_t, std::chrono::steady_clock::time_point> active_voice_players_;

public:
  std::optional<std::uint64_t> GetPlayerIdByNpc(oCNpc* npc);
  void ActorNpcChanged(Gothic2APlayer& actor, oCNpc* previous);
  void UnregisterActor(Gothic2APlayer& actor);
  void TrackProxyPresentationItem(oCNpc* npc, oCItem* item);
  Gothic2APlayer* FindEngineActor(std::uint64_t id) { return GetPlayerById(id); }
};
