/*
MIT License

Copyright (c) 2025 Gothic Multiplayer Team

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

#include "mcp_pipe_handler.h"

#include <d3d9.h>
#include <spdlog/spdlog.h>

#include <cmath>
#include <algorithm>
#include <cstdlib>

#include "ZenGin/zGothicAPI.h"
#include "renderer/d3d9/D3D9Renderer.h"
#include "config.h"
#include "net_game.h"
#include "Shared/lua_runtime/lua_value_codec.h"

using namespace Gothic_II_Addon;

namespace gmp {
namespace mcp {

// =============================================================================
// Singleton Instance
// =============================================================================

MCPPipeHandler& MCPPipeHandler::Instance() {
  static MCPPipeHandler instance;
  return instance;
}

MCPPipeHandler::~MCPPipeHandler() {
  Stop();
}

// =============================================================================
// Server Control
// =============================================================================

bool MCPPipeHandler::Start() {
  if (running_.load()) {
    SPDLOG_WARN("MCP Pipe Handler already running");
    return true;
  }

  test_rpc_enabled_ = std::getenv("GMP_MCP_TEST_RPC") && std::string(std::getenv("GMP_MCP_TEST_RPC")) == "1";
  if (const char* pipe = std::getenv("GMP_MCP_PIPE_NAME"); pipe && *pipe) {
    const std::string suffix(pipe);
    if (suffix.size() > 64 || !std::all_of(suffix.begin(), suffix.end(), [](unsigned char c) {
          return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
        })) return false;
    pipe_name_ = R"(\\.\pipe\)" + suffix;
  }
  SPDLOG_INFO("Starting MCP Pipe Handler on {}", pipe_name_);

  // Create stop event (manual-reset, initially non-signaled)
  stop_event_ = CreateEventA(nullptr, TRUE, FALSE, nullptr);
  if (stop_event_ == NULL) {
    SPDLOG_ERROR("Failed to create stop event: {}", GetLastError());
    return false;
  }

  stop_requested_.store(false);
  running_.store(true);

  // Register default handlers
  RegisterDefaultHandlers();

  // Start server thread
  server_thread_ = std::thread(&MCPPipeHandler::ServerThread, this);

  return true;
}

bool MCPPipeHandler::IsRequested() {
  const char* enabled = std::getenv("GMP_MCP_ENABLE");
  return Config::Instance().IsMCPPipeEnabled() || (enabled && std::string(enabled) == "1");
}

void MCPPipeHandler::Stop() {
  if (!running_.load()) {
    return;
  }

  SPDLOG_INFO("Stopping MCP Pipe Handler");

  stop_requested_.store(true);
  {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    for (auto& pending : pending_requests_) pending->state.store(PendingRequest::Cancelled);
    pending_requests_.clear();
  }

  // Signal the stop event to unblock any waiting operations in the server thread
  if (stop_event_ != NULL) {
    SetEvent(stop_event_);
  }

  // Wait for server thread to exit - it will handle its own cleanup
  if (server_thread_.joinable()) {
    CancelSynchronousIo(server_thread_.native_handle());
    SPDLOG_DEBUG("Waiting for server thread to exit...");
    server_thread_.join();
    SPDLOG_DEBUG("Server thread exited");
  }

  // Cleanup stop event
  if (stop_event_ != NULL) {
    CloseHandle(stop_event_);
    stop_event_ = NULL;
  }

  running_.store(false);
  SPDLOG_INFO("MCP Pipe Handler stopped");
}

// =============================================================================
// Handler Registration
// =============================================================================

void MCPPipeHandler::RegisterHandler(const std::string& method, CommandHandler handler) {
  std::lock_guard<std::mutex> lock(handlers_mutex_);
  handlers_[method] = std::move(handler);
  SPDLOG_DEBUG("Registered MCP handler: {}", method);
}

void MCPPipeHandler::UnregisterHandler(const std::string& method) {
  std::lock_guard<std::mutex> lock(handlers_mutex_);
  handlers_.erase(method);
}

// =============================================================================
// Default Handlers
// =============================================================================

namespace {

oCNpc* GetLocalNpc() {
  return ::player;
}

}  // namespace

void MCPPipeHandler::RegisterDefaultHandlers() {
  // Status - enhanced with game state tracking
  RegisterHandler("getStatus", [this](const json& /*params*/) {
    auto state = GetGameState();
    bool has_player = GetLocalNpc() != nullptr;

    json result = {{"connected", true},       {"gameRunning", ogame != nullptr},       {"inWorld", ogame && ogame->GetGameWorld() != nullptr},
                   {"hasPlayer", has_player}, {"gameState", GameStateToString(state)}, {"isReady", IsGameReady()},
                   {"version", "1.0.0"}, {"pid", GetCurrentProcessId()}, {"testRpcEnabled", test_rpc_enabled_}};

    // Add time since ready if applicable
    auto ms_since_ready = GetTimeSinceReady();
    if (ms_since_ready >= 0) {
      result["msSinceReady"] = ms_since_ready;
    }

    // Add world name if in world
    if (ogame && ogame->GetGameWorld()) {
      result["worldName"] = ogame->GetGameWorld()->GetWorldFilename().ToChar();
    }

    return result;
  });

  RegisterHandler("executeLua", [this](const json& params) {
    if (!test_rpc_enabled_) return json{{"success", false}, {"error", "Test RPC requires GMP_MCP_TEST_RPC=1"}};
    const std::string code = params.value("code", std::string{});
    if (code.empty() || code.size() > 32768) return json{{"success", false}, {"error", "Lua source must contain 1..32768 bytes"}};
    auto* runtime = NetGame::Instance().resource_runtime.get();
    if (!runtime) return json{{"success", false}, {"error", "Client Lua runtime is unavailable"}};
    auto& lua = runtime->GetLuaState();
    auto result = lua.safe_script(code, sol::script_pass_on_error);
    if (!result.valid()) {
      const sol::error error = result;
      return json{{"success", false}, {"error", error.what()}};
    }
    json values = json::array();
    for (int index = 0; index < result.return_count(); ++index) {
      json value;
      std::string error;
      if (!gmp::lua::EncodeLuaObject(lua, result.get<sol::object>(index), value, error))
        return json{{"success", false}, {"error", error}};
      values.push_back(std::move(value));
    }
    return json{{"success", true}, {"values", std::move(values)}};
  });
  RegisterHandler("connect", [this](const json& params) {
    if (!test_rpc_enabled_) return json{{"success", false}, {"error", "Test RPC requires GMP_MCP_TEST_RPC=1"}};
    const auto address = params.value("address", std::string{});
    if (address.empty() || address.size() > 256) return json{{"success", false}, {"error", "Invalid address"}};
    return json{{"success", NetGame::Instance().Connect(address)}};
  });
  RegisterHandler("getActorSyncInfo", [this](const json& params) {
    if (!test_rpc_enabled_) return json{{"success", false}, {"error", "Test RPC requires GMP_MCP_TEST_RPC=1"}};
    auto& net = NetGame::Instance();
    auto id = params.value("actorId", std::uint64_t{0});
    if (!id && net.game_client && net.game_client->player_manager().HasLocalPlayer())
      id = net.game_client->player_manager().GetLocalPlayer().id();
    auto* actor = net.FindEngineActor(id);
    if (!actor || !actor->GetNpc()) return json{{"success", false}, {"error", "Actor has no native instance"}};
    auto* npc = actor->GetNpc();
    const auto& cached = actor->base_player();
    const auto state = cached.state();
    const auto vector = [](const glm::vec3& value) { return json::array({value.x, value.y, value.z}); };
    const auto native_vector = [](const zVEC3& value) { return json::array({value[VX], value[VY], value[VZ]}); };
    const auto item = [](oCItem* value) -> json {
      if (!value) return nullptr;
      return {{"identity", static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(value))},
              {"instanceId", value->GetInstance()}, {"instance", std::string(value->GetInstanceName().ToChar())},
              {"amount", value->amount}, {"active", value->HasFlag(ITM_FLAG_ACTIVE) != 0}};
    };
    auto slot = [&](const char* name) { zSTRING native_name(name); return item(npc->GetSlotItem(native_name)); };
    json cache = {{"generation", cached.actor_generation()}, {"commandRevision", cached.command_revision()},
        {"sequence", cached.state_sequence()}, {"sourceTimeMs", cached.source_time_ms()},
        {"position", vector(state.position)}, {"heading", vector(state.nrot)}, {"health", state.health_points},
        {"mana", state.mana_points}, {"maxHealth", cached.max_health()}, {"maxMana", cached.max_mana()},
        {"strength", cached.strength()}, {"dexterity", cached.dexterity()}, {"weaponMode", state.weapon_mode},
        {"activeSpell", state.active_spell_instance}, {"activeSpellNr", state.active_spell_nr},
        {"bodyState", state.body_state}, {"walkMode", state.walk_mode}, {"lookYaw", state.look_yaw},
        {"lookPitch", state.look_pitch}, {"invulnerable", state.invulnerable}, {"lifeState", state.life_state},
        {"leftHand", state.left_hand_item_instance}, {"rightHand", state.right_hand_item_instance},
        {"armor", state.equipped_armor_instance}, {"helmet", state.equipped_helmet_instance},
        {"shield", state.equipped_shield_instance}, {"amulet", state.equipped_amulet_instance},
        {"belt", state.equipped_belt_instance}, {"ringLeft", state.equipped_ring_left_instance},
        {"ringRight", state.equipped_ring_right_instance}, {"melee", state.melee_weapon_instance},
        {"ranged", state.ranged_weapon_instance}, {"overlays", cached.overlays()},
        {"npcEpoch", cached.npc_control_epoch()}, {"npcHost", cached.npc_host_player_id()},
        {"npcAction", cached.npc_action_id()}, {"npcAnimationRevision", cached.npc_animation_revision()}};
    json native = {{"instance", std::string(npc->GetInstanceName().ToChar())},
        {"identity", static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(npc))},
        {"isGlobalHero", npc == ::player}, {"position", native_vector(npc->GetPositionWorld())},
        {"heading", native_vector(npc->GetAtVectorWorld())}, {"health", npc->GetAttribute(NPC_ATR_HITPOINTS)},
        {"maxHealth", npc->GetAttribute(NPC_ATR_HITPOINTSMAX)}, {"mana", npc->GetAttribute(NPC_ATR_MANA)},
        {"maxMana", npc->GetAttribute(NPC_ATR_MANAMAX)}, {"strength", npc->GetAttribute(NPC_ATR_STRENGTH)},
        {"dexterity", npc->GetAttribute(NPC_ATR_DEXTERITY)}, {"weaponMode", npc->GetWeaponMode()},
        {"bodyState", npc->GetBodyState()}, {"dead", npc->IsDead() != 0}, {"unconscious", npc->IsUnconscious() != 0},
        {"invulnerable", (npc->variousFlags & NPC_FLAG_IMMORTAL) != 0}, {"activeSpellNr", npc->GetActiveSpellNr()},
        {"leftHand", item(dynamic_cast<oCItem*>(npc->GetLeftHand()))},
        {"rightHand", item(dynamic_cast<oCItem*>(npc->GetRightHand()))},
        {"armor", item(npc->GetEquippedArmor())}, {"helmet", slot(NPC_NODE_HELMET)}, {"shield", slot(NPC_NODE_SHIELD)},
        {"melee", item(npc->GetEquippedMeleeWeapon())}, {"ranged", item(npc->GetEquippedRangedWeapon())}};
    if (auto* controller = npc->GetAnictrl()) {
      native["walkMode"] = controller->walkmode;
      native["lookYaw"] = controller->lookTargetx;
      native["lookPitch"] = controller->lookTargety;
    }
    if (auto* book = npc->GetSpellBook()) native["selectedSpellItem"] = item(book->GetSpellItem(book->GetSelectedSpell()));
    json inventory = json::array();
    std::size_t count = 0;
    for (auto* entry = npc->inventory2.inventory.GetNextInList(); entry; entry = entry->GetNextInList()) {
      if (auto* value = entry->GetData()) {
        ++count;
        if (inventory.size() < 256) inventory.push_back(item(value));
      }
    }
    native["inventory"] = std::move(inventory);
    native["inventoryCount"] = count;
    native["inventoryTruncated"] = count > 256;
    json animations = json::array();
    if (auto* model = npc->GetModel()) {
      for (int channel = 0; channel < std::clamp(model->numActiveAnis, 0, zMDL_MAX_ANIS_PARALLEL); ++channel) {
        if (auto* active = model->aniChannels[channel]; active && active->protoAni) {
          auto* animation = active->protoAni;
          animations.push_back({{"name", std::string(animation->GetAniName().ToChar())}, {"frame", active->actFrame},
              {"frames", static_cast<int>(animation->numFrames)}, {"fps", animation->fpsRate},
              {"direction", active->advanceDir}, {"fading", active->isFadingOut != 0}});
        }
      }
    }
    native["animations"] = std::move(animations);
    native["presentationTimeMs"] = actor->PresentationTime(ActorClockMilliseconds());
    return json{{"success", true}, {"actorId", id}, {"isNpc", cached.is_npc()}, {"isLocal", actor->IsLocalPlayer()},
                {"cache", std::move(cache)}, {"native", std::move(native)}};
  });
  RegisterHandler("disconnect", [this](const json& /*params*/) {
    if (!test_rpc_enabled_) return json{{"success", false}, {"error", "Test RPC requires GMP_MCP_TEST_RPC=1"}};
    NetGame::Instance().Disconnect();
    return json{{"success", true}};
  });

  // Wait for ready - blocks until game is ready or timeout
  RegisterHandler("waitForReady", [this](const json& params) {
    const int timeout_ms = std::clamp(params.value("timeoutMs", 30000), 1, 120000);
    const int poll_interval_ms = std::clamp(params.value("pollIntervalMs", 100), 10, 250);
    constexpr int SETTLE_DELAY_MS = 1500;  // Fixed delay for loading screen fade + world init

    auto start = std::chrono::steady_clock::now();

    while (!IsGameReady() && !stop_requested_.load()) {
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

      if (elapsed >= timeout_ms) {
        return json{{"success", false},
                    {"error", "Timeout waiting for game to be ready"},
                    {"gameState", GameStateToString(GetGameState())},
                    {"elapsedMs", elapsed}};
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
    }
    if (stop_requested_.load()) return json{{"success", false}, {"error", "MCP server is stopping"}};

    // Game reports ready, but add settle delay for:
    // - Loading screen fade-out completion
    // - World initialization/rendering
    // - Player full spawn/control
    SPDLOG_DEBUG("Game ready, waiting {}ms for settle...", SETTLE_DELAY_MS);
    std::this_thread::sleep_for(std::chrono::milliseconds(SETTLE_DELAY_MS));

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

    return json{{"success", true}, {"gameState", GameStateToString(GetGameState())}, {"elapsedMs", elapsed}};
  });

  // Player Info
  RegisterHandler("getPlayerInfo", [](const json& /*params*/) {
    auto* npc = GetLocalNpc();
    if (!npc) {
      return json{{"error", "No local player"}};
    }

    const zVEC3 pos = npc->GetPositionWorld();
    const zVEC3 forward = npc->GetAtVectorWorld();
    const float angle = std::atan2(forward[VX], forward[VZ]);

    std::string world_name;
    if (ogame && ogame->GetGameWorld()) {
      world_name = ogame->GetGameWorld()->GetWorldFilename().ToChar();
    }

    return json{{"name", npc->GetName().ToChar()},
                {"instance", npc->GetInstanceName().ToChar()},
                {"position", {{"x", pos[VX]}, {"y", pos[VY]}, {"z", pos[VZ]}}},
                {"angle", angle},
                {"health", npc->GetAttribute(NPC_ATR_HITPOINTS)},
                {"maxHealth", npc->GetAttribute(NPC_ATR_HITPOINTSMAX)},
                {"mana", npc->GetAttribute(NPC_ATR_MANA)},
                {"maxMana", npc->GetAttribute(NPC_ATR_MANAMAX)},
                {"strength", npc->GetAttribute(NPC_ATR_STRENGTH)},
                {"dexterity", npc->GetAttribute(NPC_ATR_DEXTERITY)},
                {"level", npc->level},
                {"experience", npc->experience_points},
                {"learnPoints", npc->learn_points},
                {"world", world_name}};
  });

  // Set Position
  RegisterHandler("setPlayerPosition", [](const json& params) {
    auto* npc = GetLocalNpc();
    if (!npc) {
      return json{{"error", "No local player"}};
    }

    float x = params.value("x", 0.0f);
    float y = params.value("y", 0.0f);
    float z = params.value("z", 0.0f);

    zVEC3 position{x, y, z};
    npc->SetPositionWorld(position);

    return json{{"success", true}, {"position", {{"x", x}, {"y", y}, {"z", z}}}};
  });

  // Open Inventory
  RegisterHandler("openInventory", [](const json& /*params*/) {
    auto* npc = GetLocalNpc();
    if (!npc) {
      return json{{"error", "No local player"}};
    }

    npc->OpenInventory(1);  // 1 = Open

    return json{{"success", true}};
  });

  // Set Camera Angle (Orbit)
  RegisterHandler("setCameraAngle", [](const json& params) {
    auto* cam = zCAICamera::GetCurrent();
    if (!cam) {
      return json{{"error", "No active AI camera"}};
    }

    if (params.contains("yaw")) {
      cam->bestRotY = params["yaw"].get<float>();
    }
    if (params.contains("pitch")) {
      cam->bestRotX = params["pitch"].get<float>();
    }

    return json{{"success", true}};
  });

  // Get Pixel Color (for visual verification)
  RegisterHandler("getPixelColor", [](const json& params) {
    if (!params.contains("x") || !params.contains("y")) {
      return json{{"error", "Missing x or y parameter"}};
    }

    int x = params["x"].get<int>();
    int y = params["y"].get<int>();

    auto* renderer = dynamic_cast<zCRnd_D3D_DX9*>(zrenderer);
    if (!renderer) {
      return json{{"error", "D3D9 renderer not active"}};
    }

    auto* device = renderer->GetDevice();
    if (!device) {
      return json{{"error", "No D3D9 device"}};
    }

    // Get back buffer
    IDirect3DSurface9* backbuffer = nullptr;
    HRESULT hr = device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backbuffer);
    if (FAILED(hr) || !backbuffer) {
      return json{{"error", "Failed to get back buffer"}};
    }

    // Get surface description
    D3DSURFACE_DESC desc;
    hr = backbuffer->GetDesc(&desc);
    if (FAILED(hr)) {
      backbuffer->Release();
      return json{{"error", "Failed to get surface desc"}};
    }

    if (x < 0 || x >= static_cast<int>(desc.Width) || y < 0 || y >= static_cast<int>(desc.Height)) {
      backbuffer->Release();
      return json{{"error", "Coordinates out of bounds"}, {"width", desc.Width}, {"height", desc.Height}};
    }

    IDirect3DSurface9* staging = nullptr;
    hr = device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &staging, nullptr);
    if (FAILED(hr) || !staging) {
      backbuffer->Release();
      return json{{"error", "Failed to create staging surface"}};
    }

    hr = device->GetRenderTargetData(backbuffer, staging);
    backbuffer->Release();
    if (FAILED(hr)) {
      staging->Release();
      return json{{"error", "Failed to copy render target data"}};
    }

    D3DLOCKED_RECT locked;
    RECT rect = {x, y, x + 1, y + 1};
    hr = staging->LockRect(&locked, &rect, D3DLOCK_READONLY);
    if (FAILED(hr)) {
      staging->Release();
      return json{{"error", "Failed to lock staging surface"}};
    }

    uint32_t pixel = *reinterpret_cast<uint32_t*>(locked.pBits);
    staging->UnlockRect();
    staging->Release();

    int a = (pixel >> 24) & 0xFF;
    int r = (pixel >> 16) & 0xFF;
    int g = (pixel >> 8) & 0xFF;
    int b = pixel & 0xFF;

    return json{{"success", true}, {"x", x}, {"y", y}, {"r", r}, {"g", g}, {"b", b}, {"a", a}};
  });

  // Get D3D9 Render State (for debugging rendering issues)
  RegisterHandler("getD3D9State", [](const json& /*params*/) {
    auto* renderer = dynamic_cast<zCRnd_D3D_DX9*>(zrenderer);
    if (!renderer) {
      return json{{"error", "D3D9 renderer not active"}};
    }

    auto* device = renderer->GetDevice();
    if (!device) {
      return json{{"error", "No D3D9 device"}};
    }

    json state;
    state["success"] = true;

    D3DVIEWPORT9 viewport;
    if (SUCCEEDED(device->GetViewport(&viewport))) {
      state["viewport"] = {{"x", viewport.X}, {"y", viewport.Y}, {"width", viewport.Width}, {"height", viewport.Height}};
    }

    DWORD fvf;
    if (SUCCEEDED(device->GetFVF(&fvf))) {
      state["fvf"] = fvf;
    }

    json textures = json::array();
    for (int stage = 0; stage < 8; ++stage) {
      IDirect3DBaseTexture9* texture = nullptr;
      if (SUCCEEDED(device->GetTexture(stage, &texture)) && texture) {
        D3DRESOURCETYPE type = texture->GetType();
        if (type == D3DRTYPE_TEXTURE) {
          auto* tex2d = static_cast<IDirect3DTexture9*>(texture);
          D3DSURFACE_DESC desc;
          if (SUCCEEDED(tex2d->GetLevelDesc(0, &desc))) {
            textures.push_back({{"stage", stage}, {"width", desc.Width}, {"height", desc.Height}, {"format", desc.Format}});
          }
        }
        texture->Release();
      }
    }
    state["textures"] = textures;

    for (int stage = 0; stage < 2; ++stage) {
      std::string stage_key = "stage" + std::to_string(stage);
      DWORD colorOp, colorArg1, colorArg2, alphaOp, texCoordIndex, texTransformFlags;

      if (SUCCEEDED(device->GetTextureStageState(stage, D3DTSS_COLOROP, &colorOp))) {
        state[stage_key]["colorOp"] = colorOp;
      }
      if (SUCCEEDED(device->GetTextureStageState(stage, D3DTSS_COLORARG1, &colorArg1))) {
        state[stage_key]["colorArg1"] = colorArg1;
      }
      if (SUCCEEDED(device->GetTextureStageState(stage, D3DTSS_COLORARG2, &colorArg2))) {
        state[stage_key]["colorArg2"] = colorArg2;
      }
      if (SUCCEEDED(device->GetTextureStageState(stage, D3DTSS_ALPHAOP, &alphaOp))) {
        state[stage_key]["alphaOp"] = alphaOp;
      }
      if (SUCCEEDED(device->GetTextureStageState(stage, D3DTSS_TEXCOORDINDEX, &texCoordIndex))) {
        state[stage_key]["texCoordIndex"] = texCoordIndex;
      }
      if (SUCCEEDED(device->GetTextureStageState(stage, D3DTSS_TEXTURETRANSFORMFLAGS, &texTransformFlags))) {
        state[stage_key]["texTransformFlags"] = texTransformFlags;
      }
    }

    DWORD lighting, zenable, zwriteenable, zfunc, alphablendenable, srcblend, destblend;
    if (SUCCEEDED(device->GetRenderState(D3DRS_LIGHTING, &lighting))) {
      state["lighting"] = (lighting != 0);
    }
    if (SUCCEEDED(device->GetRenderState(D3DRS_ZENABLE, &zenable))) {
      state["zEnable"] = zenable;
    }
    if (SUCCEEDED(device->GetRenderState(D3DRS_ZWRITEENABLE, &zwriteenable))) {
      state["zWriteEnable"] = (zwriteenable != 0);
    }
    if (SUCCEEDED(device->GetRenderState(D3DRS_ZFUNC, &zfunc))) {
      state["zFunc"] = zfunc;
    }
    if (SUCCEEDED(device->GetRenderState(D3DRS_ALPHABLENDENABLE, &alphablendenable))) {
      state["alphaBlendEnable"] = (alphablendenable != 0);
    }
    if (SUCCEEDED(device->GetRenderState(D3DRS_SRCBLEND, &srcblend))) {
      state["srcBlend"] = srcblend;
    }
    if (SUCCEEDED(device->GetRenderState(D3DRS_DESTBLEND, &destblend))) {
      state["destBlend"] = destblend;
    }

    IDirect3DVertexShader9* vs = nullptr;
    IDirect3DPixelShader9* ps = nullptr;
    device->GetVertexShader(&vs);
    device->GetPixelShader(&ps);
    state["vertexShader"] = (vs != nullptr);
    state["pixelShader"] = (ps != nullptr);
    if (vs)
      vs->Release();
    if (ps)
      ps->Release();

    return state;
  });
}

// =============================================================================
// Server Thread
// =============================================================================

void MCPPipeHandler::ServerThread() {
  SPDLOG_INFO("MCP Pipe server thread started");

  while (!stop_requested_.load()) {
    // Create named pipe with overlapped I/O support
    pipe_handle_ = CreateNamedPipeA(pipe_name_.c_str(),
                                    PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,  // Enable overlapped I/O
                                    PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                                    1,  // Max instances
                                    kBufferSize, kBufferSize, kTimeout, nullptr);

    if (pipe_handle_ == INVALID_HANDLE_VALUE) {
      SPDLOG_ERROR("Failed to create named pipe: {}", GetLastError());
      // Check stop event with timeout instead of sleep
      if (WaitForSingleObject(stop_event_, 1000) == WAIT_OBJECT_0) {
        break;
      }
      continue;
    }

    SPDLOG_DEBUG("Waiting for MCP client connection...");

    // Set up overlapped structure for ConnectNamedPipe
    OVERLAPPED overlapped = {};
    overlapped.hEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (overlapped.hEvent == NULL) {
      SPDLOG_ERROR("Failed to create overlapped event: {}", GetLastError());
      CloseHandle(pipe_handle_);
      pipe_handle_ = INVALID_HANDLE_VALUE;
      continue;
    }

    // Start async connect
    BOOL connected = ConnectNamedPipe(pipe_handle_, &overlapped);
    DWORD connect_error = GetLastError();

    if (!connected) {
      if (connect_error == ERROR_IO_PENDING) {
        // Connection is pending - wait for either connection or stop event
        HANDLE wait_handles[2] = {overlapped.hEvent, stop_event_};
        DWORD wait_result = WaitForMultipleObjects(2, wait_handles, FALSE, INFINITE);

        if (wait_result == WAIT_OBJECT_0) {
          // Overlapped event signaled - connection completed or failed
          DWORD bytes_transferred = 0;
          if (!GetOverlappedResult(pipe_handle_, &overlapped, &bytes_transferred, FALSE)) {
            DWORD result_error = GetLastError();
            if (result_error != ERROR_PIPE_CONNECTED) {
              SPDLOG_WARN("ConnectNamedPipe failed: {}", result_error);
              CloseHandle(overlapped.hEvent);
              CloseHandle(pipe_handle_);
              pipe_handle_ = INVALID_HANDLE_VALUE;
              continue;
            }
          }
          // Connection successful, fall through to handle client
        } else if (wait_result == WAIT_OBJECT_0 + 1) {
          // Stop event signaled - exit the loop
          SPDLOG_DEBUG("Stop event signaled during connection wait");
          CancelIoEx(pipe_handle_, &overlapped);
          CloseHandle(overlapped.hEvent);
          CloseHandle(pipe_handle_);
          pipe_handle_ = INVALID_HANDLE_VALUE;
          break;
        } else {
          // Wait failed
          SPDLOG_ERROR("WaitForMultipleObjects failed: {}", GetLastError());
          CancelIoEx(pipe_handle_, &overlapped);
          CloseHandle(overlapped.hEvent);
          CloseHandle(pipe_handle_);
          pipe_handle_ = INVALID_HANDLE_VALUE;
          continue;
        }
      } else if (connect_error != ERROR_PIPE_CONNECTED) {
        // Actual error
        SPDLOG_WARN("Client connection failed: {}", connect_error);
        CloseHandle(overlapped.hEvent);
        CloseHandle(pipe_handle_);
        pipe_handle_ = INVALID_HANDLE_VALUE;
        continue;
      }
      // ERROR_PIPE_CONNECTED means client connected between CreateNamedPipe and ConnectNamedPipe
    }

    CloseHandle(overlapped.hEvent);

    // Check if we should stop before handling client
    if (stop_requested_.load()) {
      CloseHandle(pipe_handle_);
      pipe_handle_ = INVALID_HANDLE_VALUE;
      break;
    }

    SPDLOG_INFO("MCP client connected");

    // Handle client
    HandleClient(pipe_handle_);

    // Cleanup
    DisconnectNamedPipe(pipe_handle_);
    CloseHandle(pipe_handle_);
    pipe_handle_ = INVALID_HANDLE_VALUE;

    SPDLOG_INFO("MCP client disconnected");
  }

  SPDLOG_INFO("MCP Pipe server thread exiting");
}

void MCPPipeHandler::HandleClient(HANDLE pipe) {
  std::vector<char> buffer(kBufferSize);

  while (!stop_requested_.load()) {
    DWORD bytes_read = 0;
    BOOL success = ReadFile(pipe, buffer.data(), static_cast<DWORD>(buffer.size()), &bytes_read, nullptr);

    if (!success) {
      DWORD error = GetLastError();
      if (error == ERROR_BROKEN_PIPE) {
        SPDLOG_INFO("Client disconnected (broken pipe)");
        break;
      }
      SPDLOG_WARN("Read failed: {}", error);
      break;
    }

    if (bytes_read == 0) {
      continue;
    }

    std::string request_str(buffer.data(), bytes_read);
    SPDLOG_DEBUG("Received: {}", request_str);

    json response;
    try {
      json request = json::parse(request_str);
      response = DispatchRequest(request);
    } catch (const std::exception& e) {
      SPDLOG_ERROR("JSON parse error: {}", e.what());
      response = json{{"error", "Invalid JSON"}, {"details", e.what()}};
    }

    std::string response_str = response.dump();
    SPDLOG_DEBUG("Sending: {}", response_str);

    DWORD bytes_written = 0;
    success = WriteFile(pipe, response_str.c_str(), static_cast<DWORD>(response_str.size()), &bytes_written, nullptr);

    if (!success) {
      SPDLOG_WARN("Write failed: {}", GetLastError());
      break;
    }
  }
}

json MCPPipeHandler::ProcessRequest(const json& request) {
  json response;

  // Get request ID for response correlation
  if (request.contains("id")) {
    response["id"] = request["id"];
  }

  // Get method
  if (!request.contains("method")) {
    response["error"] = "Missing method";
    return response;
  }

  std::string method = request["method"].get<std::string>();

  // Get params (default to empty object)
  json params = request.value("params", json::object());

  // Find handler
  CommandHandler handler;
  {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    auto it = handlers_.find(method);
    if (it == handlers_.end()) {
      response["error"] = "Unknown method";
      response["method"] = method;
      return response;
    }
    handler = it->second;
  }

  // Execute handler
  try {
    response["result"] = handler(params);
  } catch (const std::exception& e) {
    SPDLOG_ERROR("Handler error for {}: {}", method, e.what());
    response["error"] = e.what();
  }

  return response;
}

json MCPPipeHandler::DispatchRequest(const json& request) {
  // Waiting only observes atomic readiness, so it must stay off the render
  // thread. Every command that reads/writes Gothic objects runs on that thread.
  if (request.value("method", std::string{}) == "waitForReady") return ProcessRequest(request);
  auto pending = std::make_shared<PendingRequest>();
  pending->request = request;
  auto response = pending->completion.get_future();
  {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    if (stop_requested_.load()) return json{{"error", "MCP server is stopping"}};
    if (pending_requests_.size() >= 64) return json{{"error", "MCP command queue is full"}};
    pending_requests_.push_back(pending);
  }
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (response.wait_for(std::chrono::milliseconds(50)) != std::future_status::ready) {
    if (stop_requested_.load() || std::chrono::steady_clock::now() >= deadline) {
      auto queued = PendingRequest::Queued;
      const bool cancelled = pending->state.compare_exchange_strong(queued, PendingRequest::Cancelled);
      return json{{"error", !cancelled && queued == PendingRequest::Running ? "Native command started but response timed out; do not retry" :
          "Native command cancelled before execution"}};
    }
  }
  return response.get();
}

void MCPPipeHandler::ProcessPendingMessages() {
  // This method should be called from the game thread
  // It handles both pending messages and state updates

  // 1. Process state updates
  OnRenderFrame();
  std::deque<std::shared_ptr<PendingRequest>> requests;
  {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    requests.swap(pending_requests_);
  }
  for (auto& pending : requests) {
    auto queued = PendingRequest::Queued;
    if (stop_requested_.load() || !pending->state.compare_exchange_strong(queued, PendingRequest::Running)) continue;
    try {
      pending->completion.set_value(ProcessRequest(pending->request));
    } catch (const std::exception& error) {
      pending->completion.set_value(json{{"error", error.what()}});
    }
  }
}

// =============================================================================
// Game State Tracking
// =============================================================================

void MCPPipeHandler::OnLoadingComplete() {
  SPDLOG_INFO("Loading complete (HT_CLOSELOADSCREEN)");
  // Just mark that loading screen closed, actual ready state is checked in OnRenderFrame
  ready_time_ms_.store(-1);
  game_state_.store(GameState::Loading);
}

void MCPPipeHandler::OnRenderFrame() {
  UpdateGameState();
}

int64_t MCPPipeHandler::GetTimeSinceReady() const {
  const auto ready = ready_time_ms_.load();
  if (ready < 0) {
    return -1;
  }

  const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
  return now - ready;
}

void MCPPipeHandler::UpdateGameState() {
  // Basic checks
  if (!ogame) {
    if (game_state_.load() != GameState::NotStarted) {
      game_state_.store(GameState::NotStarted);
    }
    return;
  }

  // Are we in a world?
  if (!ogame->GetGameWorld()) {
    // If not in world, assume menu or starting
    if (game_state_.load() != GameState::Menu && game_state_.load() != GameState::Starting) {
      game_state_.store(GameState::Menu);
      ready_time_ms_.store(-1);
    }
    return;
  }

  // We are in a world. Check if we have a player.
  auto* player = GetLocalNpc();

  // If no player but in world, we might be in free cam or initializing
  if (!player) {
    ready_time_ms_.store(-1);
    game_state_.store(GameState::Loading);
    return;
  }

  auto current_state = game_state_.load();

  // Transition to InGame
  if (current_state != GameState::InGame) {
    ready_time_ms_.store(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
    game_state_.store(GameState::InGame);
    SPDLOG_INFO("Game is ready! Player: {}", player->GetName().ToChar());
  }
}

}  // namespace mcp
}  // namespace gmp
