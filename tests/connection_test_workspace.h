#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

#include "network_test_port.h"

// Each integration fixture gets a minimal
// resource that spawns players and reports their IP/identity from callbacks. A tiny client
// script exercises real resource packaging and downloading without game APIs.
class ConnectionTestWorkspace {
public:
  explicit ConnectionTestWorkspace(std::uint32_t slots = 12, bool ban_loopback = false) {
    namespace fs = std::filesystem;
    original_cwd_ = fs::current_path();
    root_ = original_cwd_ / "build" /
            ("connection_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root_);
    Write(root_ / "bans.json", ban_loopback ? R"([{"IP":"127.0.0.1","Reason":"Connection test ban"}])" : "[]");

    const auto resource = root_ / "resources" / "connection_test";
    fs::create_directories(resource / "server");
    fs::create_directories(resource / "client");
    Write(resource / "resource.toml", "version = \"1.0.0\"\nauthor = \"GMP tests\"\nscripts = [\"server/main.lua\"]\n");
    Write(resource / "server" / "main.lua",
          "local players = {}\n"
          "addEventHandler('onPlayerConnect', function(playerId)\n"
          "  players[playerId] = true\n"
          "  if getPlayerName(playerId) == 'AppearanceOverrideProbe' then\n"
          "    setPlayerVisual(playerId, 'HUM_BODY_NAKED0', 2, 'HUM_HEAD_BALD', 7, 0, 0)\n"
          "    removePlayerOverlay(playerId, 'HUMANS_BABE.MDS')\n"
          "  end\n"
          "  spawnPlayer(playerId, 0, 0, 0)\n"
          "  if getPlayerName(playerId) == 'UseProbe' then giveItem(playerId, 'ITMI_TEST', 2) end\n"
          "  if getPlayerName(playerId) == 'IdentityProbe' then\n"
          "    sendMessageToPlayer(playerId, 255, 255, 255, 'identity: ' .. (getPlayerIdentity(playerId) or '<missing>'))\n"
          "  end\nend)\n"
          "addEventHandler('onPlayerDisconnect', function(playerId)\n"
          "  players[playerId] = nil\n"
          "  local ip = getPlayerIP(playerId) or '<missing>'\n"
          "  local identity = getPlayerIdentity(playerId) or '<missing>'\n"
          "  for peerId in pairs(players) do\n"
          "    sendMessageToPlayer(peerId, 255, 255, 255, 'disconnect IP: ' .. ip)\n"
          "    if getPlayerName(playerId) == 'IdentityProbe' then\n"
          "      sendMessageToPlayer(peerId, 255, 255, 255, 'disconnect identity: ' .. identity)\n"
          "    end\n"
          "  end\nend)\n"
          "addEventHandler('onPlayerUseItem', function(playerId, item, quantity, scheme)\n"
          "  assert(item == 'ITMI_TEST' and quantity == 2 and scheme == '')\n"
          "  cancelEvent()\n"
          "  sendMessageToPlayer(playerId, 255, 255, 255, 'item use cancelled')\n"
          "end)\n");
    Write(resource / "client" / "main.lua", "-- Connection test resource payload\n");
    Write(root_ / "config.toml",
          "name = \"Connection test server\"\nport = " + std::to_string(FindTestServerPort()) +
          "\npublic = false\ndaemon = false\nslots = " + std::to_string(slots) + "\nallow_modification = true\n"
          "seconds_per_game_minute = 0\ntick_rate_ms = 10\nlog_to_stdout = true\nlog_level = \"info\"\n"
          "resources = [\"connection_test\"]\naddon_vdfs = []\nmap = \"NEWWORLD\\\\NEWWORLD.ZEN\"\n");
    fs::current_path(root_);
  }

  ~ConnectionTestWorkspace() {
    std::error_code error;
    std::filesystem::current_path(original_cwd_, error);
    std::filesystem::remove_all(root_, error);
  }

private:
  static void Write(const std::filesystem::path& path, const std::string& contents) {
    std::ofstream output(path, std::ios::binary);
    output.exceptions(std::ios::badbit | std::ios::failbit);
    output << contents;
  }

  std::filesystem::path original_cwd_;
  std::filesystem::path root_;
};
