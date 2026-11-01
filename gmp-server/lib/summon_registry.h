#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

// Trusted server definitions. No NPC instance/count/stats come from a cast packet.
class SummonRegistry {
public:
  struct Definition {
    std::string instance;
    std::int32_t count{1};
    std::int32_t mana_cost{0};
    std::int32_t scroll_mana_cost{5};
    std::int32_t max_health{100};
    std::int32_t lifetime_ms{60000};
  };

  SummonRegistry();
  // An absent file uses Gothic II Addon defaults; an invalid file fails startup.
  // Entries override defaults by spell_id, for servers with modified scripts.
  bool Load(const std::filesystem::path& path);
  const Definition* Find(std::uint16_t spell_id) const;

private:
  std::unordered_map<std::uint16_t, Definition> definitions_;
};
