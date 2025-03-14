
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

#include "character_definition.h"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
#include <pugixml.hpp>
#include <string>

namespace {
std::string ToLower(const std::string& str) {
  std::string lower_str = str;
  std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(), [](unsigned char c) { return std::tolower(c); });
  return lower_str;
}
}  // namespace

CharacterDefinitionManager::CharacterDefinitionManager() {
  CreateDefaultCharacterDefinitions();
}

void CharacterDefinitionManager::CreateDefaultCharacterDefinitions() {
  CharacterDefinition new_def;
  new_def.id = id_counter_;
  new_def.name = "Player";
  new_def.team_name = "Human";
  new_def.description = "Default player character.";
  new_def.abilities[STR] = 10;
  new_def.abilities[DEX] = 10;
  new_def.abilities[MP] = 0;
  new_def.abilities[HP] = 100;
  new_def.abilities[WEP1H] = 10;
  new_def.abilities[WEP2H] = 10;
  new_def.abilities[BOW] = 0;
  new_def.abilities[XBOW] = 0;
  new_def.abilities[MLVL] = 0;
  new_def.abilities[SNEAK] = 0;
  new_def.abilities[LOCKPICKING] = 0;
  new_def.abilities[ACROBATICS] = 0;
  new_def.abilities[PICKPOCKET] = 0;
  character_definitions_.insert({new_def.id, new_def});
  id_counter_++;
}

bool CharacterDefinitionManager::Load(const std::string& definition_file_path) {
  std::filesystem::path file_path(definition_file_path);
  if (!std::filesystem::exists(file_path)) {
    SPDLOG_WARN("File '{}' not found. No character definitions loaded.", definition_file_path);
    return false;
  }
  std::string extension = ToLower(file_path.extension().string());

  if (extension == ".xml") {
    ParseXML(definition_file_path);
  } else if (extension == ".json") {
    ParseJSON(definition_file_path);
  } else {
    SPDLOG_WARN("Unsupported file format '{}'. No character definitions loaded.", extension);
    return false;
  }
  return true;
}

void CharacterDefinitionManager::ParseXML(const std::string& path) {
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_file(path.c_str());
  if (!result) {
    SPDLOG_WARN("Unable to load character definitions from '{}'. No character definitions loaded.", path);
    return;
  }

  for (auto classNode : doc.child("character").children("class")) {
    CharacterDefinition new_def;
    new_def.id = id_counter_;
    new_def.name = classNode.child("name").text().get();
    new_def.team_name = classNode.child("team").text().get();
    new_def.description = classNode.child("description").text().get();
    new_def.armor = classNode.child("armor").text().get();
    new_def.prim_wep = classNode.child("prim_wep").text().get();
    new_def.sec_wep = classNode.child("sec_wep").text().get();

    new_def.abilities[STR] = classNode.child("strength").text().as_int();
    new_def.abilities[DEX] = classNode.child("dexterity").text().as_int();
    new_def.abilities[MP] = classNode.child("mana").text().as_int();
    new_def.abilities[HP] = classNode.child("health").text().as_int();
    new_def.abilities[WEP1H] = classNode.child("onehweapon").text().as_int();
    new_def.abilities[WEP2H] = classNode.child("twohweapon").text().as_int();
    new_def.abilities[BOW] = classNode.child("bow").text().as_int();
    new_def.abilities[XBOW] = classNode.child("crossbow").text().as_int();
    new_def.abilities[MLVL] = classNode.child("magic").text().as_int();
    new_def.abilities[SNEAK] = classNode.child("sneaking").text().as_int();
    new_def.abilities[LOCKPICKING] = classNode.child("lockpicking").text().as_int();
    new_def.abilities[ACROBATICS] = classNode.child("acrobatics").text().as_int();
    new_def.abilities[PICKPOCKET] = classNode.child("pickpocket").text().as_int();
    character_definitions_[id_counter_] = std::move(new_def);
    id_counter_++;
  }
}

template <typename T>
T try_get(const nlohmann::json& node, const std::string& key, T default_value = T{}) {
  try {
    if (node.count(key) > 0) {
      return node[key].get<T>();
    }
  } catch (const std::exception&) {
    throw;
  }
  return default_value;
}

void CharacterDefinitionManager::ParseJSON(const std::string& path) {
  std::ifstream file(path);
  nlohmann::json json_data;
  file >> json_data;

  for (const auto& classNode : json_data["character"]["class"]) {
    try {
      CharacterDefinition new_def;
      new_def.id = id_counter_;
      new_def.name = try_get(classNode, "name", std::string{});
      new_def.team_name = try_get(classNode, "team", std::string{});
      new_def.armor = try_get(classNode, "armor", std::string{});
      new_def.prim_wep = try_get(classNode, "prim_wep", std::string{});
      new_def.sec_wep = try_get(classNode, "sec_wep", std::string{});

      new_def.abilities[STR] = try_get(classNode, "strength", 0);
      new_def.abilities[DEX] = try_get(classNode, "dexterity", 0);
      new_def.abilities[MP] = try_get(classNode, "mana", 0);
      new_def.abilities[HP] = try_get(classNode, "health", 0);
      new_def.abilities[WEP1H] = try_get(classNode, "onehweapon", 0);
      new_def.abilities[WEP2H] = try_get(classNode, "twohweapon", 0);
      new_def.abilities[BOW] = try_get(classNode, "bow", 0);
      new_def.abilities[XBOW] = try_get(classNode, "crossbow", 0);
      new_def.abilities[MLVL] = try_get(classNode, "magic_lvl", 0);

      character_definitions_[id_counter_] = std::move(new_def);
      id_counter_++;
    } catch (const std::exception& e) {
      SPDLOG_ERROR("Error: Failed to process class definition: {}", e.what());
      continue;
    }
  }
}
