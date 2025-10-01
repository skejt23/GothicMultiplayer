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

#include <charconv>
#include <chrono>
#include <dylib.hpp>
#include <future>
#include <iterator>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <stack>
#include <string>
#include <string_view>
#include <system_error>

#include "HTTPServer.h"
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

DiscordActivityPacket MakeDiscordActivityPacket(const GameServer::DiscordActivityState& activity) {
  DiscordActivityPacket packet;
  packet.packet_type = PT_DISCORD_ACTIVITY;
  packet.state = activity.state;
  packet.details = activity.details;
  packet.large_image_key = activity.large_image_key;
  packet.large_image_text = activity.large_image_text;
  packet.small_image_key = activity.small_image_key;
  packet.small_image_text = activity.small_image_text;
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
  g_server = this;

  // Register server-side events.
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

  script.reset();

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
  http_server_ = std::make_unique<HTTPServer>();
  http_server_->Start(port);
  this->last_stand_timer = 0;

  SPDLOG_INFO(kFrame);
  script = std::make_unique<Script>(config_.Get<std::vector<std::string>>("scripts"));
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

  if (script) {
    script->ProcessTimers();
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
  Packet p{data, size, connectionHandle};

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

      // Send packet with initial information.
      InitialInfoPacket packet;
      packet.packet_type = PT_INITIAL_INFO;
      packet.map_name = config_.Get<std::string>("map");
      packet.player_id = new_player_id;
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
      player_manager_.RemovePlayerByConnection(p.id);
      g_net_server->AddToBanList(p.id, 3600000);  // i dorzucamy banana na 1h
      return;
    }
  }

  JoinGamePacket packet;
  using InputAdapter = bitsery::InputBufferAdapter<unsigned char*>;
  auto state = bitsery::quickDeserialization<InputAdapter>({p.data, p.length}, packet);
  SPDLOG_TRACE("{} from {}", packet, p.id);

  bool was_dead = player.tod != 0;
  player.tod = 0;
  player.health = 100;
  player.state.health_points = player.health;

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

  // Update the packet we received with his ID, so we can send it to others.
  packet.player_id = player.player_id;

  std::vector<ExistingPlayerInfo> existing_players;
  existing_players.reserve(player_manager_.GetPlayerCount());

  // Informing other players about new player
  player_manager_.ForEachPlayer([&](const Player& existing_player) {
    if (existing_player.player_id != player.player_id) {
      if (existing_player.is_ingame) {
        SerializeAndSend(packet, IMMEDIATE_PRIORITY, RELIABLE, existing_player.connection);
      }

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
    }
  });

  if (!existing_players.empty()) {
    // Previously, we were splitting this into mulitple packets. Find out if this is still necessary.
    ExistingPlayersPacket existing_players_packet;
    existing_players_packet.packet_type = PT_EXISTING_PLAYERS;
    existing_players_packet.existing_players = std::move(existing_players);
    SerializeAndSend(existing_players_packet, IMMEDIATE_PRIORITY, RELIABLE, p.id);
  }

  player.is_ingame = 1;

  SendDiscordActivity(player.connection);

  // spawn
  if (was_dead) {
    EventManager::Instance().TriggerEvent(kEventOnPlayerRespawnName, OnPlayerRespawnEvent{player.player_id, player.state.position});
  }

  EventManager::Instance().TriggerEvent(kEventOnPlayerSpawnName, OnPlayerSpawnEvent{player.player_id, player.state.position});

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

  if (!packet.message.empty() && packet.message.front() == '/') {
    auto command = packet.message.substr(1);
    if (!command.empty()) {
      SPDLOG_INFO("{} issued command: {}", player.name, command);
      EventManager::Instance().TriggerEvent(kEventOnPlayerCommandName, OnPlayerCommandEvent{player.player_id, std::move(command)});
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

void GameServer::UpdateDiscordActivity(const DiscordActivityState& activity) {
  discord_activity_ = activity;
  discord_activity_initialized_ = true;

  SPDLOG_INFO("Discord activity updated: state='{}', details='{}'", discord_activity_.state, discord_activity_.details);

  auto packet = MakeDiscordActivityPacket(discord_activity_);
  player_manager_.ForEachIngamePlayer([&](const Player& player) { SerializeAndSend(packet, LOW_PRIORITY, RELIABLE, player.connection); });
}

const GameServer::DiscordActivityState& GameServer::GetDiscordActivity() const {
  return discord_activity_;
}

void GameServer::SendDiscordActivity(Net::ConnectionHandle handle) {
  if (!discord_activity_initialized_) {
    return;
  }

  auto packet = MakeDiscordActivityPacket(discord_activity_);
  SerializeAndSend(packet, LOW_PRIORITY, RELIABLE, handle);
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

void GameServer::SendServerMessage(const std::string& message) {
  if (message.empty()) {
    return;
  }

  MessagePacket packet;
  packet.packet_type = PT_SRVMSG;
  packet.message = message;

  player_manager_.ForEachIngamePlayer([&](const Player& player) { SerializeAndSend(packet, MEDIUM_PRIORITY, RELIABLE, player.connection, 11); });
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

std::uint32_t GameServer::GetPort() const {
  if (g_net_server) {
    return g_net_server->GetPort();
  }
  return 0;
}
