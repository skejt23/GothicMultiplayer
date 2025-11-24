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

#include <array>
#include <algorithm>
#include <charconv>
#include <chrono>
#include <dylib.hpp>
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

std::uint8_t ClampColorComponent(int value) {
  return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

MessagePacket CreateMessagePacket(std::optional<std::uint32_t> sender_id, std::optional<std::uint32_t> recipient_id,
                                  std::uint8_t r, std::uint8_t g, std::uint8_t b, std::string text,
                                  std::uint8_t packet_type = PT_MSG) {
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
  EventManager::Instance().RegisterEvent(kEventOnPacketName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerConnectName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerDisconnectName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerMessageName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerCommandName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerWhisperName);
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

  clock_ = std::make_unique<GothicClock>(GothicClock::Time{});
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

        PlayerStateUpdatePacket player_b_update_packet;
        player_b_update_packet.packet_type = PT_ACTUAL_STATISTICS;
        player_b_update_packet.player_id = player_b.player_id;
        player_b_update_packet.state = player_b.state;
        player_b_update_packet.state.health_points = player_b.health;

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
      player.health = 100;

      SendRespawnInfo(player.player_id);
    }
  });
}

bool GameServer::HandlePacket(Net::ConnectionHandle connectionHandle, unsigned char* data, std::uint32_t size) {
  Packet p(data, size, connectionHandle);

  unsigned char packetIdentifier = GetPacketIdentifier(p);

  if (packetIdentifier == PT_EXTENDED_4_SCRIPTS) {
    if (auto player_id = player_manager_.GetPlayerId(connectionHandle)) {
      Packet script_packet(p.data + 1, p.length > 0 ? p.length - 1 : 0, connectionHandle);
      EventManager::Instance().TriggerEvent(kEventOnPacketName, OnPacketEvent{*player_id, script_packet});
    }
    return true;
  }
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
    case PT_HP_DIFF:
      MakeHPDiff(p);
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
    case PT_WHISPER:
      HandleWhisp(p);
      break;
    case PT_COMMAND:
      HandleRMConsole(p);
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
  player.head = packet.head_model;
  player.skin = packet.skin_texture;
  player.body = packet.face_texture;
  player.walkstyle = packet.walk_style;
  player.name = packet.player_name;

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

  updated_player.state = packet.state;
}

void GameServer::MakeHPDiff(Packet p) {
  PlayerId victim_player_id;
  short diffed_hp;

  auto attacker_opt = player_manager_.GetPlayerByConnection(p.id);
  if (!attacker_opt.has_value())
    return;

  auto& attacker = attacker_opt.value().get();

  if (attacker.is_ingame) {
    memcpy(&victim_player_id, p.data + 1, sizeof(PlayerId));
    memcpy(&diffed_hp, p.data + (1 + sizeof(PlayerId)), 2);

    auto victim_opt = player_manager_.GetPlayer(victim_player_id);
    if (!victim_opt.has_value()) {
      return;
    }
    auto& victim = victim_opt.value().get();

    if ((victim.health <= 0) || (victim.tod)) {
      return;
    }

    std::optional<PlayerId> killer_id;
    if (victim_player_id != attacker.player_id) {
      killer_id = attacker.player_id;
    }

    if (victim_player_id == attacker.player_id) {
      if (victim.health) {
        victim.health += diffed_hp;
      }
    } else {
      victim.health += diffed_hp;
      if (victim.health == 1) {
        victim.health = 0;
      } else if (victim.health < 0) {
        victim.health = 0;
      }
    }

    if (diffed_hp < 0) {
      EventManager::Instance().TriggerEvent(kEventOnPlayerHitName,
                                            OnPlayerHitEvent{killer_id, victim.player_id, static_cast<std::int16_t>(-diffed_hp)});
    }

    if (victim.health <= 0) {
      HandlePlayerDeath(victim, killer_id);
    } else if (victim.health > 100) {
      victim.health = 100;
    }

    victim.state.health_points = victim.health;
  }
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
        EventManager::Instance().TriggerEvent(kEventOnPlayerCommandName,
                                             OnPlayerCommandEvent{player.player_id, command, params});
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

void GameServer::HandleWhisp(Packet p) {
  auto player_opt = player_manager_.GetPlayerByConnection(p.id);
  if (!player_opt.has_value() || !player_opt.value().get().is_ingame)
    return;

  auto& player = player_opt.value().get();

  MessagePacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);

  packet.message = SanitizeServerText(packet.message);
  packet.r = 255;
  packet.g = 255;
  packet.b = 255;

  if (!packet.recipient.has_value()) {
    SPDLOG_ERROR("No recipient in whisper packet!");
    return;
  }

  auto recipient_opt = player_manager_.GetPlayer(*packet.recipient);
  if (!recipient_opt.has_value())
    return;
  auto& recipient = recipient_opt.value().get();
  packet.sender = player.player_id;

  EventManager::Instance().TriggerEvent(kEventOnPlayerWhisperName, OnPlayerWhisperEvent{player.player_id, recipient.player_id, packet.message});

  SerializeAndSend(packet, LOW_PRIORITY, RELIABLE_ORDERED, player.connection);
  SerializeAndSend(packet, LOW_PRIORITY, RELIABLE_ORDERED, recipient.connection);

  SPDLOG_INFO("({} WHISPERS TO {}) {}", player.name, recipient.name, (const char*)(p.data + 1 + sizeof(PlayerId)));
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

void GameServer::HandleRMConsole(Packet p) {
  // Intentionally left blank. This can be implemented in the scripts.
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
  player_manager_.ForEachIngamePlayer(
      [&](const Player& player) { SerializeAndSend(packet, LOW_PRIORITY, RELIABLE_ORDERED, player.connection); });
}

void GameServer::SendMessageToPlayer(PlayerId player_id, std::uint8_t r, std::uint8_t g, std::uint8_t b,
                                     const std::string& text) {
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

void GameServer::SendPlayerMessageToAll(PlayerId sender_id, std::uint8_t r, std::uint8_t g, std::uint8_t b,
                                        const std::string& text) {
  if (text.empty()) {
    return;
  }

  auto sender = player_manager_.GetPlayer(sender_id);
  if (!sender.has_value() || !sender->get().is_ingame) {
    SPDLOG_WARN("Cannot broadcast message from invalid sender {}", sender_id);
    return;
  }

  auto packet = CreateMessagePacket(sender_id, std::nullopt, r, g, b, text);
  player_manager_.ForEachIngamePlayer(
      [&](const Player& player) { SerializeAndSend(packet, LOW_PRIORITY, RELIABLE_ORDERED, player.connection); });
}

void GameServer::SendPlayerMessageToPlayer(PlayerId sender_id, PlayerId receiver_id, std::uint8_t r, std::uint8_t g,
                                           std::uint8_t b, const std::string& text) {
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
  packet.head_model = joining_player.head;
  packet.skin_texture = joining_player.skin;
  packet.face_texture = joining_player.body;
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
  player_manager_.ForEachPlayer([&](Player& existing_player) {
    if (existing_player.player_id == target_player.player_id) {
      return;
    }

    // Skip players that have not finished the join handshake yet
    if (existing_player.name.empty()) {
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
    player_packet.head_model = existing_player.head;
    player_packet.skin_texture = existing_player.skin;
    player_packet.face_texture = existing_player.body;
    player_packet.walk_style = existing_player.walkstyle;
    player_packet.player_name = existing_player.name;
    existing_players.push_back(std::move(player_packet));
  });

  if (existing_players.empty()) {
    return;
  }

  ExistingPlayersPacket existing_players_packet;
  existing_players_packet.packet_type = PT_EXISTING_PLAYERS;
  existing_players_packet.existing_players = std::move(existing_players);
  SerializeAndSend(existing_players_packet, IMMEDIATE_PRIORITY, RELIABLE, target_player.connection);
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
  player.health = 100;
  player.state.health_points = player.health;

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
  packet.head_model = player.head;
  packet.skin_texture = player.skin;
  packet.face_texture = player.body;
  packet.walk_style = player.walkstyle;

  SerializeAndSend(packet, IMMEDIATE_PRIORITY, RELIABLE, player.connection);

  player_manager_.ForEachIngamePlayer([&](Player& existing_player) {
    if (existing_player.player_id == player.player_id) {
      return;
    }

    existing_player.spawned_players.insert(player.player_id);
    player.streamed_by_players.insert(existing_player.player_id);

    SerializeAndSend(packet, IMMEDIATE_PRIORITY, RELIABLE, existing_player.connection);
  });

  if (was_dead) {
    EventManager::Instance().TriggerEvent(kEventOnPlayerRespawnName, OnPlayerRespawnEvent{player.player_id, player.state.position});
  }

  EventManager::Instance().TriggerEvent(kEventOnPlayerSpawnName, OnPlayerSpawnEvent{player.player_id, player.state.position});
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

  player_manager_.ForEachIngamePlayer([&](const Player& existing_player) {
    SerializeAndSend(packet, IMMEDIATE_PRIORITY, RELIABLE, existing_player.connection);
  });

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

std::vector<GameServer::PlayerId> GameServer::FindNearbyPlayers(const glm::vec3& position, float radius,
                                                                const std::string& world,
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
  streaming_players.insert(streaming_players.end(), player.streamed_by_players.begin(),
                           player.streamed_by_players.end());

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
  GothicClock::Time new_time{static_cast<std::uint16_t>(day == 0 ? current_time.day_ : day),
                             static_cast<std::uint8_t>(hour), static_cast<std::uint8_t>(min)};
  clock_->UpdateTime(new_time);

  EventManager::Instance().TriggerEvent(kEventOnGameTimeName,
                                        OnGameTimeEvent{new_time.day_, new_time.hour_, new_time.min_});

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
