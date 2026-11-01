#include "summon_registry.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <unordered_set>

#include "summon_spells.h"

SummonRegistry::SummonRegistry()
    : definitions_{{22, {"SUMMONED_GOBBO_SKELETON", 1, 20, 5, 75, 60000}}, {25, {"SUMMONED_WOLF", 1, 40, 5, 100, 60000}},
                   {31, {"SUMMONED_SKELETON", 1, 60, 5, 150, 60000}},      {35, {"SUMMONED_GOLEM", 1, 80, 5, 250, 60000}},
                   {40, {"SUMMONED_DEMON", 1, 120, 5, 500, 60000}},        {45, {"SUMMONED_SKELETON", 6, 150, 5, 150, 60000}},
                   {84, {"SUMMONED_GUARDIAN", 1, 60, 5, 150, 60000}},      {88, {"SUMMONED_ZOMBIE", 1, 80, 5, 400, 60000}}} {
}

bool SummonRegistry::Load(const std::filesystem::path& path) {
  try {
    SummonRegistry loaded;
    if (!std::filesystem::exists(path)) {
      definitions_ = std::move(loaded.definitions_);
      return true;
    }
    std::ifstream input(path);
    const auto root = nlohmann::json::parse(input);
    const auto& entries = root.at("summons");
    if (!entries.is_array() || entries.size() > 8)
      return false;
    std::unordered_set<int> seen;
    for (const auto& entry : entries) {
      for (const auto* key : {"spell_id", "count", "mana_cost", "scroll_mana_cost", "max_health", "lifetime_ms"}) {
        if (entry.contains(key)) {
          const auto& value = entry.at(key);
          if (!value.is_number_integer() || value < 0 || value > std::numeric_limits<std::int32_t>::max())
            return false;
        }
      }
      const int id = entry.at("spell_id").get<int>();
      if (id < 0 || id >= 100 || !gmp::IsNpcSummonSpell(static_cast<std::uint16_t>(id)) || !seen.insert(id).second)
        return false;
      auto& definition = loaded.definitions_.at(static_cast<std::uint16_t>(id));
      definition.instance = entry.value("instance", definition.instance);
      definition.count = entry.value("count", definition.count);
      definition.mana_cost = entry.value("mana_cost", definition.mana_cost);
      definition.scroll_mana_cost = entry.value("scroll_mana_cost", definition.scroll_mana_cost);
      definition.max_health = entry.value("max_health", definition.max_health);
      definition.lifetime_ms = entry.value("lifetime_ms", definition.lifetime_ms);
      if (definition.instance.empty() || definition.instance.size() > 128 ||
          !std::all_of(definition.instance.begin(), definition.instance.end(), [](unsigned char ch) { return std::isalnum(ch) || ch == '_'; }) ||
          definition.count < 1 || definition.count > 6 || definition.mana_cost < 0 || definition.mana_cost > 1000000 ||
          definition.scroll_mana_cost < 0 || definition.scroll_mana_cost > 1000000 || definition.max_health < 1 || definition.max_health > 1000000 ||
          definition.lifetime_ms < 1 || definition.lifetime_ms > 3600000)
        return false;
      std::transform(definition.instance.begin(), definition.instance.end(), definition.instance.begin(),
                     [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    }
    definitions_ = std::move(loaded.definitions_);
    return true;
  } catch (const std::exception& error) {
    SPDLOG_ERROR("Cannot load summon definitions '{}': {}", path.string(), error.what());
    return false;
  }
}

const SummonRegistry::Definition* SummonRegistry::Find(std::uint16_t spell_id) const {
  const auto found = definitions_.find(spell_id);
  return found == definitions_.end() ? nullptr : &found->second;
}
