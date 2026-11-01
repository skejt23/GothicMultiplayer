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

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "character.h"

class NpcManager {
public:
  using NpcId = std::uint32_t;
  using Clock = std::chrono::steady_clock;
  using TimePoint = Clock::time_point;

  static constexpr std::size_t kMaxQueuedActions = 64;
  static constexpr std::uint32_t kDefaultActionTimeoutMs = 10000;
  static constexpr std::uint32_t kMaxActionTimeoutMs = 30000;
  static constexpr std::uint32_t kMaxActionLifetimeMs = 60000;

  struct NpcAction {
    std::uint32_t id{0};
    std::string animation;
    std::uint32_t timeout_ms{kDefaultActionTimeoutMs};
    TimePoint queued_at;
    std::optional<TimePoint> started_at;
  };

  struct Npc : Character {
    struct Summon {
      std::uint32_t caster_id;
      std::uint64_t caster_generation;
      std::uint32_t cast_id;
      TimePoint expires_at;
    };
    std::optional<Summon> summon;
    std::string owner_resource;
    std::uint32_t host_player_id{0};
    // Changes on host reassignment, baseline resets and queue head changes.
    // Zero is invalid, and an exhausted epoch is never wrapped or reused.
    std::uint32_t control_epoch{1};
    std::deque<NpcAction> actions;
    std::uint32_t next_action_id{1};
    std::uint32_t last_finished_action_id{0};
    // Persistent animation is presentation state, not queued execution.
    std::string animation;
    std::uint32_t animation_revision{1};
    std::uint64_t animation_started_at_ms{0};
  };

  NpcManager() = default;
  NpcManager(const NpcManager&) = delete;
  NpcManager& operator=(const NpcManager&) = delete;

  // NPC IDs never reuse an earlier value and never overlap the player slots.
  // Returns zero for invalid data or exhaustion of the signed Lua ID range.
  NpcId Create(const std::string& name, const std::string& instance, const std::string& world, std::uint32_t max_slots);
  bool Remove(NpcId npc_id);
  std::optional<std::reference_wrapper<Npc>> GetNpc(NpcId npc_id);
  std::optional<std::reference_wrapper<const Npc>> GetNpc(NpcId npc_id) const;

  // Iterate the copied IDs and reacquire each NPC before using it when callers
  // can run Lua callbacks that create or destroy NPCs.
  std::vector<NpcId> GetNpcIds() const;
  std::size_t GetNpcCount() const { return npcs_.size(); }

  // Host eligibility (connection, world and streaming) is checked by GameServer.
  // Losing the host preserves elapsed phase and never extends either deadline.
  bool SetHost(NpcId npc_id, std::uint32_t host_player_id, TimePoint now = Clock::now());
  bool BumpControlEpoch(NpcId npc_id, TimePoint now = Clock::now());
  // Empty clears the animation. Reapplying the same value is an idempotent
  // success; a change increments its revision without changing the queue.
  bool SetAnimation(NpcId npc_id, const std::string& animation);
  std::uint32_t QueueAnimation(NpcId npc_id, const std::string& animation,
                              std::uint32_t timeout_ms = kDefaultActionTimeoutMs, TimePoint now = Clock::now());

  // Only the current host may finish the current action for the current epoch.
  // The returned action has been removed; callers emit its completion event.
  std::optional<NpcAction> FinishAction(NpcId npc_id, std::uint32_t host_player_id, std::uint32_t control_epoch,
                                       std::uint32_t action_id, TimePoint now = Clock::now());
  std::optional<std::vector<NpcAction>> ClearActions(NpcId npc_id);
  std::vector<NpcAction> ExpireActions(NpcId npc_id, TimePoint now = Clock::now());
  bool IsActionFinished(NpcId npc_id, std::uint32_t action_id) const;

private:
  static bool IsExpired(const NpcAction& action, TimePoint now);
  static void ResetFrontStart(Npc& npc, TimePoint now);

  // Wider than the public ID so incrementing the final legal ID cannot wrap.
  std::uint64_t next_npc_id_{1};
  std::unordered_map<NpcId, Npc> npcs_;
};
