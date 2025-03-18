
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

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

enum Attribute { STR = 0, DEX, MP, HP, WEP1H, WEP2H, BOW, XBOW, MLVL, SNEAK, LOCKPICKING, ACROBATICS, PICKPOCKET };

struct CharacterDefinition {
  std::uint32_t id;
  std::string name;
  std::string description;
  std::string team_name;
  std::string armor;
  std::string prim_wep;
  std::string sec_wep;
  short abilities[13];
};

class CharacterDefinitionManager {
public:
  CharacterDefinitionManager();
  CharacterDefinitionManager(const CharacterDefinitionManager&) = delete;
  CharacterDefinitionManager(CharacterDefinitionManager&&) = delete;
  ~CharacterDefinitionManager() = default;

  CharacterDefinitionManager& operator=(const CharacterDefinitionManager&) = delete;
  CharacterDefinitionManager& operator=(CharacterDefinitionManager&&) = delete;

  bool Load(const std::string& definition_file_path);
  const CharacterDefinition& GetCharacterDefinition(std::uint32_t id) const {
    return character_definitions_.at(id);
  }
  bool IsEmpty() const { return character_definitions_.empty(); }

private:
  std::uint32_t id_counter_ = 0;
  std::unordered_map<std::uint32_t, CharacterDefinition> character_definitions_;

  void CreateDefaultCharacterDefinitions();

  void ParseXML(const std::string& path);
  void ParseJSON(const std::string& path);
};
