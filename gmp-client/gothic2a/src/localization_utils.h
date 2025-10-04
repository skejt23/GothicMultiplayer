
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

#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <spdlog/spdlog.h>
#endif

namespace localization {

enum class LanguageEncoding {
  kNone,
  kCp1250,
  kCp1251,
};

namespace detail {

struct LanguageEncodingHint {
  LanguageEncoding encoding;
  std::array<const char*, 6> keywords;
};

inline std::string ToLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return value;
}

inline bool KeywordMatches(const std::string& haystack, const char* keyword) {
  if (!keyword || *keyword == '\0') {
    return false;
  }
  return haystack.find(keyword) != std::string::npos;
}

inline constexpr std::array<LanguageEncodingHint, 5> kEncodingHints = {
    {{LanguageEncoding::kCp1250, {"polish", "polski", "german", "deutsch", "czech", "cesky"}},
     {LanguageEncoding::kCp1250, {"hungarian", "magyar", "interslavic", "medzuslovjansky", "italian", "italiano"}},
     {LanguageEncoding::kCp1251, {"russian", "\u0440\u0443\u0441", "rossiya", nullptr, nullptr, nullptr}},
     {LanguageEncoding::kCp1251, {"ukrainian", "\u0443\u043a\u0440", nullptr, nullptr, nullptr, nullptr}},
     {LanguageEncoding::kCp1251, {"\u0440\u0443\u0441\u0441\u043a\u0438\u0439", nullptr, nullptr, nullptr, nullptr, nullptr}}}};

#ifdef _WIN32
inline std::string ConvertUtf8ToCodePage(const std::string& text, unsigned int code_page) {
  if (text.empty()) {
    return text;
  }

  int wide_length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
  if (wide_length <= 0) {
    SPDLOG_WARN("Failed to calculate UTF-8 -> wide char buffer size (error: {})", GetLastError());
    return text;
  }

  std::wstring wide(static_cast<std::size_t>(wide_length), L'\0');
  if (MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wide.data(), wide_length) == 0) {
    SPDLOG_WARN("Failed to convert UTF-8 -> wide char (error: {})", GetLastError());
    return text;
  }

  int cp_length = WideCharToMultiByte(code_page, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
  if (cp_length <= 0) {
    SPDLOG_WARN("Failed to calculate wide char -> code page buffer size (error: {})", GetLastError());
    return text;
  }

  std::string encoded(static_cast<std::size_t>(cp_length), '\0');
  if (WideCharToMultiByte(code_page, 0, wide.c_str(), -1, encoded.data(), cp_length, nullptr, nullptr) == 0) {
    SPDLOG_WARN("Failed to convert wide char -> code page {} (error: {})", code_page, GetLastError());
    return text;
  }

  if (!encoded.empty() && encoded.back() == '\0') {
    encoded.pop_back();
  }

  return encoded;
}
#else
inline std::string ConvertUtf8ToCodePage(const std::string& text, unsigned int) {
  return text;
}
#endif

}  // namespace detail

inline LanguageEncoding DetectLanguageEncoding(const std::string& language_field, const std::filesystem::path& file_path) {
  const std::string lower_language = detail::ToLower(language_field);
  const std::string lower_path = detail::ToLower(file_path.string());

  for (const auto& hint : detail::kEncodingHints) {
    for (const auto* keyword : hint.keywords) {
      if (detail::KeywordMatches(lower_language, keyword) || detail::KeywordMatches(lower_path, keyword)) {
        return hint.encoding;
      }
    }
  }

  const auto path_contains = [&lower_path](const char* needle) { return needle && lower_path.find(needle) != std::string::npos; };

  if (path_contains("_pl") || path_contains("/pl") || path_contains("\\pl")) {
    return LanguageEncoding::kCp1250;
  }
  if (path_contains("_de") || path_contains("/de") || path_contains("\\de")) {
    return LanguageEncoding::kCp1250;
  }
  if (path_contains("_cz") || path_contains("/cz") || path_contains("\\cz")) {
    return LanguageEncoding::kCp1250;
  }
  if (path_contains("_hu") || path_contains("/hu") || path_contains("\\hu")) {
    return LanguageEncoding::kCp1250;
  }
  if (path_contains("_it") || path_contains("/it") || path_contains("\\it")) {
    return LanguageEncoding::kCp1250;
  }
  if (path_contains("_ru") || path_contains("/ru") || path_contains("\\ru")) {
    return LanguageEncoding::kCp1251;
  }
  if (path_contains("_ua") || path_contains("/ua") || path_contains("\\ua")) {
    return LanguageEncoding::kCp1251;
  }
  if (path_contains("_is") || path_contains("/is") || path_contains("\\is")) {
    return LanguageEncoding::kCp1250;
  }

  return LanguageEncoding::kNone;
}

inline std::string ConvertFromUtf8(const std::string& text, LanguageEncoding encoding) {
  switch (encoding) {
    case LanguageEncoding::kCp1250:
      return detail::ConvertUtf8ToCodePage(text, 1250);
    case LanguageEncoding::kCp1251:
      return detail::ConvertUtf8ToCodePage(text, 1251);
    case LanguageEncoding::kNone:
    default:
      return text;
  }
}

inline std::string utf8_to_cp1250(const std::string& text) {
  return detail::ConvertUtf8ToCodePage(text, 1250);
}

inline std::string utf8_to_cp1251(const std::string& text) {
  return detail::ConvertUtf8ToCodePage(text, 1251);
}

}  // namespace localization
