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

#include "npc_manager.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <utility>

namespace {
bool IsValidText(const std::string& text, std::size_t max_size, bool allow_empty = false) {
  return (allow_empty || !text.empty()) && text.size() <= max_size && text.find('\0') == std::string::npos;
}
}  // namespace

NpcManager::NpcId NpcManager::Create(const std::string& name, const std::string& instance, const std::string& world,
                                    std::uint32_t max_slots) {
  if (!IsValidText(name, 255, true) || !IsValidText(instance, 255) || !IsValidText(world, 255)) {
    return 0;
  }

  const auto candidate = std::max(next_npc_id_, static_cast<std::uint64_t>(max_slots) + 1);
  if (candidate > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())) {
    return 0;
  }

  Npc npc;
  npc.player_id = static_cast<NpcId>(candidate);
  npc.name = name;
  npc.instance = instance;
  npc.world = world;
  npc.state.nrot = glm::vec3{0.0f, 0.0f, 1.0f};
  npcs_.emplace(npc.player_id, std::move(npc));
  next_npc_id_ = candidate + 1;
  return static_cast<NpcId>(candidate);
}

bool NpcManager::Remove(NpcId npc_id) {
  return npcs_.erase(npc_id) != 0;
}

std::optional<std::reference_wrapper<NpcManager::Npc>> NpcManager::GetNpc(NpcId npc_id) {
  const auto it = npcs_.find(npc_id);
  if (it == npcs_.end()) {
    return std::nullopt;
  }
  return std::ref(it->second);
}

std::optional<std::reference_wrapper<const NpcManager::Npc>> NpcManager::GetNpc(NpcId npc_id) const {
  const auto it = npcs_.find(npc_id);
  if (it == npcs_.end()) {
    return std::nullopt;
  }
  return std::cref(it->second);
}

std::vector<NpcManager::NpcId> NpcManager::GetNpcIds() const {
  std::vector<NpcId> ids;
  ids.reserve(npcs_.size());
  for (const auto& [id, npc] : npcs_) {
    ids.push_back(id);
  }
  return ids;
}

void NpcManager::ResetFrontStart(Npc& npc, TimePoint now) {
  if (!npc.actions.empty() && npc.host_player_id != 0 && !npc.actions.front().started_at) {
    // Host migration resumes the original phase and deadline.
    npc.actions.front().started_at = now;
  }
}

bool NpcManager::SetHost(NpcId npc_id, std::uint32_t host_player_id, TimePoint now) {
  const auto npc_opt = GetNpc(npc_id);
  if (!npc_opt) {
    return false;
  }
  auto& npc = npc_opt->get();
  if (npc.host_player_id == host_player_id) {
    return true;
  }
  if (npc.control_epoch == std::numeric_limits<std::uint32_t>::max()) {
    return false;
  }
  ++npc.control_epoch;
  npc.host_player_id = host_player_id;
  // Incoming IDs belong to the reporting host's session, not to the NPC ID.
  npc.last_actor_action_id.reset();
  npc.last_cast_action_id.reset();
  npc.last_teleport_action_id.reset();
  npc.last_hit_action_id.reset();
  npc.hit_victims.clear();
  npc.approved_projectiles.clear();
  ResetFrontStart(npc, now);
  return true;
}

bool NpcManager::BumpControlEpoch(NpcId npc_id, TimePoint now) {
  const auto npc_opt = GetNpc(npc_id);
  if (!npc_opt || npc_opt->get().control_epoch == std::numeric_limits<std::uint32_t>::max()) {
    return false;
  }
  auto& npc = npc_opt->get();
  ++npc.control_epoch;
  ResetFrontStart(npc, now);
  return true;
}

bool NpcManager::SetAnimation(NpcId npc_id, const std::string& animation) {
  const auto npc_opt = GetNpc(npc_id);
  if (!npc_opt || !IsValidText(animation, kMaxPlayerAnimationNameLength, true)) {
    return false;
  }
  auto& npc = npc_opt->get();
  if (npc.animation == animation) {
    return true;
  }
  if (npc.animation_revision == std::numeric_limits<std::uint32_t>::max()) {
    return false;
  }
  npc.animation = animation;
  npc.animation_started_at_ms = ActorClockMilliseconds();
  ++npc.animation_revision;
  return true;
}

std::uint32_t NpcManager::QueueAnimation(NpcId npc_id, const std::string& animation, std::uint32_t timeout_ms, TimePoint now) {
  const auto npc_opt = GetNpc(npc_id);
  if (!npc_opt || !IsValidText(animation, kMaxPlayerAnimationNameLength) || timeout_ms == 0 || timeout_ms > kMaxActionTimeoutMs) {
    return 0;
  }
  auto& npc = npc_opt->get();
  if (npc.actions.size() >= kMaxQueuedActions || npc.next_action_id == std::numeric_limits<std::uint32_t>::max() ||
      npc.control_epoch == std::numeric_limits<std::uint32_t>::max()) {
    return 0;
  }

  const bool was_empty = npc.actions.empty();
  const auto action_id = npc.next_action_id;
  npc.actions.push_back(NpcAction{action_id, animation, timeout_ms, now, std::nullopt});
  ++npc.next_action_id;
  if (was_empty) {
    ++npc.control_epoch;
    ResetFrontStart(npc, now);
  }
  return action_id;
}

bool NpcManager::IsExpired(const NpcAction& action, TimePoint now) {
  return now - action.queued_at >= std::chrono::milliseconds{kMaxActionLifetimeMs} ||
         (action.started_at && now - *action.started_at >= std::chrono::milliseconds{action.timeout_ms});
}

std::optional<NpcManager::NpcAction> NpcManager::FinishAction(NpcId npc_id, std::uint32_t host_player_id,
                                                           std::uint32_t control_epoch, std::uint32_t action_id, TimePoint now) {
  const auto npc_opt = GetNpc(npc_id);
  if (!npc_opt) {
    return std::nullopt;
  }
  auto& npc = npc_opt->get();
  if (host_player_id == 0 || host_player_id != npc.host_player_id || control_epoch != npc.control_epoch || npc.actions.empty() ||
      action_id != npc.actions.front().id || !npc.actions.front().started_at || IsExpired(npc.actions.front(), now) ||
      npc.control_epoch == std::numeric_limits<std::uint32_t>::max()) {
    return std::nullopt;
  }

  NpcAction action = std::move(npc.actions.front());
  npc.actions.pop_front();
  npc.last_finished_action_id = action.id;
  ++npc.control_epoch;
  ResetFrontStart(npc, now);
  return action;
}

std::optional<std::vector<NpcManager::NpcAction>> NpcManager::ClearActions(NpcId npc_id) {
  const auto npc_opt = GetNpc(npc_id);
  if (!npc_opt || npc_opt->get().control_epoch == std::numeric_limits<std::uint32_t>::max()) {
    return std::nullopt;
  }
  auto& npc = npc_opt->get();
  std::vector<NpcAction> cleared(std::make_move_iterator(npc.actions.begin()), std::make_move_iterator(npc.actions.end()));
  if (!cleared.empty()) {
    npc.last_finished_action_id = cleared.back().id;
  }
  npc.actions.clear();
  ++npc.control_epoch;
  return cleared;
}

std::vector<NpcManager::NpcAction> NpcManager::ExpireActions(NpcId npc_id, TimePoint now) {
  const auto npc_opt = GetNpc(npc_id);
  if (!npc_opt || npc_opt->get().actions.empty()) {
    return {};
  }
  auto& npc = npc_opt->get();
  const auto previous_front_id = npc.actions.front().id;
  const bool front_expired = IsExpired(npc.actions.front(), now);
  if (front_expired && npc.control_epoch == std::numeric_limits<std::uint32_t>::max()) {
    return {};
  }

  std::vector<NpcAction> expired;
  for (auto it = npc.actions.begin(); it != npc.actions.end();) {
    if (IsExpired(*it, now)) {
      npc.last_finished_action_id = it->id;
      expired.push_back(std::move(*it));
      it = npc.actions.erase(it);
    } else {
      ++it;
    }
  }
  if (npc.actions.empty() || npc.actions.front().id != previous_front_id) {
    ++npc.control_epoch;
    ResetFrontStart(npc, now);
  }
  return expired;
}

bool NpcManager::IsActionFinished(NpcId npc_id, std::uint32_t action_id) const {
  const auto npc_opt = GetNpc(npc_id);
  if (!npc_opt || action_id == 0 || action_id >= npc_opt->get().next_action_id) {
    return false;
  }
  const auto& actions = npc_opt->get().actions;
  return std::none_of(actions.begin(), actions.end(), [action_id](const NpcAction& action) { return action.id == action_id; });
}
