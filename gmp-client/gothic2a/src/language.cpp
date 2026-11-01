
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

#include "language.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <limits>
#include <nlohmann/json.hpp>
#include <string_view>
#include <system_error>

#include "localization_utils.h"

// ============================================================================
// LanguageManager Implementation
// ============================================================================

LanguageManager& LanguageManager::Instance() {
  static LanguageManager instance;
  return instance;
}

void LanguageManager::LoadLanguages(const char* languageDir, std::string_view languageCode) {
  languageDir_ = languageDir;
  availableLanguages_.clear();
  activeLanguageIndex_ = -1;

  std::vector<std::filesystem::path> languageFiles;
  std::error_code error;
  for (std::filesystem::directory_iterator it(languageDir_, error), end; it != end; it.increment(error)) {
    if (error) {
      break;
    }
    if (it->is_regular_file(error) && !error && it->path().extension() == ".json") {
      languageFiles.push_back(it->path());
    }
  }

  if (error) {
    SPDLOG_ERROR("LanguageManager: Failed to scan language directory {}: {}", languageDir_, error.message());
  }

  // Load metadata for each language file
  for (const auto& langPath : languageFiles) {
    const std::string filename = langPath.filename().string();
    std::ifstream langFile(langPath, std::ifstream::in);

    if (!langFile.is_open()) {
      SPDLOG_ERROR("LanguageManager: Couldn't open language file {}", langPath.string());
      // Add with filename as fallback
      LanguageInfo info;
      info.filename = filename;
      info.displayName = zSTRING(filename.c_str());
      info.encoding = localization::LanguageEncoding::kNone;
      info.displayOrder = std::numeric_limits<int>::max();
      availableLanguages_.push_back(info);
      continue;
    }

    try {
      nlohmann::json jsonData;
      langFile >> jsonData;

      LanguageInfo info;
      info.filename = filename;
      info.displayOrder = std::numeric_limits<int>::max();
      if (const auto order = jsonData.find("ORDER"); order != jsonData.end() && order->is_number_integer()) {
        info.displayOrder = order->get<int>();
      }
      if (info.displayOrder < 0 || info.displayOrder == std::numeric_limits<int>::max()) {
        SPDLOG_WARN("LanguageManager: Language file {} has no valid non-negative ORDER; placing it last", filename);
        info.displayOrder = std::numeric_limits<int>::max();
      }

      // Get display name from JSON
      auto rawLanguageName = jsonData.value("LANGUAGE", std::string{});
      if (rawLanguageName.empty()) {
        rawLanguageName = filename;
      }

      // Detect encoding and convert from UTF-8
      info.encoding = localization::DetectLanguageEncoding(rawLanguageName, langPath.string());
      const auto localizedName = localization::ConvertFromUtf8(rawLanguageName, info.encoding);
      info.displayName = zSTRING(localizedName.c_str());

      availableLanguages_.push_back(info);
      SPDLOG_DEBUG("LanguageManager: Loaded language {} ({})", filename, localizedName);

    } catch (const std::exception& ex) {
      SPDLOG_ERROR("LanguageManager: Failed to parse language file {}: {}", langPath.string(), ex.what());
      // Add with filename as fallback
      LanguageInfo info;
      info.filename = filename;
      info.displayName = zSTRING(filename.c_str());
      info.encoding = localization::LanguageEncoding::kNone;
      info.displayOrder = std::numeric_limits<int>::max();
      availableLanguages_.push_back(info);
    }
  }

  std::sort(availableLanguages_.begin(), availableLanguages_.end(), [](const LanguageInfo& lhs, const LanguageInfo& rhs) {
    if (lhs.displayOrder != rhs.displayOrder) {
      return lhs.displayOrder < rhs.displayOrder;
    }
    return lhs.filename < rhs.filename;
  });

  for (std::size_t index = 1; index < availableLanguages_.size(); ++index) {
    const auto& previous = availableLanguages_[index - 1];
    const auto& current = availableLanguages_[index];
    if (current.displayOrder != std::numeric_limits<int>::max() && current.displayOrder == previous.displayOrder) {
      SPDLOG_WARN("LanguageManager: Language files {} and {} use duplicate ORDER {}",
                  previous.filename,
                  current.filename,
                  current.displayOrder);
    }
  }

  SPDLOG_INFO("LanguageManager: Loaded {} languages", availableLanguages_.size());

  // Load the active language
  if (availableLanguages_.empty()) {
    SPDLOG_ERROR("LanguageManager: No languages available to load");
    return;
  }

  int targetIndex = GetLanguageIndex(languageCode);
  if (targetIndex < 0) {
    SPDLOG_WARN("LanguageManager: Unknown language code '{}'; defaulting to English", languageCode);
    targetIndex = GetLanguageIndex("EN");
  }
  if (targetIndex < 0) {
    targetIndex = 0;
  }

  LoadLanguage(targetIndex);
}

const LanguageManager::LanguageInfo* LanguageManager::GetLanguage(int index) const {
  if (index < 0 || index >= static_cast<int>(availableLanguages_.size())) {
    return nullptr;
  }
  return &availableLanguages_[index];
}

int LanguageManager::GetLanguageIndex(std::string_view languageCode) const {
  const auto language = std::find_if(availableLanguages_.begin(), availableLanguages_.end(), [languageCode](const LanguageInfo& info) {
    return std::filesystem::path(info.filename).stem().string() == languageCode;
  });
  return language == availableLanguages_.end() ? -1 : static_cast<int>(std::distance(availableLanguages_.begin(), language));
}

bool LanguageManager::LoadLanguage(int index) {
  const auto* language = GetLanguage(index);
  if (!language) {
    SPDLOG_ERROR("LanguageManager: Invalid language index {}", index);
    return false;
  }

  const auto languagePath = std::filesystem::path(languageDir_) / language->filename;
  SPDLOG_INFO("LanguageManager: Loading active language: {} from {}", language->displayName.ToChar(), languagePath.string());

  if (!Language::Instance().LoadFromJsonFile(languagePath)) {
    SPDLOG_ERROR("LanguageManager: Failed to load language file {}", languagePath.string());
    return false;
  }

  activeLanguageIndex_ = index;
  return true;
}

// ============================================================================
// Language Implementation
// ============================================================================

namespace {
constexpr std::array<std::string_view, 4> kKnownFontPrefixes = {"CP1250_", "CP1251_", "CP1252_", "CP1254_"};

constexpr auto kStringKeys = std::to_array<const char*>({"LANGUAGE",
                                                           "WRITE_NICKNAME",
                                                           "MMENU_CHSERVER",
                                                           "MMENU_OPTIONS",
                                                           "MMENU_LEAVEGAME",
                                                           "MMENU_BACK",
                                                           "MMENU_NICKNAME",
                                                           "MMENU_ANTIALIASING",
                                                           "MMENU_JOYSTICK",
                                                           "MMENU_LANGUAGE",
                                                           "MMENU_INTROVIDEOS",
                                                           "MMENU_ON",
                                                           "MMENU_OFF",
                                                           "MMENU_YES",
                                                           "MMENU_NO",
                                                           "INGAMEM_BACKTOGAME",
                                                           "EXITTOMAINMENU",
                                                           "DISCONNECTED",
                                                           "ITEM_TOOFAR",
                                                           "SRVLIST_ALL",
                                                           "SRVLIST_FAVOURITES",
                                                           "SRVLIST_NAME",
                                                           "SRVLIST_MAP",
                                                           "SRVLIST_PLAYERNUMBER",
                                                           "SRVLIST_HINT_NAVIGATION",
                                                           "SRVLIST_HINT_FAVOURITES",
                                                           "SRVLIST_HINT_OTHER",
                                                           "SRVLIST_DIRECT_TITLE",
                                                           "SRVLIST_ADD_FAVOURITE_TITLE",
                                                           "SRVLIST_ENTRY_HINT",
                                                           "SRVLIST_INVALID_ENDPOINT",
                                                           "SRVLIST_FAVOURITE_ADDED",
                                                           "SRVLIST_FAVOURITE_EXISTS",
                                                           "SRVLIST_FAVOURITE_SAVE_FAILED",
                                                           "MMENU_GMP_OPTIONS",
                                                           "MMENU_GMP_CATEGORY",
                                                           "MMENU_DISPLAY_CATEGORY",
                                                           "MMENU_VOICE_ENABLED",
                                                           "MMENU_VOICE_PTT_KEY",
                                                           "MMENU_VOICE_VOLUME",
                                                           "MMENU_EXTENDED_MENU_SCENES",
                                                           "MMENU_WINDOW_ALWAYS_ON_TOP",
                                                           "MMENU_VSYNC",
                                                           "MMENU_RENDERER",
                                                           "MMENU_RESTART_REQUIRED",
                                                           "MMENU_APPEARANCE",
                                                           "APP_CONTROLS",
                                                           "APP_SAVE_HINT",
                                                           "APP_BODY",
                                                           "HEAD_MODEL",
                                                           "FACE_APPEARANCE",
                                                           "SKIN_TEXTURE",
                                                           "WALK_STYLE",
                                                           "APP_MALE",
                                                           "APP_FEMALE",
                                                           "APP_FIGHTER",
                                                           "APP_BALD",
                                                           "APP_FAT_BALD",
                                                           "APP_PONY",
                                                           "APP_PSIONIC",
                                                           "APP_THIEF",
                                                           "APP_DEFAULT",
                                                           "APP_GUARD",
                                                           "APP_MAGE",
                                                           "APP_RELAXED",
                                                           "APP_ARROGANT",
                                                           "APP_TIRED",
                                                           "APP_WOMAN"});


std::string_view GetFontPrefixForEncoding(localization::LanguageEncoding encoding) {
  switch (encoding) {
    case localization::LanguageEncoding::kCp1250:
      return "CP1250_";
    case localization::LanguageEncoding::kCp1251:
      return "CP1251_";
    case localization::LanguageEncoding::kCp1252:
      return "CP1252_";
    case localization::LanguageEncoding::kCp1254:
      return "CP1254_";
    case localization::LanguageEncoding::kNone:
    default:
      return {};
  }
}

bool HasKnownFontPrefix(std::string_view font_name) {
  for (const auto prefix : kKnownFontPrefixes) {
    if (!prefix.empty() && font_name.starts_with(prefix)) {
      return true;
    }
  }
  return false;
}
}  // namespace

bool Language::LoadFromJsonFile(const std::filesystem::path& file) {
  std::ifstream ifs(file);
  if (!ifs.good()) {
    SPDLOG_ERROR("Failed to open language file {}: {}", file.string(), strerror(errno));
    return false;
  }

  nlohmann::json json_data;
  try {
    ifs >> json_data;
  } catch (const std::exception& ex) {
    SPDLOG_ERROR("Failed to parse language file {}: {}", file.string(), ex.what());
    return false;
  }

  data.clear();
  const std::string language_field = json_data.value("LANGUAGE", std::string{});
  const auto encoding = localization::DetectLanguageEncoding(language_field, file);
  encoding_ = encoding;
  fontPrefix_ = GetFontPrefixForEncoding(encoding);

  data.resize(kStringKeys.size());
  for (std::size_t i = 0; i < kStringKeys.size(); ++i) {
    const auto key = kStringKeys[i];
    std::string value;
    try {
      if (!json_data.contains(key)) {
        SPDLOG_WARN("Missing language key '{}' in file {}", key, file.string());
      }
      value = json_data.value(key, std::string{});
    } catch (const std::exception& ex) {
      SPDLOG_WARN("Language key '{}' in file {} has incompatible type: {}", key, file.string(), ex.what());
    }
    value = localization::ConvertFromUtf8(value, encoding);
    data[i] = value.c_str();
  }

  return true;
}

std::string Language::ApplyFontPrefix(std::string_view fontName) const {
  if (fontName.empty() || fontPrefix_.empty() || HasKnownFontPrefix(fontName)) {
    return std::string(fontName);
  }
  std::string prefixed;
  prefixed.reserve(fontPrefix_.size() + fontName.size());
  prefixed.append(fontPrefix_);
  prefixed.append(fontName);
  return prefixed;
}

void Language::RemovePolishCharactersFromWideString(std::wstring& txt) {
  wchar_t letter[1] = {0x22};
  size_t found;
  found = 0;
  while (found != std::string::npos) {
    found = txt.find(L"Ż");
    if (found == std::string::npos)
      break;
    else
      txt.replace(found, 1, L"Z");
  }
  found = 0;
  while (found != std::string::npos) {
    found = txt.find(L"&#8221;");
    if (found == std::string::npos)
      break;
    else
      txt.replace(found, 7, letter);
  }
  found = 0;
  while (found != std::string::npos) {
    found = txt.find(L"&#8211;");
    if (found == std::string::npos)
      break;
    else
      txt.replace(found, 7, L"-");
  }
  found = 0;
  while (found != std::string::npos) {
    found = txt.find(L"ż");
    if (found == std::string::npos)
      break;
    else
      txt.replace(found, 1, L"z");
  }
  found = 0;
  while (found != std::string::npos) {
    found = txt.find(L"ł");
    if (found == std::string::npos)
      break;
    else
      txt.replace(found, 1, L"l");
  }
  found = 0;
  while (found != std::string::npos) {
    found = txt.find(L"Ł");
    if (found == std::string::npos)
      break;
    else
      txt.replace(found, 1, L"L");
  }
  found = 0;
  while (found != std::string::npos) {
    found = txt.find(L"ą");
    if (found == std::string::npos)
      break;
    else
      txt.replace(found, 1, L"a");
  }
  found = 0;
  while (found != std::string::npos) {
    found = txt.find(L"ń");
    if (found == std::string::npos)
      break;
    else
      txt.replace(found, 1, L"n");
  }
  found = 0;
  while (found != std::string::npos) {
    found = txt.find(L"ę");
    if (found == std::string::npos)
      break;
    else
      txt.replace(found, 1, L"e");
  }
  found = 0;
  while (found != std::string::npos) {
    found = txt.find(L"ś");
    if (found == std::string::npos)
      break;
    else
      txt.replace(found, 1, L"s");
  }
  found = 0;
  while (found != std::string::npos) {
    found = txt.find(L"ć");
    if (found == std::string::npos)
      break;
    else
      txt.replace(found, 1, L"c");
  }
  found = 0;
  while (found != std::string::npos) {
    found = txt.find(L"ź");
    if (found == std::string::npos)
      break;
    else
      txt.replace(found, 1, L"z");
  }
};

const zSTRING& Language::operator[](unsigned long i) const {
  thread_local zSTRING tl_unknown = "UNKNOWN TRANSLATION STRING";
  return (i < data.size()) ? data[i] : tl_unknown;
}
