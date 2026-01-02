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
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <version.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <dylib.hpp>
#include <limits>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <iterator>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <random>
#include <sstream>
#include <stack>
#include <string>
#include <string_view>
#include <system_error>

#include "gothic_clock.h"
#include "net_enums.h"
#include "packets.h"
#include "platform_depend.h"
#include "server_events.h"
#include "shared/event.h"
#include "shared/math.h"
#include "znet_server.h"

Net::NetServer* g_net_server = nullptr;

const char* WTF = "Dude, I dont understand you.";
const char* OK = "OK!";
const char* AG = "Access granted!";
const char* PDE = "Player doesn't exist!";
const char* IPNOTBANNED = "Following IP isn't banned!";
const char* IPISBANNED = "Following IP is already banned!";
const char* INVALIDPARAMETER = "Invalid command parameter!";
#define MAX_KILL_TXT 3
const char* KILLED[MAX_KILL_TXT] = {"K.O.", "R.I.P.", "FATALITY"};
std::atomic<bool> g_is_server_running = true;
void (*g_destroy_net_server_func)(Net::NetServer*) = nullptr;

using namespace Net;

namespace {

constexpr std::size_t kMaxWorldNameLength = 32;
constexpr std::size_t kMaxPlayerNameLength = 64;
constexpr const char* kBanListFileName = "bans.json";
constexpr std::string_view kFrame = "-========================================-";

#ifdef MASTER_SERVER_ENDPOINT
constexpr std::string_view kMasterServerEndpoint = MASTER_SERVER_ENDPOINT;
#else
constexpr std::string_view kMasterServerEndpoint{};
#endif

struct MasterServerEndpointInfo {
  std::string host;
  std::string path{"/"};
  int port{80};
  bool use_https{false};
};

std::optional<MasterServerEndpointInfo> ParseMasterServerEndpoint(std::string_view endpoint) {
  if (endpoint.empty()) {
    return std::nullopt;
  }

  MasterServerEndpointInfo info;
  std::string_view remainder = endpoint;
  auto scheme_pos = remainder.find("://");
  if (scheme_pos != std::string_view::npos) {
    auto scheme = remainder.substr(0, scheme_pos);
    if (scheme == "http") {
      info.use_https = false;
    } else if (scheme == "https") {
      info.use_https = true;
      info.port = 443;
    } else {
      return std::nullopt;
    }
    remainder.remove_prefix(scheme_pos + 3);
  }

  if (remainder.empty()) {
    return std::nullopt;
  }

  auto path_pos = remainder.find('/');
  std::string_view host_port = remainder;
  if (path_pos != std::string_view::npos) {
    host_port = remainder.substr(0, path_pos);
    auto path = remainder.substr(path_pos);
    info.path.assign(path.begin(), path.end());
    if (info.path.empty()) {
      info.path = "/";
    }
  }

  if (host_port.empty()) {
    return std::nullopt;
  }

  auto port_pos = host_port.rfind(':');
  if (port_pos != std::string_view::npos) {
    auto port_view = host_port.substr(port_pos + 1);
    int port_value = 0;
    auto result = std::from_chars(port_view.data(), port_view.data() + port_view.size(), port_value);
    if (result.ec != std::errc{}) {
      return std::nullopt;
    }
    info.port = port_value;
    host_port.remove_suffix(host_port.size() - port_pos);
  } else if (!info.use_https) {
    info.port = 80;
  }

  info.host.assign(host_port.begin(), host_port.end());

  if (info.host.empty()) {
    return std::nullopt;
  }

  return info;
}

std::string SanitizeWorldName(std::string world) {
  if (world.size() > kMaxWorldNameLength) {
    SPDLOG_WARN("World name '{}' is longer than {} characters and will be truncated", world, kMaxWorldNameLength);
    world.resize(kMaxWorldNameLength);
  }

  return world;
}

std::unique_ptr<httplib::Client> CreateMasterServerClient(const MasterServerEndpointInfo& info) {
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
  if (info.use_https) {
    auto client = std::make_unique<httplib::SSLClient>(info.host, info.port);
    client->enable_server_certificate_verification(true);
    return client;
  }
#else
  if (info.use_https) {
    SPDLOG_ERROR("Master server endpoint '{}' requires HTTPS support, but the build lacks OpenSSL support.", info.host);
    return nullptr;
  }
#endif

  return std::make_unique<httplib::Client>(info.host, info.port);
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

std::uint8_t ClampColorComponent(int value) {
  return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

MessagePacket CreateMessagePacket(std::optional<std::uint32_t> sender_id, std::optional<std::uint32_t> recipient_id, std::uint8_t r, std::uint8_t g,
                                  std::uint8_t b, std::string text, std::uint8_t packet_type = PT_MSG) {
  MessagePacket packet{};
  packet.packet_type = packet_type;
  packet.message = SanitizeServerText(std::move(text));
  packet.r = ClampColorComponent(static_cast<int>(r));
  packet.g = ClampColorComponent(static_cast<int>(g));
  packet.b = ClampColorComponent(static_cast<int>(b));
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
void SerializeAndSend(const Packet& packet, Net::PacketPriority priority, Net::PacketReliability reliable, Net::ConnectionHandle id,
                      std::uint32_t channel = 0) {
  TContainer buffer;
  auto written_size = bitsery::quickSerialization<bitsery::OutputBufferAdapter<TContainer>>(buffer, packet);
  g_net_server->Send(buffer.data(), written_size, priority, reliable, channel, id);
}

template <typename Packet>
void BroadcastToRelevant(PlayerManager& player_manager, const PlayerManager::Player& subject, const Packet& packet, Net::PacketPriority priority,
                         Net::PacketReliability reliable, std::uint32_t channel = 0) {
  if (subject.is_ingame) {
    SerializeAndSend(packet, priority, reliable, subject.connection, channel);
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
    SerializeAndSend(packet, priority, reliable, viewer.connection, channel);
  }
}

void SendPlayerAttributeUpdate(PlayerManager& player_manager, const PlayerManager::Player& subject, PlayerAttributeId attribute_id,
                               std::int32_t value) {
  PlayerAttributeUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_ATTRIBUTE_UPDATE;
  packet.player_id = subject.player_id;
  packet.attribute_id = attribute_id;
  packet.value = value;

  BroadcastToRelevant(player_manager, subject, packet, IMMEDIATE_PRIORITY, RELIABLE);
}

void SendPlayerAttributeSnapshot(PlayerManager& player_manager, const PlayerManager::Player& subject, Net::ConnectionHandle connection) {
  PlayerAttributeSnapshotPacket packet{};
  packet.packet_type = PT_PLAYER_ATTRIBUTE_SNAPSHOT;
  packet.player_id = subject.player_id;
  packet.strength = subject.strength;
  packet.dexterity = subject.dexterity;
  packet.level = subject.level;
  packet.exp = subject.exp;
  packet.next_level_exp = subject.next_level_exp;
  packet.learn_points = subject.learn_points;
  packet.health = subject.health;
  packet.max_health = subject.max_health;
  packet.mana = subject.mana;
  packet.max_mana = subject.max_mana;
  SerializeAndSend(packet, IMMEDIATE_PRIORITY, RELIABLE_ORDERED, connection);
}

void SendPlayerInstanceUpdate(PlayerManager& player_manager, const PlayerManager::Player& subject, Net::ConnectionHandle connection) {
  PlayerInstanceUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_INSTANCE_UPDATE;
  packet.player_id = subject.player_id;
  packet.instance = subject.instance;
  SerializeAndSend(packet, IMMEDIATE_PRIORITY, RELIABLE_ORDERED, connection);
}

void BroadcastPlayerInstanceUpdate(PlayerManager& player_manager, const PlayerManager::Player& subject) {
  PlayerInstanceUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_INSTANCE_UPDATE;
  packet.player_id = subject.player_id;
  packet.instance = subject.instance;
  BroadcastToRelevant(player_manager, subject, packet, IMMEDIATE_PRIORITY, RELIABLE);
}

void SendPlayerColorUpdate(PlayerManager& player_manager, const PlayerManager::Player& subject, Net::ConnectionHandle connection) {
  PlayerColorUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_COLOR_UPDATE;
  packet.player_id = subject.player_id;
  packet.r = subject.name_color_r;
  packet.g = subject.name_color_g;
  packet.b = subject.name_color_b;
  SerializeAndSend(packet, IMMEDIATE_PRIORITY, RELIABLE_ORDERED, connection);
}

void BroadcastPlayerColorUpdate(PlayerManager& player_manager, const PlayerManager::Player& subject) {
  PlayerColorUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_COLOR_UPDATE;
  packet.player_id = subject.player_id;
  packet.r = subject.name_color_r;
  packet.g = subject.name_color_g;
  packet.b = subject.name_color_b;
  BroadcastToRelevant(player_manager, subject, packet, IMMEDIATE_PRIORITY, RELIABLE);
}

void SendPlayerSkillWeaponUpdate(PlayerManager& player_manager, const PlayerManager::Player& subject, std::int32_t skill_id,
                                 std::int32_t percentage, Net::ConnectionHandle connection) {
  PlayerSkillWeaponUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_SKILL_WEAPON_UPDATE;
  packet.player_id = subject.player_id;
  packet.skill_id = skill_id;
  packet.percentage = percentage;
  SerializeAndSend(packet, IMMEDIATE_PRIORITY, RELIABLE_ORDERED, connection);
}

void BroadcastPlayerSkillWeaponUpdate(PlayerManager& player_manager, const PlayerManager::Player& subject, std::int32_t skill_id,
                                      std::int32_t percentage) {
  PlayerSkillWeaponUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_SKILL_WEAPON_UPDATE;
  packet.player_id = subject.player_id;
  packet.skill_id = skill_id;
  packet.percentage = percentage;
  BroadcastToRelevant(player_manager, subject, packet, IMMEDIATE_PRIORITY, RELIABLE);
}

void SendPlayerTalentUpdate(PlayerManager& player_manager, const PlayerManager::Player& subject, std::int32_t talent_id,
                            std::int32_t talent_value, Net::ConnectionHandle connection) {
  PlayerTalentUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_TALENT_UPDATE;
  packet.player_id = subject.player_id;
  packet.talent_id = talent_id;
  packet.talent_value = talent_value;
  SerializeAndSend(packet, IMMEDIATE_PRIORITY, RELIABLE_ORDERED, connection);
}

void BroadcastPlayerTalentUpdate(PlayerManager& player_manager, const PlayerManager::Player& subject, std::int32_t talent_id,
                                 std::int32_t talent_value) {
  PlayerTalentUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_TALENT_UPDATE;
  packet.player_id = subject.player_id;
  packet.talent_id = talent_id;
  packet.talent_value = talent_value;
  BroadcastToRelevant(player_manager, subject, packet, IMMEDIATE_PRIORITY, RELIABLE);
}

void SendPlayerFatnessUpdate(PlayerManager& player_manager, const PlayerManager::Player& subject, Net::ConnectionHandle connection) {
  PlayerFatnessUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_FATNESS_UPDATE;
  packet.player_id = subject.player_id;
  packet.fatness = subject.fatness;
  SerializeAndSend(packet, IMMEDIATE_PRIORITY, RELIABLE_ORDERED, connection);
}

void BroadcastPlayerFatnessUpdate(PlayerManager& player_manager, const PlayerManager::Player& subject) {
  PlayerFatnessUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_FATNESS_UPDATE;
  packet.player_id = subject.player_id;
  packet.fatness = subject.fatness;
  BroadcastToRelevant(player_manager, subject, packet, IMMEDIATE_PRIORITY, RELIABLE);
}

void SendPlayerScaleUpdate(PlayerManager& player_manager, const PlayerManager::Player& subject, Net::ConnectionHandle connection) {
  PlayerScaleUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_SCALE_UPDATE;
  packet.player_id = subject.player_id;
  packet.scale = subject.scale;
  SerializeAndSend(packet, IMMEDIATE_PRIORITY, RELIABLE_ORDERED, connection);
}

void BroadcastPlayerScaleUpdate(PlayerManager& player_manager, const PlayerManager::Player& subject) {
  PlayerScaleUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_SCALE_UPDATE;
  packet.player_id = subject.player_id;
  packet.scale = subject.scale;
  BroadcastToRelevant(player_manager, subject, packet, IMMEDIATE_PRIORITY, RELIABLE);
}

void SendPlayerOverlayUpdate(PlayerManager& player_manager, const PlayerManager::Player& subject, const std::string& overlay,
                             bool apply, Net::ConnectionHandle connection) {
  PlayerOverlayUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_OVERLAY_UPDATE;
  packet.player_id = subject.player_id;
  packet.overlay = overlay;
  packet.apply = apply ? 1 : 0;
  SerializeAndSend(packet, IMMEDIATE_PRIORITY, RELIABLE_ORDERED, connection);
}

void BroadcastPlayerOverlayUpdate(PlayerManager& player_manager, const PlayerManager::Player& subject, const std::string& overlay,
                                  bool apply) {
  PlayerOverlayUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_OVERLAY_UPDATE;
  packet.player_id = subject.player_id;
  packet.overlay = overlay;
  packet.apply = apply ? 1 : 0;
  BroadcastToRelevant(player_manager, subject, packet, IMMEDIATE_PRIORITY, RELIABLE);
}

PlayerSpawnPacket CreatePlayerSpawnPacket(const PlayerManager::Player& player) {
  auto packet = CreatePlayerSpawnPacket(player);
  return packet;
}

void LoadNetworkLibrary() {
  try {
    static dylib lib("znet_server");
    auto create_net_server_func = lib.get_function<Net::NetServer*()>("CreateNetServer");
    g_destroy_net_server_func = lib.get_function<void(Net::NetServer*)>("DestroyNetServer");
    g_net_server = create_net_server_func();
  } catch (std::exception& ex) {
    SPDLOG_ERROR("LoadNetworkLibrary error: {}", ex.what());
    std::abort();
  }
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

  // Register server-side events.
  EventManager::Instance().RegisterEvent(kEventOnPlayerConnectName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerDisconnectName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerMessageName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerCommandName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerKillName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerDeathName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerDropItemName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerTakeItemName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerCastSpellName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerSpawnName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerRespawnName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerHitName);
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

  if (g_net_server != nullptr) {
    g_net_server->RemovePacketHandler(*this);
    g_destroy_net_server_func(g_net_server);
  }

  g_server = nullptr;
}

bool GameServer::Init() {
  LoadNetworkLibrary();
  g_net_server->AddPacketHandler(*this);
#ifndef WIN32
  if (config_.Get<bool>("daemon")) {
    System::MakeMeDaemon(false);
  }
#endif
  auto slots = config_.Get<std::int32_t>("slots");
  allow_modification = config_.Get<bool>("allow_modification");

  auto port = config_.Get<std::int32_t>("port");

  if (!g_net_server->Start(port, slots)) {
    SPDLOG_CRITICAL("Failed to start server on port {}", port);
    return false;
  }

  ban_manager_ = std::make_unique<BanManager>(*g_net_server);
  ban_manager_->Load();
  g_is_server_running = true;

  auto seconds_per_game_minute = config_.Get<std::int32_t>("seconds_per_game_minute");
  clock_ = std::make_unique<GothicClock>(GothicClock::Time{}, seconds_per_game_minute);
  if (IsPublic() && !kMasterServerEndpoint.empty()) {
    public_list_http_thread_future_ = std::async(&GameServer::AddToPublicListHTTP, this);
    SPDLOG_INFO("Master Server connection successful!");
  } else if (IsPublic()) {
    SPDLOG_WARN("Server marked as public, but no Master Server endpoint is configured. Skipping registration.");
  } else if (!IsPublic()) {
    SPDLOG_WARN("Server marked as private, skipping connection to Master Server..");
  }
  this->last_stand_timer = 0;

  SPDLOG_INFO(kFrame);

  // Initialize Lua VM
  lua_script_ = std::make_unique<LuaScript>();

  // Initialize resource manager
  resource_manager_ = std::make_unique<ResourceManager>();

  // Set up resource-aware timer binding
  resource_manager_->BindResourceAwareTimer(*lua_script_);

  // Set up exports proxy
  resource_manager_->CreateExportsProxy(lua_script_->GetLuaState());

  // Discover and load all resources from resources/
  auto discovered_resources = resource_manager_->DiscoverResources();
  resource_manager_->LogResourceInfo();

  try {
    client_resource_descriptors_ = ClientResourcePackager::Build(resource_manager_->GetDiscoveredResourceInfo());
  } catch (const std::exception& ex) {
    SPDLOG_ERROR("Failed to pack client resources: {}", ex.what());
    return false;
  }

  resource_server_ = std::make_unique<ResourceServer>(config_.Get<std::int32_t>("port"), std::filesystem::absolute("public"));
  if (!resource_server_->Start()) {
    return false;
  }

  for (const auto& resource_name : discovered_resources) {
    resource_manager_->LoadResource(resource_name, *lua_script_);
  }

  last_update_time_ = std::chrono::steady_clock::now();

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
  constexpr double kRadius = 5000.0;

  g_net_server->Pulse();
  clock_->RunClock();

  if (lua_script_) {
    lua_script_->ProcessTimers();
  }

  ProcessRespawns();

  // Send updates to all players.
  auto now = std::chrono::steady_clock::now();
  if (now - last_update_time_ > std::chrono::milliseconds(config_.Get<std::int32_t>("tick_rate_ms"))) {
    last_update_time_ = now;

    // Pre-filter active players
    std::vector<std::pair<PlayerId, const Player*>> active_players;
    active_players.reserve(player_manager_.GetPlayerCount());
    player_manager_.ForEachPlayer([&](const Player& player) {
      if (player.is_ingame) {
        active_players.emplace_back(player.player_id, &player);
      }
    });

    using PlayersKey = std::pair<PlayerId, PlayerId>;
    struct PlayersKeyHash {
      std::size_t operator()(const PlayersKey& key) const {
        std::hash<uint64_t> hasher;
        return hasher(key.first) ^ (hasher(key.second) << 1);
      }
    };

    struct PlayersKeyEqual {
      bool operator()(const PlayersKey& lhs, const PlayersKey& rhs) const {
        return lhs.first == rhs.first && lhs.second == rhs.second;
      }
    };

    // Pre-allocate map with estimated size
    std::unordered_map<PlayersKey, float, PlayersKeyHash, PlayersKeyEqual> distances;
    distances.reserve((active_players.size() * (active_players.size() - 1)) / 2);
    // Iteration over player pairs
    for (size_t i = 0; i < active_players.size(); ++i) {
      for (size_t j = i + 1; j < active_players.size(); ++j) {
        PlayersKey key{std::min(active_players[i].first, active_players[j].first), std::max(active_players[i].first, active_players[j].first)};

        distances[key] = glm::distance(active_players[i].second->state.position, active_players[j].second->state.position);
      }
    }

    for (const auto& [players, distance] : distances) {
      auto player_a_opt = player_manager_.GetPlayer(players.first);
      auto player_b_opt = player_manager_.GetPlayer(players.second);

      if (!player_a_opt.has_value() || !player_b_opt.has_value()) {
        continue;
      }

      const auto& player_a = player_a_opt->get();
      const auto& player_b = player_b_opt->get();

      if (distance < kRadius) {
        PlayerStateUpdatePacket player_a_update_packet;
        player_a_update_packet.packet_type = PT_ACTUAL_STATISTICS;
        player_a_update_packet.player_id = player_a.player_id;
        player_a_update_packet.state = player_a.state;
        player_a_update_packet.state.health_points = player_a.health;
        player_a_update_packet.state.mana_points = player_a.mana;

        PlayerStateUpdatePacket player_b_update_packet;
        player_b_update_packet.packet_type = PT_ACTUAL_STATISTICS;
        player_b_update_packet.player_id = player_b.player_id;
        player_b_update_packet.state = player_b.state;
        player_b_update_packet.state.health_points = player_b.health;
        player_b_update_packet.state.mana_points = player_b.mana;

        SerializeAndSend(player_a_update_packet, IMMEDIATE_PRIORITY, UNRELIABLE, player_b.connection);
        SerializeAndSend(player_b_update_packet, IMMEDIATE_PRIORITY, UNRELIABLE, player_a.connection);
      } else {
        PlayerPositionUpdatePacket player_a_update_packet;
        player_a_update_packet.packet_type = PT_MAP_ONLY;
        player_a_update_packet.player_id = player_a.player_id;
        player_a_update_packet.position = player_a.state.position;

        PlayerPositionUpdatePacket player_b_update_packet;
        player_b_update_packet.packet_type = PT_MAP_ONLY;
        player_b_update_packet.player_id = player_b.player_id;
        player_b_update_packet.position = player_b.state.position;

        SerializeAndSend(player_a_update_packet, IMMEDIATE_PRIORITY, UNRELIABLE, player_b.connection);
        SerializeAndSend(player_b_update_packet, IMMEDIATE_PRIORITY, UNRELIABLE, player_a.connection);
      }
    }
  }
}

void GameServer::ProcessRespawns() {
  auto respawn_time = config_.Get<std::int32_t>("respawn_time_seconds");
  if (respawn_time < 0) {
    return;
  }

  auto now = std::time(nullptr);

  player_manager_.ForEachPlayer([&](Player& player) {
    if (!player.is_ingame || player.tod == 0) {
      return;
    }

    if (respawn_time == 0 || player.tod + respawn_time <= now) {
      player.flags = 0;
      player.tod = 0;
      player.health = player.max_health;
      player.mana = player.max_mana;
      player.state.health_points = player.health;
      player.state.mana_points = player.mana;

      SendRespawnInfo(player.player_id);
    }
  });
}

bool GameServer::HandlePacket(Net::ConnectionHandle connectionHandle, unsigned char* data, std::uint32_t size) {
  Packet p(data, size, connectionHandle);

  unsigned char packetIdentifier = GetPacketIdentifier(p);

  switch (packetIdentifier) {
    case ID_DISCONNECTION_NOTIFICATION: {
      auto player_opt = player_manager_.GetPlayerByConnection(p.id);
      if (player_opt.has_value()) {
        SendDisconnectionInfo(player_opt->get().player_id);
      }
      HandlePlayerDisconnect(p.id);
      SPDLOG_INFO("{} disconnected. Still connected {} users.", g_net_server->GetPlayerIp(p.id), player_manager_.GetPlayerCount());
      break;
    }
    case ID_NEW_INCOMING_CONNECTION: {
      // Add player to the manager
      PlayerId new_player_id = player_manager_.AddPlayer(p.id, "");
      if (auto new_player = player_manager_.GetPlayer(new_player_id)) {
        new_player->get().world = server_world_;
        new_player->get().virtual_world = 0;
      }

      // Send packet with initial information.
      InitialInfoPacket packet;
      packet.packet_type = PT_INITIAL_INFO;
      packet.map_name = server_world_;
      packet.player_id = new_player_id;
      packet.server_name = GetHostname();
      packet.max_slots = static_cast<std::uint16_t>(GetMaxSlots());
      packet.resource_token = resource_server_->IssueToken(p.id);
      packet.resource_base_path = "/public";
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
        packet.client_resources.push_back(std::move(entry));
      }
      SerializeAndSend(packet, HIGH_PRIORITY, RELIABLE, p.id, 9);
    }
      SPDLOG_INFO("ID_NEW_INCOMING_CONNECTION from {} with connection {}. Now we have {} connected users.", g_net_server->GetPlayerIp(p.id), p.id,
                  player_manager_.GetPlayerCount());
      break;
    case ID_INCOMPATIBLE_PROTOCOL_VERSION:
      SPDLOG_WARN("ID_INCOMPATIBLE_PROTOCOL_VERSION");
      break;
    case ID_CONNECTION_LOST: {
      auto player_opt = player_manager_.GetPlayerByConnection(p.id);
      if (player_opt.has_value()) {
        SendDisconnectionInfo(player_opt->get().player_id);
      }
      HandlePlayerDisconnect(p.id);
      SPDLOG_WARN("Connection lost from {}. Still connected {} users.", g_net_server->GetPlayerIp(p.id), player_manager_.GetPlayerCount());
      break;
    }
    case PT_REQUEST_FILE_LENGTH:
    case PT_REQUEST_FILE_PART:
      break;
    case PT_JOIN_GAME:
      SomeoneJoinGame(p);
      break;
    case PT_ACTUAL_STATISTICS:  // dostarcza nam informacji o sobie
      HandlePlayerUpdate(p);
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
    case PT_DROPITEM:
      HandleDropItem(p);
      break;
    case PT_TAKEITEM:
      HandleTakeItem(p);
      break;
    case PT_GAME_INFO:  // na razie tylko czas
      HandleGameInfo(p);
      break;
    case PT_VOICE:
      HandleVoice(p);
      break;
    default:
      SPDLOG_WARN("(S)He or it try to do something strange. It's packet ID: {}", packetIdentifier);
      break;
  }
  return true;
}

bool GameServer::Receive() {
  g_net_server->Pulse();
  return true;
}

unsigned char GameServer::GetPacketIdentifier(const Packet& p) {
  if ((unsigned char)p.data[0] == ID_TIMESTAMP) {
    return (unsigned char)p.data[1 + sizeof(std::uint32_t)];
  } else
    return (unsigned char)p.data[0];
}

void GameServer::DeleteFromPlayerList(PlayerId player_id) {
  player_manager_.RemovePlayer(player_id);
}

void GameServer::HandlePlayerDisconnect(Net::ConnectionHandle connection) {
  resource_server_->RevokeToken(connection);

  auto player_opt = player_manager_.GetPlayerByConnection(connection);
  if (player_opt.has_value()) {
    auto& player = player_opt.value().get();
    if (player.is_ingame) {
      EventManager::Instance().TriggerEvent(kEventOnPlayerDisconnectName, player.player_id);
    }
    DeleteFromPlayerList(player.player_id);
  }
}

void GameServer::HandlePlayerDeath(Player& victim, std::optional<PlayerId> killer_id) {
  if (victim.tod != 0) {
    return;
  }

  victim.health = 0;
  victim.state.health_points = 0;
  victim.tod = time(NULL);

  if (killer_id.has_value() && killer_id.value() != victim.player_id) {
    EventManager::Instance().TriggerEvent(kEventOnPlayerKillName, OnPlayerKillEvent{killer_id.value(), victim.player_id});
  }

  EventManager::Instance().TriggerEvent(kEventOnPlayerDeathName, OnPlayerDeathEvent{victim.player_id, killer_id});

  SendDeathInfo(victim.player_id);
}

void GameServer::SomeoneJoinGame(Packet p) {
  auto player_opt = player_manager_.GetPlayerByConnection(p.id);
  if (!player_opt) {
    SPDLOG_WARN("Someone tried to join game, but he is not on the player list, connection {}!", p.id);
    return;
  }
  auto& player = player_opt.value().get();

  if (!allow_modification) {
    if (!player.passed_crc_test) {
      resource_server_->RevokeToken(p.id);
      player_manager_.RemovePlayerByConnection(p.id);
      g_net_server->AddToBanList(p.id, 3600000);  // i dorzucamy banana na 1h
      return;
    }
  }

  JoinGamePacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  SPDLOG_TRACE("{} from {}", packet, p.id);

  player.state.position = packet.position;
  player.state.nrot = packet.normal;
  player.state.left_hand_item_instance = packet.left_hand_item_instance;
  player.state.right_hand_item_instance = packet.right_hand_item_instance;
  player.state.equipped_armor_instance = packet.equipped_armor_instance;
  player.state.animation = packet.animation;
  player.body_model = packet.body_model;
  player.body_texture = packet.body_texture;
  player.head_model = packet.head_model;
  player.head_texture = packet.head_texture;
  player.walkstyle = packet.walk_style;
  player.name = SanitizePlayerName(packet.player_name);

  // Inform the joining player about already spawned players before any spawn happens
  SendExistingPlayersPacket(player);

  BroadcastPlayerJoined(player);

  // join
  EventManager::Instance().TriggerEvent(kEventOnPlayerConnectName, player.player_id);
}

void GameServer::HandlePlayerUpdate(Packet p) {
  constexpr double kRadiusSquared = 5000.0 * 5000.0;

  auto player_opt = player_manager_.GetPlayerByConnection(p.id);
  if (!player_opt.has_value()) {
    return;
  }
  auto& updated_player = player_opt.value().get();

  PlayerStateUpdatePacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  updated_player.state.position = packet.state.position;
  updated_player.state.nrot = packet.state.nrot;
  updated_player.state.left_hand_item_instance = packet.state.left_hand_item_instance;
  updated_player.state.right_hand_item_instance = packet.state.right_hand_item_instance;
  updated_player.state.equipped_armor_instance = packet.state.equipped_armor_instance;
  updated_player.state.animation = packet.state.animation;
  updated_player.state.weapon_mode = packet.state.weapon_mode;
  updated_player.state.active_spell_nr = packet.state.active_spell_nr;
  updated_player.state.head_direction = packet.state.head_direction;
  updated_player.state.melee_weapon_instance = packet.state.melee_weapon_instance;
  updated_player.state.ranged_weapon_instance = packet.state.ranged_weapon_instance;
  updated_player.state.health_points = updated_player.health;
  updated_player.state.mana_points = updated_player.mana;
}

void GameServer::HandleVoice(Packet p) {
  // TODO: no need to resend player id right now, it won't be needed until we add 3d chat
  std::string data;
  data.reserve(p.length);
  memcpy(data.data(), p.data, p.length);
  player_manager_.ForEachIngamePlayer([&](const Player& existing_player) {
    if (existing_player.connection != p.id) {
      g_net_server->Send((unsigned char*)data.data(), p.length, IMMEDIATE_PRIORITY, UNRELIABLE, 5, existing_player.connection);
    }
  });
}

void GameServer::HandleNormalMsg(Packet p) {
  auto player_opt = player_manager_.GetPlayerByConnection(p.id);
  if (!player_opt.has_value() || !player_opt.value().get().is_ingame || player_opt.value().get().mute)
    return;

  auto& player = player_opt.value().get();

  MessagePacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  packet.message = SanitizeServerText(packet.message);
  packet.r = 255;
  packet.g = 255;
  packet.b = 255;

  if (!packet.message.empty() && packet.message.front() == '/') {
    auto command_line = packet.message.substr(1);
    auto command_start = command_line.find_first_not_of(' ');
    if (command_start != std::string::npos) {
      command_line = command_line.substr(command_start);
      auto space_pos = command_line.find(' ');
      auto command = command_line.substr(0, space_pos);
      if (!command.empty()) {
        auto params_start = command_line.find_first_not_of(' ', space_pos);
        std::string params = params_start == std::string::npos ? std::string{} : command_line.substr(params_start);
        SPDLOG_INFO("{} issued command: /{} {}", player.name, command, params);
        EventManager::Instance().TriggerEvent(kEventOnPlayerCommandName, OnPlayerCommandEvent{player.player_id, command, params});
      }
    }
    return;
  }

  EventManager::Instance().TriggerEvent(kEventOnPlayerMessageName, OnPlayerMessageEvent{player.player_id, packet.message});

  packet.sender = player.player_id;
  player_manager_.ForEachIngamePlayer(
      [&](const Player& existing_player) { SerializeAndSend(packet, LOW_PRIORITY, RELIABLE_ORDERED, existing_player.connection); });

  SPDLOG_INFO("{}", packet);
}

void GameServer::HandleCastSpell(Packet p, bool target) {
  auto player_opt = player_manager_.GetPlayerByConnection(p.id);
  if (!player_opt.has_value() || !player_opt.value().get().is_ingame)
    return;

  auto& player = player_opt.value().get();

  CastSpellPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  packet.caster_id = player.player_id;

  if (target) {
    if (!packet.target_id.has_value()) {
      SPDLOG_ERROR("No target in cast spell packet!");
      return;
    }

    auto target_opt = player_manager_.GetPlayer(*packet.target_id);
    if (!target_opt.has_value() || !target_opt.value().get().is_ingame) {
      return;
    }
  }

  EventManager::Instance().TriggerEvent(kEventOnPlayerCastSpellName, OnPlayerCastSpellEvent{player.player_id, packet.spell_id, packet.target_id});

  player_manager_.ForEachIngamePlayer([&](const Player& existing_player) {
    if (existing_player.player_id != player.player_id) {
      SerializeAndSend(packet, HIGH_PRIORITY, RELIABLE, existing_player.connection);
    }
  });
}

void GameServer::HandleDropItem(Packet p) {
  auto player_opt = player_manager_.GetPlayerByConnection(p.id);
  if (!player_opt.has_value() || !player_opt.value().get().is_ingame)
    return;

  auto& player = player_opt.value().get();

  DropItemPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  packet.player_id = player.player_id;

  EventManager::Instance().TriggerEvent(kEventOnPlayerDropItemName,
                                        OnPlayerDropItemEvent{player.player_id, packet.item_instance, packet.item_amount});

  player_manager_.ForEachIngamePlayer([&](const Player& existing_player) {
    if (existing_player.player_id != player.player_id) {
      SerializeAndSend(packet, HIGH_PRIORITY, RELIABLE, existing_player.connection);
    }
  });
  SPDLOG_INFO("{} DROPPED ITEM. AMOUNT: {}", player.name, packet.item_amount);
}

void GameServer::HandleTakeItem(Packet p) {
  auto player_opt = player_manager_.GetPlayerByConnection(p.id);
  if (!player_opt.has_value() || !player_opt.value().get().is_ingame)
    return;

  auto& player = player_opt.value().get();

  TakeItemPacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  packet.player_id = player.player_id;

  EventManager::Instance().TriggerEvent(kEventOnPlayerTakeItemName, OnPlayerTakeItemEvent{player.player_id, packet.item_instance});

  player_manager_.ForEachIngamePlayer([&](const Player& existing_player) {
    if (existing_player.player_id != player.player_id) {
      SerializeAndSend(packet, HIGH_PRIORITY, RELIABLE, existing_player.connection);
    }
  });
  SPDLOG_INFO("{} TOOK ITEM.", player.name);
}

void GameServer::AddToPublicListHTTP() {
  using namespace std::chrono_literals;

  auto endpoint_info_opt = ParseMasterServerEndpoint(kMasterServerEndpoint);
  if (!endpoint_info_opt) {
    SPDLOG_WARN("Master server endpoint is not configured. Skipping public list updates.");
    return;
  }

  const auto endpoint_info = *endpoint_info_opt;

  auto client = CreateMasterServerClient(endpoint_info);
  if (!client) {
    SPDLOG_ERROR("Unable to create HTTP client for master server endpoint '{}'.", kMasterServerEndpoint);
    return;
  }

  client->set_connection_timeout(5, 0);
  client->set_read_timeout(5, 0);
  client->set_write_timeout(5, 0);

  auto server_name = SanitizeServerText(g_server->config_.Get<std::string>("name"));
  auto server_auth_key = SanitizeServerText(g_server->config_.Get<std::string>("auth_key"));
  const auto make_server_key = [](std::string address, std::uint32_t port) {
    if (address == "0.0.0.0" || address == "::" || address.empty()) {
      address = "0.0.0.0";
    }

    return address + ":" + std::to_string(port);
  };

  auto server_address = g_net_server->GetAddress();
  auto server_port = static_cast<std::uint32_t>(g_net_server->GetPort());
  auto server_key = make_server_key(server_address, server_port);
  auto last_update = std::chrono::system_clock::now() - 15s;

  while (g_is_server_running) {
    auto now = std::chrono::system_clock::now();
    if (now - last_update >= 15s) {
      last_update = now;
      nlohmann::json server_info{{"servername", server_name},
                                 {"players", static_cast<std::int32_t>(player_manager_.GetPlayerCount())},
                                 {"maxslots", g_server->config_.Get<std::int32_t>("slots")}};

      if (!server_auth_key.empty()) {
        server_info["auth_key"] = server_auth_key;
      }

      nlohmann::json payload = nlohmann::json::object();
      payload[server_key] = std::move(server_info);

      auto response = client->Post(endpoint_info.path.c_str(), payload.dump(), "application/json");
      if (!response) {
        SPDLOG_WARN("Failed to update master server at {}:{}{}", endpoint_info.host, endpoint_info.port, endpoint_info.path);
      } else if (response->status >= 400) {
        SPDLOG_WARN("Master server responded with status {} when updating {}:{}{}", response->status, endpoint_info.host, endpoint_info.port,
                    endpoint_info.path);
      }
    }
    std::this_thread::sleep_for(100ms);
  }
}

void GameServer::HandleGameInfo(Packet p) {
  SendGameInfo(p.id);
}

// void GameServer::HandleGameInfo(Packet p){
void GameServer::SendGameInfo(Net::ConnectionHandle who) {
  GameInfoPacket packet;
  packet.packet_type = PT_GAME_INFO;
  GothicClock::TimeUnion game_time = clock_->GetTime();
  packet.raw_game_time = game_time.raw;

  if (config_.Get<bool>("hide_map")) {
    packet.flags |= HIDE_MAP;
  }

  SerializeAndSend(packet, MEDIUM_PRIORITY, RELIABLE, who, 9);
}

void GameServer::HandleMapNameReq(Packet p) {
}

void GameServer::SendDisconnectionInfo(PlayerId disconnected_player_id) {
  DisconnectionInfoPacket packet;
  packet.disconnected_id = disconnected_player_id;
  packet.packet_type = PT_LEFT_GAME;

  player_manager_.ForEachIngamePlayer([&](const Player& player) {
    if (player.player_id != disconnected_player_id) {
      SerializeAndSend(packet, IMMEDIATE_PRIORITY, RELIABLE, player.connection);
    }
  });
}

bool GameServer::IsPublic() {
  return (config_.Get<bool>("public")) ? true : false;
}

void GameServer::SendMessageToAll(std::uint8_t r, std::uint8_t g, std::uint8_t b, const std::string& text) {
  if (text.empty()) {
    return;
  }

  auto packet = CreateMessagePacket(std::nullopt, std::nullopt, r, g, b, text);
  player_manager_.ForEachIngamePlayer([&](const Player& player) { SerializeAndSend(packet, LOW_PRIORITY, RELIABLE_ORDERED, player.connection); });
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
  SerializeAndSend(packet, LOW_PRIORITY, RELIABLE_ORDERED, target_player->get().connection);
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
  player_manager_.ForEachIngamePlayer([&](const Player& player) { SerializeAndSend(packet, LOW_PRIORITY, RELIABLE_ORDERED, player.connection); });
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
  SerializeAndSend(packet, LOW_PRIORITY, RELIABLE_ORDERED, receiver->get().connection);
}

void GameServer::SendDeathInfo(PlayerId dead_player_id) {
  PlayerDeathInfoPacket packet;
  packet.packet_type = PT_DODIE;
  packet.player_id = dead_player_id;

  player_manager_.ForEachIngamePlayer([&](const Player& player) { SerializeAndSend(packet, IMMEDIATE_PRIORITY, RELIABLE, player.connection, 13); });
}

void GameServer::SendRespawnInfo(PlayerId respawned_player_id) {
  PlayerRespawnInfoPacket packet;
  packet.packet_type = PT_RESPAWN;
  packet.player_id = respawned_player_id;

  player_manager_.ForEachIngamePlayer([&](const Player& player) { SerializeAndSend(packet, IMMEDIATE_PRIORITY, RELIABLE, player.connection, 13); });
}

void GameServer::BroadcastPlayerJoined(const Player& joining_player) {
  JoinGamePacket packet;
  packet.packet_type = PT_JOIN_GAME;
  packet.position = joining_player.state.position;
  packet.normal = joining_player.state.nrot;
  packet.left_hand_item_instance = joining_player.state.left_hand_item_instance;
  packet.right_hand_item_instance = joining_player.state.right_hand_item_instance;
  packet.equipped_armor_instance = joining_player.state.equipped_armor_instance;
  packet.animation = joining_player.state.animation;
  packet.body_model = joining_player.body_model;
  packet.body_texture = joining_player.body_texture;
  packet.head_model = joining_player.head_model;
  packet.head_texture = joining_player.head_texture;
  packet.walk_style = joining_player.walkstyle;
  packet.player_name = joining_player.name;
  packet.player_id = joining_player.player_id;

  player_manager_.ForEachPlayer([&](const Player& existing_player) {
    if (existing_player.player_id == joining_player.player_id) {
      return;
    }
    SerializeAndSend(packet, HIGH_PRIORITY, RELIABLE, existing_player.connection);
  });
}

void GameServer::SendExistingPlayersPacket(Player& target_player) {
  std::vector<ExistingPlayerInfo> existing_players;
  std::vector<PlayerId> existing_player_ids;
  player_manager_.ForEachPlayer([&](Player& existing_player) {
    if (existing_player.player_id == target_player.player_id) {
      return;
    }

    // Skip players that have not finished the join handshake yet
    if (existing_player.name.empty()) {
      return;
    }

    if (existing_player.world != target_player.world || existing_player.virtual_world != target_player.virtual_world) {
      return;
    }

    target_player.spawned_players.insert(existing_player.player_id);
    existing_player.streamed_by_players.insert(target_player.player_id);

    ExistingPlayerInfo player_packet;
    player_packet.player_id = existing_player.player_id;
    player_packet.position = existing_player.state.position;
    player_packet.left_hand_item_instance = existing_player.state.left_hand_item_instance;
    player_packet.right_hand_item_instance = existing_player.state.right_hand_item_instance;
    player_packet.equipped_armor_instance = existing_player.state.equipped_armor_instance;
    player_packet.body_model = existing_player.body_model;
    player_packet.body_texture = existing_player.body_texture;
    player_packet.head_model = existing_player.head_model;
    player_packet.head_texture = existing_player.head_texture;
    player_packet.walk_style = existing_player.walkstyle;
    player_packet.player_name = existing_player.name;
    existing_players.push_back(std::move(player_packet));
    existing_player_ids.push_back(existing_player.player_id);
  });

  if (existing_players.empty()) {
    return;
  }

  ExistingPlayersPacket existing_players_packet;
  existing_players_packet.packet_type = PT_EXISTING_PLAYERS;
  existing_players_packet.existing_players = std::move(existing_players);
  SerializeAndSend(existing_players_packet, IMMEDIATE_PRIORITY, RELIABLE_ORDERED, target_player.connection);

  for (const auto player_id : existing_player_ids) {
    auto existing_player_opt = player_manager_.GetPlayer(player_id);
    if (!existing_player_opt.has_value()) {
      continue;
    }
    const auto& existing_player = existing_player_opt->get();
    SendPlayerAttributeSnapshot(player_manager_, existing_player, target_player.connection);

    if (!existing_player.body_model.empty() || !existing_player.head_model.empty()) {
      PlayerVisualUpdatePacket packet{};
      packet.packet_type = PT_PLAYER_VISUAL_UPDATE;
      packet.player_id = existing_player.player_id;
      packet.body_model = existing_player.body_model;
      packet.body_texture = existing_player.body_texture;
      packet.head_model = existing_player.head_model;
      packet.head_texture = existing_player.head_texture;
      SerializeAndSend(packet, IMMEDIATE_PRIORITY, RELIABLE_ORDERED, target_player.connection);
    }

    SendPlayerInstanceUpdate(player_manager_, existing_player, target_player.connection);
    SendPlayerColorUpdate(player_manager_, existing_player, target_player.connection);
    SendPlayerFatnessUpdate(player_manager_, existing_player, target_player.connection);
    SendPlayerScaleUpdate(player_manager_, existing_player, target_player.connection);

    for (const auto& [skill_id, percentage] : existing_player.weapon_skills) {
      SendPlayerSkillWeaponUpdate(player_manager_, existing_player, skill_id, percentage, target_player.connection);
    }

    for (const auto& [talent_id, value] : existing_player.talents) {
      SendPlayerTalentUpdate(player_manager_, existing_player, talent_id, value, target_player.connection);
    }

    for (const auto& overlay : existing_player.overlays) {
      SendPlayerOverlayUpdate(player_manager_, existing_player, overlay, true, target_player.connection);
    }
  }
}

bool GameServer::SpawnPlayer(PlayerId player_id, std::optional<glm::vec3> position_override) {
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
    player.state.position = *position_override;
  }

  const bool was_dead = player.tod != 0;

  player.flags = 0;
  player.tod = 0;
  player.health = player.max_health;
  player.mana = player.max_mana;
  player.state.health_points = player.health;
  player.state.mana_points = player.mana;

  player.is_ingame = 1;

  PlayerSpawnPacket packet;
  packet.packet_type = PT_PLAYER_SPAWN;
  packet.player_id = player.player_id;
  packet.player_name = player.name;
  packet.position = player.state.position;
  packet.normal = player.state.nrot;
  packet.left_hand_item_instance = player.state.left_hand_item_instance;
  packet.right_hand_item_instance = player.state.right_hand_item_instance;
  packet.equipped_armor_instance = player.state.equipped_armor_instance;
  packet.animation = player.state.animation;
  packet.body_model = player.body_model;
  packet.body_texture = player.body_texture;
  packet.head_model = player.head_model;
  packet.head_texture = player.head_texture;
  packet.walk_style = player.walkstyle;

  SerializeAndSend(packet, IMMEDIATE_PRIORITY, RELIABLE_ORDERED, player.connection);
  SendPlayerAttributeSnapshot(player_manager_, player, player.connection);
  if (!player.body_model.empty() || !player.head_model.empty()) {
    PlayerVisualUpdatePacket visual_packet{};
    visual_packet.packet_type = PT_PLAYER_VISUAL_UPDATE;
    visual_packet.player_id = player.player_id;
    visual_packet.body_model = player.body_model;
    visual_packet.body_texture = player.body_texture;
    visual_packet.head_model = player.head_model;
    visual_packet.head_texture = player.head_texture;
    SerializeAndSend(visual_packet, IMMEDIATE_PRIORITY, RELIABLE_ORDERED, player.connection);
  }

  SendPlayerInstanceUpdate(player_manager_, player, player.connection);
  SendPlayerColorUpdate(player_manager_, player, player.connection);
  SendPlayerFatnessUpdate(player_manager_, player, player.connection);
  SendPlayerScaleUpdate(player_manager_, player, player.connection);

  for (const auto& [skill_id, percentage] : player.weapon_skills) {
    SendPlayerSkillWeaponUpdate(player_manager_, player, skill_id, percentage, player.connection);
  }

  for (const auto& [talent_id, value] : player.talents) {
    SendPlayerTalentUpdate(player_manager_, player, talent_id, value, player.connection);
  }

  for (const auto& overlay : player.overlays) {
    SendPlayerOverlayUpdate(player_manager_, player, overlay, true, player.connection);
  }

  player_manager_.ForEachIngamePlayer([&](Player& existing_player) {
    if (existing_player.player_id == player.player_id) {
      return;
    }

    existing_player.spawned_players.insert(player.player_id);
    player.streamed_by_players.insert(existing_player.player_id);

    SerializeAndSend(packet, IMMEDIATE_PRIORITY, RELIABLE_ORDERED, existing_player.connection);
    SendPlayerAttributeSnapshot(player_manager_, player, existing_player.connection);
    if (!player.body_model.empty() || !player.head_model.empty()) {
      PlayerVisualUpdatePacket visual_packet{};
      visual_packet.packet_type = PT_PLAYER_VISUAL_UPDATE;
      visual_packet.player_id = player.player_id;
      visual_packet.body_model = player.body_model;
      visual_packet.body_texture = player.body_texture;
      visual_packet.head_model = player.head_model;
      visual_packet.head_texture = player.head_texture;
      SerializeAndSend(visual_packet, IMMEDIATE_PRIORITY, RELIABLE_ORDERED, existing_player.connection);
    }

    SendPlayerInstanceUpdate(player_manager_, player, existing_player.connection);
    SendPlayerColorUpdate(player_manager_, player, existing_player.connection);
    SendPlayerFatnessUpdate(player_manager_, player, existing_player.connection);
    SendPlayerScaleUpdate(player_manager_, player, existing_player.connection);

    for (const auto& [skill_id, percentage] : player.weapon_skills) {
      SendPlayerSkillWeaponUpdate(player_manager_, player, skill_id, percentage, existing_player.connection);
    }

    for (const auto& [talent_id, value] : player.talents) {
      SendPlayerTalentUpdate(player_manager_, player, talent_id, value, existing_player.connection);
    }

    for (const auto& overlay : player.overlays) {
      SendPlayerOverlayUpdate(player_manager_, player, overlay, true, existing_player.connection);
    }
  });

  if (was_dead) {
    EventManager::Instance().TriggerEvent(kEventOnPlayerRespawnName, OnPlayerRespawnEvent{player.player_id, player.state.position});
  }

  EventManager::Instance().TriggerEvent(kEventOnPlayerSpawnName, OnPlayerSpawnEvent{player.player_id, player.state.position});
  return true;
}

bool GameServer::SetPlayerName(PlayerId player_id, const std::string& name) {
  auto player_opt = player_manager_.GetPlayer(player_id);
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

  BroadcastToRelevant(player_manager_, player, packet, IMMEDIATE_PRIORITY, RELIABLE);
  return true;
}

bool GameServer::SetPlayerPosition(PlayerId player_id, const glm::vec3& position) {
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerPosition called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  player.state.position = position;

  PlayerPositionUpdatePacket packet{};
  packet.packet_type = PT_MAP_ONLY;
  packet.player_id = player.player_id;
  packet.position = position;

  BroadcastToRelevant(player_manager_, player, packet, IMMEDIATE_PRIORITY, RELIABLE);

  return true;
}

bool GameServer::SetPlayerAngle(PlayerId player_id, float angle) {
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerAngle called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  const float angle_radians = angle;
  player.state.nrot = glm::vec3{std::cos(angle_radians), 0.0f, -std::sin(angle_radians)};

  PlayerStateUpdatePacket packet{};
  packet.packet_type = PT_ACTUAL_STATISTICS;
  packet.player_id = player.player_id;
  packet.state = player.state;
  packet.state.health_points = player.health;
  packet.state.mana_points = player.mana;

  BroadcastToRelevant(player_manager_, player, packet, IMMEDIATE_PRIORITY, RELIABLE);
  return true;
}

bool GameServer::SetPlayerWorld(PlayerId player_id, const std::string& world, std::optional<std::string> start_point) {
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerWorld called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  const auto sanitized_world = SanitizeWorldName(world);
  const bool world_changed = sanitized_world != player.world;

  if (player.is_ingame && world_changed) {
    DisconnectionInfoPacket left_packet;
    left_packet.packet_type = PT_LEFT_GAME;
    left_packet.disconnected_id = player.player_id;

    for (const auto viewer_id : player.streamed_by_players) {
      auto viewer_opt = player_manager_.GetPlayer(viewer_id);
      if (!viewer_opt.has_value()) {
        continue;
      }
      auto& viewer = viewer_opt->get();
      viewer.spawned_players.erase(player.player_id);
      SerializeAndSend(left_packet, IMMEDIATE_PRIORITY, RELIABLE_ORDERED, viewer.connection);
    }

    for (const auto spawned_id : player.spawned_players) {
      auto spawned_opt = player_manager_.GetPlayer(spawned_id);
      if (!spawned_opt.has_value()) {
        continue;
      }
      auto& spawned_player = spawned_opt->get();
      spawned_player.streamed_by_players.erase(player.player_id);
      DisconnectionInfoPacket packet;
      packet.packet_type = PT_LEFT_GAME;
      packet.disconnected_id = spawned_player.player_id;
      SerializeAndSend(packet, IMMEDIATE_PRIORITY, RELIABLE_ORDERED, player.connection);
    }
    player.spawned_players.clear();
    player.streamed_by_players.clear();
  }

  player.world = sanitized_world;
  std::string start_point_name = SanitizeServerText(start_point.value_or(""));
  if (start_point_name.size() > 64) {
    start_point_name.resize(64);
  }

  PlayerWorldUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_WORLD_UPDATE;
  packet.player_id = player.player_id;
  packet.world_name = player.world;
  packet.start_point = start_point_name;

  SerializeAndSend(packet, IMMEDIATE_PRIORITY, RELIABLE_ORDERED, player.connection);

  if (!player.is_ingame || !world_changed) {
    return true;
  }

  SendExistingPlayersPacket(player);

  PlayerSpawnPacket spawn_packet;
  spawn_packet.packet_type = PT_PLAYER_SPAWN;
  spawn_packet.player_id = player.player_id;
  spawn_packet.player_name = player.name;
  spawn_packet.position = player.state.position;
  spawn_packet.normal = player.state.nrot;
  spawn_packet.left_hand_item_instance = player.state.left_hand_item_instance;
  spawn_packet.right_hand_item_instance = player.state.right_hand_item_instance;
  spawn_packet.equipped_armor_instance = player.state.equipped_armor_instance;
  spawn_packet.animation = player.state.animation;
  spawn_packet.body_model = player.body_model;
  spawn_packet.body_texture = player.body_texture;
  spawn_packet.head_model = player.head_model;
  spawn_packet.head_texture = player.head_texture;
  spawn_packet.walk_style = player.walkstyle;

  player_manager_.ForEachIngamePlayer([&](Player& existing_player) {
    if (existing_player.player_id == player.player_id) {
      return;
    }
    if (existing_player.world != player.world || existing_player.virtual_world != player.virtual_world) {
      return;
    }

    existing_player.spawned_players.insert(player.player_id);
    player.streamed_by_players.insert(existing_player.player_id);

    SerializeAndSend(spawn_packet, IMMEDIATE_PRIORITY, RELIABLE_ORDERED, existing_player.connection);
    SendPlayerAttributeSnapshot(player_manager_, player, existing_player.connection);
    if (!player.body_model.empty() || !player.head_model.empty()) {
      PlayerVisualUpdatePacket visual_packet{};
      visual_packet.packet_type = PT_PLAYER_VISUAL_UPDATE;
      visual_packet.player_id = player.player_id;
      visual_packet.body_model = player.body_model;
      visual_packet.body_texture = player.body_texture;
      visual_packet.head_model = player.head_model;
      visual_packet.head_texture = player.head_texture;
      SerializeAndSend(visual_packet, IMMEDIATE_PRIORITY, RELIABLE_ORDERED, existing_player.connection);
    }
  });

  return true;
}

bool GameServer::SetPlayerStrength(PlayerId player_id, std::int32_t strength) {
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerStrength called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  player.strength = std::max<std::int32_t>(0, strength);
  SendPlayerAttributeUpdate(player_manager_, player, ATTR_STRENGTH, player.strength);
  return true;
}

bool GameServer::SetPlayerDexterity(PlayerId player_id, std::int32_t dexterity) {
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerDexterity called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  player.dexterity = std::max<std::int32_t>(0, dexterity);
  SendPlayerAttributeUpdate(player_manager_, player, ATTR_DEXTERITY, player.dexterity);
  return true;
}

bool GameServer::SetPlayerLevel(PlayerId player_id, std::int32_t level) {
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerLevel called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  player.level = std::max<std::int32_t>(0, level);
  SendPlayerAttributeUpdate(player_manager_, player, ATTR_LEVEL, player.level);
  return true;
}

bool GameServer::SetPlayerExp(PlayerId player_id, std::int32_t exp) {
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerExp called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  player.exp = std::max<std::int32_t>(0, exp);
  SendPlayerAttributeUpdate(player_manager_, player, ATTR_EXP, player.exp);
  return true;
}

bool GameServer::SetPlayerNextLevelExp(PlayerId player_id, std::int32_t next_level_exp) {
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerNextLevelExp called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  player.next_level_exp = std::max<std::int32_t>(0, next_level_exp);
  SendPlayerAttributeUpdate(player_manager_, player, ATTR_NEXT_LEVEL_EXP, player.next_level_exp);
  return true;
}

bool GameServer::SetPlayerLearnPoints(PlayerId player_id, std::int32_t learn_points) {
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerLearnPoints called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  player.learn_points = std::max<std::int32_t>(0, learn_points);
  SendPlayerAttributeUpdate(player_manager_, player, ATTR_LEARN_POINTS, player.learn_points);
  return true;
}

bool GameServer::SetPlayerMaxHealth(PlayerId player_id, std::int32_t max_health) {
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerMaxHealth called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  player.max_health = static_cast<std::int16_t>(std::max<std::int32_t>(0, max_health));
  if (player.health > player.max_health) {
    player.health = player.max_health;
  }
  player.state.health_points = player.health;
  SendPlayerAttributeUpdate(player_manager_, player, ATTR_MAX_HEALTH, player.max_health);
  SendPlayerAttributeUpdate(player_manager_, player, ATTR_HEALTH, player.health);
  return true;
}

bool GameServer::SetPlayerHealth(PlayerId player_id, std::int32_t health) {
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerHealth called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  const auto clamped = std::clamp<std::int32_t>(health, 0, player.max_health);
  player.health = static_cast<std::int16_t>(clamped);
  player.state.health_points = player.health;
  SendPlayerAttributeUpdate(player_manager_, player, ATTR_HEALTH, player.health);
  return true;
}

bool GameServer::SetPlayerMaxMana(PlayerId player_id, std::int32_t max_mana) {
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerMaxMana called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  player.max_mana = static_cast<std::int16_t>(std::max<std::int32_t>(0, max_mana));
  if (player.mana > player.max_mana) {
    player.mana = player.max_mana;
  }
  player.state.mana_points = player.mana;
  SendPlayerAttributeUpdate(player_manager_, player, ATTR_MAX_MANA, player.max_mana);
  SendPlayerAttributeUpdate(player_manager_, player, ATTR_MANA, player.mana);
  return true;
}

bool GameServer::SetPlayerMana(PlayerId player_id, std::int32_t mana) {
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerMana called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  const auto clamped = std::clamp<std::int32_t>(mana, 0, player.max_mana);
  player.mana = static_cast<std::int16_t>(clamped);
  player.state.mana_points = player.mana;
  SendPlayerAttributeUpdate(player_manager_, player, ATTR_MANA, player.mana);
  return true;
}

bool GameServer::SetPlayerVisual(PlayerId player_id, const std::string& body_model, std::int16_t body_texture, const std::string& head_model,
                                 std::int16_t head_texture) {
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerVisual called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
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

  PlayerVisualUpdatePacket packet{};
  packet.packet_type = PT_PLAYER_VISUAL_UPDATE;
  packet.player_id = player.player_id;
  packet.body_model = player.body_model;
  packet.body_texture = player.body_texture;
  packet.head_model = player.head_model;
  packet.head_texture = player.head_texture;

  BroadcastToRelevant(player_manager_, player, packet, IMMEDIATE_PRIORITY, RELIABLE);
  return true;
}

bool GameServer::SetPlayerInstance(PlayerId player_id, const std::string& instance) {
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerInstance called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  player.instance = SanitizeServerText(instance);
  if (player.instance.size() > 255) {
    player.instance.resize(255);
  }

  BroadcastPlayerInstanceUpdate(player_manager_, player);
  return true;
}

bool GameServer::SetPlayerColor(PlayerId player_id, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerColor called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  player.name_color_r = r;
  player.name_color_g = g;
  player.name_color_b = b;

  BroadcastPlayerColorUpdate(player_manager_, player);
  return true;
}

bool GameServer::SetPlayerSkillWeapon(PlayerId player_id, std::int32_t skill_id, std::int32_t percentage) {
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerSkillWeapon called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  const auto clamped = std::clamp<std::int32_t>(percentage, 0, 100);
  player.weapon_skills[skill_id] = clamped;
  BroadcastPlayerSkillWeaponUpdate(player_manager_, player, skill_id, clamped);
  return true;
}

bool GameServer::SetPlayerTalent(PlayerId player_id, std::int32_t talent_id, std::int32_t talent_value) {
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerTalent called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  const auto clamped = std::max<std::int32_t>(0, talent_value);
  player.talents[talent_id] = clamped;
  BroadcastPlayerTalentUpdate(player_manager_, player, talent_id, clamped);
  return true;
}

bool GameServer::SetPlayerFatness(PlayerId player_id, float fatness) {
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerFatness called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  player.fatness = fatness;
  BroadcastPlayerFatnessUpdate(player_manager_, player);
  return true;
}

bool GameServer::SetPlayerScale(PlayerId player_id, const glm::vec3& scale) {
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("setPlayerScale called for unknown player id {}", player_id);
    return false;
  }

  auto& player = player_opt->get();
  player.scale = scale;
  BroadcastPlayerScaleUpdate(player_manager_, player);
  return true;
}

bool GameServer::ApplyPlayerOverlay(PlayerId player_id, const std::string& overlay) {
  auto player_opt = player_manager_.GetPlayer(player_id);
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

  if (std::find(player.overlays.begin(), player.overlays.end(), overlay_name) == player.overlays.end()) {
    player.overlays.push_back(overlay_name);
    BroadcastPlayerOverlayUpdate(player_manager_, player, overlay_name, true);
  }

  return true;
}

bool GameServer::RemovePlayerOverlay(PlayerId player_id, const std::string& overlay) {
  auto player_opt = player_manager_.GetPlayer(player_id);
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

  player.overlays.erase(it);
  BroadcastPlayerOverlayUpdate(player_manager_, player, overlay_name, false);
  return true;
}

bool GameServer::GiveItem(PlayerId player_id, const std::string& instance, std::int32_t amount) {
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("giveItem called for unknown player id {}", player_id);
    return false;
  }

  auto item_instance = SanitizeServerText(instance);
  if (item_instance.size() > 255) {
    item_instance.resize(255);
  }
  if (item_instance.empty() || amount <= 0) {
    return false;
  }

  auto& player = player_opt->get();
  GiveItemPacket packet{};
  packet.packet_type = PT_GIVEITEM;
  packet.player_id = player.player_id;
  packet.item_instance = std::move(item_instance);
  packet.item_amount = std::max<std::int32_t>(0, amount);

  BroadcastToRelevant(player_manager_, player, packet, IMMEDIATE_PRIORITY, RELIABLE);
  return true;
}

bool GameServer::EquipItem(PlayerId player_id, const std::string& instance, std::int32_t slot_id) {
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("equipItem called for unknown player id {}", player_id);
    return false;
  }

  auto item_instance = SanitizeServerText(instance);
  if (item_instance.size() > 255) {
    item_instance.resize(255);
  }
  if (item_instance.empty()) {
    return false;
  }

  auto& player = player_opt->get();
  EquipItemPacket packet{};
  packet.packet_type = PT_EQUIPITEM;
  packet.player_id = player.player_id;
  packet.item_instance = std::move(item_instance);
  packet.slot_id = static_cast<std::int16_t>(std::clamp<std::int32_t>(
      slot_id, std::numeric_limits<std::int16_t>::min(), std::numeric_limits<std::int16_t>::max()));

  BroadcastToRelevant(player_manager_, player, packet, IMMEDIATE_PRIORITY, RELIABLE);
  return true;
}

bool GameServer::UnequipItem(PlayerId player_id, const std::string& instance) {
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value()) {
    SPDLOG_WARN("unequipItem called for unknown player id {}", player_id);
    return false;
  }

  auto item_instance = SanitizeServerText(instance);
  if (item_instance.size() > 255) {
    item_instance.resize(255);
  }
  if (item_instance.empty()) {
    return false;
  }

  auto& player = player_opt->get();
  UnequipItemPacket packet{};
  packet.packet_type = PT_UNEQUIPITEM;
  packet.player_id = player.player_id;
  packet.item_instance = std::move(item_instance);

  BroadcastToRelevant(player_manager_, player, packet, IMMEDIATE_PRIORITY, RELIABLE);
  return true;
}

std::optional<glm::vec3> GameServer::GetPlayerPosition(PlayerId player_id) const {
  auto player_opt = player_manager_.GetPlayer(player_id);
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

std::vector<GameServer::PlayerId> GameServer::GetSpawnedPlayersForPlayer(PlayerId player_id) const {
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

std::vector<GameServer::PlayerId> GameServer::GetStreamedPlayersByPlayer(PlayerId player_id) const {
  std::vector<PlayerId> streaming_players;
  auto player_opt = player_manager_.GetPlayer(player_id);
  if (!player_opt.has_value() || !player_opt->get().is_ingame) {
    return streaming_players;
  }

  const auto& player = player_opt->get();
  streaming_players.reserve(player.streamed_by_players.size());
  streaming_players.insert(streaming_players.end(), player.streamed_by_players.begin(), player.streamed_by_players.end());

  return streaming_players;
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

  EventManager::Instance().TriggerEvent(kEventOnGameTimeName, OnGameTimeEvent{new_time.day_, new_time.hour_, new_time.min_});

  player_manager_.ForEachIngamePlayer([&](const Player& player) { SendGameInfo(player.connection); });
  return true;
}

GothicClock::Time GameServer::GetTime() const {
  if (!clock_) {
    return GothicClock::Time{};
  }

  return clock_->GetTime();
}

std::uint32_t GameServer::GetPort() const {
  if (g_net_server) {
    return g_net_server->GetPort();
  }
  return 0;
}
