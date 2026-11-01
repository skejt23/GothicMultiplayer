/*
MIT License

Copyright (c) 2026 Gothic Multiplayer Team.

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

#include "lua_diagnostics.h"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace lua::diagnostics {
namespace {

constexpr int kMaximumFrames = 64;
constexpr int kMaximumArguments = 64;
constexpr int kMaximumLocalsPerFrame = 64;
constexpr int kMaximumVarargsPerFrame = 32;
constexpr std::size_t kMaximumReportSize = 256 * 1024;
constexpr std::string_view kCallstackHeader = "callstack:";
constexpr std::string_view kArgumentsHeader = "arguments:";
constexpr std::string_view kLocalsHeader = "locals:";
constexpr std::string_view kTruncationMessage = "... Lua diagnostic truncated ...";

class ReportBuilder {
public:
  ReportBuilder() {
    text_.reserve(4096);
  }

  bool AppendLine(std::string_view line) {
    if (truncated_) {
      return false;
    }

    const std::size_t required = line.size() + 1;
    if (text_.size() + required + kTruncationMessage.size() + 1 > kMaximumReportSize) {
      text_.append(kTruncationMessage);
      truncated_ = true;
      return false;
    }

    text_.append(line);
    text_.push_back('\n');
    return true;
  }

  bool IsTruncated() const {
    return truncated_;
  }

  std::string Finish() {
    if (!text_.empty() && text_.back() == '\n') {
      text_.pop_back();
    }
    return std::move(text_);
  }

private:
  std::string text_;
  bool truncated_ = false;
};

std::string NormalizeSource(std::string_view source) {
  if (!source.empty() && (source.front() == '@' || source.front() == '=')) {
    source.remove_prefix(1);
  }

  std::string normalized(source);
  std::replace(normalized.begin(), normalized.end(), '\\', '/');
  return normalized;
}

std::string EscapeString(std::string_view value) {
  std::string escaped;
  escaped.reserve(value.size() + 2);
  escaped.push_back('"');

  constexpr char kHexDigits[] = "0123456789ABCDEF";
  for (unsigned char ch : value) {
    switch (ch) {
      case '\\':
        escaped.append("\\\\");
        break;
      case '"':
        escaped.append("\\\"");
        break;
      case '\n':
        escaped.append("\\n");
        break;
      case '\r':
        escaped.append("\\r");
        break;
      case '\t':
        escaped.append("\\t");
        break;
      default:
        if (std::iscntrl(ch) != 0) {
          escaped.append("\\x");
          escaped.push_back(kHexDigits[(ch >> 4) & 0x0F]);
          escaped.push_back(kHexDigits[ch & 0x0F]);
        } else {
          escaped.push_back(static_cast<char>(ch));
        }
        break;
    }
  }

  escaped.push_back('"');
  return escaped;
}

std::string FormatPointer(const void* pointer) {
  return pointer != nullptr ? fmt::format("{}", fmt::ptr(pointer)) : "null";
}

std::string FormatValue(lua_State* state, int index) {
  const int type = lua_type(state, index);
  switch (type) {
    case LUA_TNIL:
      return "nil";
    case LUA_TBOOLEAN:
      return lua_toboolean(state, index) != 0 ? "true" : "false";
    case LUA_TNUMBER:
      if (lua_isinteger(state, index) != 0) {
        return fmt::format("{}", lua_tointeger(state, index));
      }
      return fmt::format("{:.17g}", lua_tonumber(state, index));
    case LUA_TSTRING: {
      std::size_t length = 0;
      const char* value = lua_tolstring(state, index, &length);
      return EscapeString(std::string_view(value, length));
    }
    case LUA_TLIGHTUSERDATA:
      return FormatPointer(lua_touserdata(state, index));
    case LUA_TTABLE:
    case LUA_TFUNCTION:
    case LUA_TUSERDATA:
    case LUA_TTHREAD:
      return FormatPointer(lua_topointer(state, index));
    default:
      return "<unavailable>";
  }
}

std::string UserdataTypeName(lua_State* state, int index) {
  const int absolute_index = lua_absindex(state, index);
  if (lua_getmetatable(state, absolute_index) == 0) {
    return "userdata";
  }

  lua_pushliteral(state, "__name");
  lua_rawget(state, -2);

  std::string result = "userdata";
  if (lua_type(state, -1) == LUA_TSTRING) {
    std::size_t length = 0;
    const char* name = lua_tolstring(state, -1, &length);
    result = fmt::format("userdata<{}>", std::string_view(name, length));
  }

  lua_pop(state, 2);
  return result;
}

std::string ValueTypeName(lua_State* state, int index) {
  const int type = lua_type(state, index);
  if (type == LUA_TUSERDATA) {
    return UserdataTypeName(state, index);
  }
  const char* name = lua_typename(state, type);
  return name != nullptr ? name : "unknown";
}

bool IsTemporaryLocal(std::string_view name) {
  return name == "(temporary)" || name == "(*temporary)";
}

std::string FormatLocal(lua_State* state, std::string_view name, int value_index) {
  return fmt::format("\t\t{} = {} ({})", name, FormatValue(state, value_index), ValueTypeName(state, value_index));
}

std::string FormatArgument(lua_State* state, int argument_number, int value_index) {
  return fmt::format("\t#{} {}: {}", argument_number, ValueTypeName(state, value_index), FormatValue(state, value_index));
}

std::string FrameSource(const lua_Debug& frame) {
  if (frame.source != nullptr) {
    const std::string_view source(frame.source, frame.srclen);
    if (!source.empty() && (source.front() == '@' || source.front() == '=')) {
      return NormalizeSource(source);
    }
  }
  return NormalizeSource(frame.short_src);
}

std::string FrameDescription(const lua_Debug& frame) {
  if (frame.name != nullptr && frame.name[0] != '\0') {
    return fmt::format("function '{}'", frame.name);
  }
  if (frame.what != nullptr && std::string_view(frame.what) == "main") {
    return "main chunk";
  }
  if (frame.what != nullptr && std::string_view(frame.what) == "C") {
    return "C function";
  }
  if (frame.linedefined > 0) {
    return fmt::format("function <{}:{}>", FrameSource(frame), frame.linedefined);
  }
  return "anonymous function";
}

std::string FrameLine(const lua_Debug& frame) {
  const std::string source = FrameSource(frame);
  if (frame.currentline > 0) {
    return fmt::format("\t{}:{}: in {}", source, frame.currentline, FrameDescription(frame));
  }
  return fmt::format("\t{}: in {}", source, FrameDescription(frame));
}

std::string SourceBasename(const lua_Debug& frame) {
  const std::string source = FrameSource(frame);
  const std::size_t separator = source.find_last_of('/');
  return separator == std::string::npos ? source : source.substr(separator + 1);
}

std::string LocalsFrameLine(const lua_Debug& frame) {
  const int line = frame.currentline > 0 ? frame.currentline : frame.linedefined;
  if (line > 0) {
    return fmt::format("\t{}:{} in {}:", SourceBasename(frame), line, FrameDescription(frame));
  }
  return fmt::format("\t{} in {}:", SourceBasename(frame), FrameDescription(frame));
}

constexpr const char* kNativeArgumentErrorMarker = "__gmp_native_argument_error_v1";

struct BadArgument {
  int number = 0;
  int argument_count = 0;
  bool missing = false;
  bool structured = false;
};

bool IsStructuredArgumentError(lua_State* state) {
  if (lua_type(state, 1) != LUA_TTABLE) {
    return false;
  }
  lua_pushstring(state, kNativeArgumentErrorMarker);
  lua_rawget(state, 1);
  const bool structured = lua_toboolean(state, -1) != 0;
  lua_pop(state, 1);
  return structured;
}

std::optional<BadArgument> ReadStructuredArgumentError(lua_State* state) {
  if (!IsStructuredArgumentError(state)) {
    return std::nullopt;
  }

  lua_pushliteral(state, "bad_argument");
  lua_rawget(state, 1);
  const int number = static_cast<int>(lua_tointeger(state, -1));
  lua_pop(state, 1);
  lua_pushliteral(state, "argument_count");
  lua_rawget(state, 1);
  const int argument_count = static_cast<int>(lua_tointeger(state, -1));
  lua_pop(state, 1);
  lua_pushliteral(state, "missing");
  lua_rawget(state, 1);
  const bool missing = lua_toboolean(state, -1) != 0;
  lua_pop(state, 1);

  if (number <= 0 || number > kMaximumArguments || argument_count < 0) {
    return std::nullopt;
  }
  return BadArgument{number, argument_count, missing, true};
}

std::string ErrorObjectText(lua_State* state) {
  if (IsStructuredArgumentError(state)) {
    lua_pushliteral(state, "message");
    lua_rawget(state, 1);
    std::size_t length = 0;
    const char* message = lua_tolstring(state, -1, &length);
    std::string result = message != nullptr ? std::string(message, length) : "invalid native argument error";
    lua_pop(state, 1);
    return result;
  }

  std::size_t length = 0;
  if (const char* message = lua_tolstring(state, 1, &length); message != nullptr) {
    return std::string(message, length);
  }

  const int type = lua_type(state, 1);
  const char* type_name = lua_typename(state, type);
  return fmt::format("error object is {} ({})", type_name != nullptr ? type_name : "unknown", FormatValue(state, 1));
}

bool AlreadyContainsDiagnostic(std::string_view message) {
  return message.find(kCallstackHeader) != std::string_view::npos;
}

std::optional<BadArgument> ParseBadArgument(std::string_view message) {
  constexpr std::string_view prefix = "bad argument #";
  const std::size_t prefix_start = message.find(prefix);
  if (prefix_start == std::string_view::npos) {
    return std::nullopt;
  }

  const std::size_t number_start = prefix_start + prefix.size();
  std::size_t number_end = number_start;
  while (number_end < message.size() && std::isdigit(static_cast<unsigned char>(message[number_end])) != 0) {
    ++number_end;
  }
  if (number_end == number_start) {
    return std::nullopt;
  }

  int number = 0;
  const char* begin = message.data() + number_start;
  const char* end = message.data() + number_end;
  const auto [parsed_to, parse_error] = std::from_chars(begin, end, number);
  if (parse_error != std::errc{} || parsed_to != end || number <= 0 || number > kMaximumArguments) {
    return std::nullopt;
  }

  return BadArgument{number, 0, message.find("got no value", number_end) != std::string_view::npos, false};
}

int CaptureError(lua_State* state) {
  try {
    const auto structured_bad_argument = ReadStructuredArgumentError(state);
    std::string message = ErrorObjectText(state);
    if (AlreadyContainsDiagnostic(message)) {
      lua_pushlstring(state, message.data(), message.size());
      return 1;
    }

    struct CapturedFrame {
      lua_Debug debug{};
    };

    std::vector<CapturedFrame> frames;
    frames.reserve(kMaximumFrames);
    for (int level = 1; level <= kMaximumFrames; ++level) {
      lua_Debug frame{};
      if (lua_getstack(state, level, &frame) == 0) {
        break;
      }
      if (lua_getinfo(state, "nSltu", &frame) == 0) {
        continue;
      }

      frames.push_back({frame});
    }

    if (message.starts_with("bad argument #") && message.find(" to '") == std::string::npos) {
      for (const CapturedFrame& captured : frames) {
        const lua_Debug& frame = captured.debug;
        if (frame.what == nullptr || std::string_view(frame.what) != "C" || frame.name == nullptr || frame.name[0] == '\0') {
          continue;
        }
        const std::size_t details = message.find(" (");
        if (details != std::string::npos) {
          message.insert(details, fmt::format(" to '{}'", frame.name));
        }
        break;
      }
    }

    ReportBuilder report;
    report.AppendLine(message);
    report.AppendLine(kCallstackHeader);

    for (const CapturedFrame& captured : frames) {
      if (!report.AppendLine(FrameLine(captured.debug))) {
        break;
      }
    }

    if (frames.empty()) {
      report.AppendLine("\t<no Lua frames available>");
    } else if (frames.size() == kMaximumFrames) {
      report.AppendLine("\t... additional frames omitted ...");
    }

    const auto bad_argument = structured_bad_argument ? structured_bad_argument : ParseBadArgument(message);
    lua_Debug* c_frame = nullptr;
    if (!frames.empty() && frames.front().debug.what != nullptr && std::string_view(frames.front().debug.what) == "C") {
      c_frame = &frames.front().debug;
    }
    if (bad_argument && (bad_argument->structured || c_frame != nullptr) && !report.IsTruncated()) {
      report.AppendLine(kArgumentsHeader);
      int structured_arguments_index = 0;
      if (bad_argument->structured) {
        lua_pushliteral(state, "arguments");
        lua_rawget(state, 1);
        if (lua_type(state, -1) == LUA_TTABLE) {
          structured_arguments_index = lua_absindex(state, -1);
        } else {
          lua_pop(state, 1);
        }
      }

      for (int argument = 1; argument <= bad_argument->number && !report.IsTruncated(); ++argument) {
        if (argument == bad_argument->number && bad_argument->missing) {
          report.AppendLine(fmt::format("\t#{} <missing>", argument));
          continue;
        }

        if (structured_arguments_index != 0 && argument <= bad_argument->argument_count) {
          lua_rawgeti(state, structured_arguments_index, argument);
          report.AppendLine(FormatArgument(state, argument, -1));
          lua_pop(state, 1);
        } else if (c_frame != nullptr) {
          const char* argument_name = lua_getlocal(state, c_frame, argument);
          if (argument_name == nullptr) {
            report.AppendLine(fmt::format("\t#{} <unavailable>", argument));
            continue;
          }
          report.AppendLine(FormatArgument(state, argument, -1));
          lua_pop(state, 1);
        } else {
          report.AppendLine(fmt::format("\t#{} <unavailable>", argument));
        }
      }
      if (structured_arguments_index != 0) {
        lua_pop(state, 1);
      }
    }

    bool wrote_locals_header = false;
    bool wrote_locals_frame = false;
    for (std::size_t frame_index = 0; frame_index < frames.size() && !report.IsTruncated(); ++frame_index) {
      lua_Debug& frame = frames[frame_index].debug;
      if (frame.what != nullptr && std::string_view(frame.what) == "C") {
        continue;
      }

      std::vector<std::string> locals;
      locals.reserve(8);

      for (int local_index = 1; local_index <= kMaximumLocalsPerFrame; ++local_index) {
        const char* local_name = lua_getlocal(state, &frame, local_index);
        if (local_name == nullptr) {
          break;
        }

        const std::string_view name(local_name);
        if (!IsTemporaryLocal(name)) {
          locals.push_back(FormatLocal(state, name, -1));
        }
        lua_pop(state, 1);
      }

      for (int vararg_index = 1; frame.isvararg != 0 && vararg_index <= kMaximumVarargsPerFrame && !report.IsTruncated(); ++vararg_index) {
        const char* vararg_name = lua_getlocal(state, &frame, -vararg_index);
        if (vararg_name == nullptr) {
          break;
        }

        const std::string name = fmt::format("...{}", vararg_index);
        locals.push_back(FormatLocal(state, name, -1));
        lua_pop(state, 1);
      }

      if (locals.empty()) {
        continue;
      }
      if (!wrote_locals_header) {
        wrote_locals_header = report.AppendLine(kLocalsHeader);
      }
      if (wrote_locals_frame) {
        report.AppendLine("");
      }
      report.AppendLine(LocalsFrameLine(frame));
      wrote_locals_frame = true;
      for (const std::string& local : locals) {
        if (!report.AppendLine(local)) {
          break;
        }
      }
    }

    const std::string result = report.Finish();
    lua_pushlstring(state, result.data(), result.size());
    return 1;
  } catch (...) {
    lua_pushliteral(state, "Unknown Lua error\ncallstack:\n\t<failed to capture Lua diagnostics>");
    return 1;
  }
}

std::string SanitizeContextValue(std::string_view value) {
  std::string sanitized;
  sanitized.reserve(value.size());
  for (char ch : value) {
    switch (ch) {
      case '\r':
        sanitized.append("\\r");
        break;
      case '\n':
        sanitized.append("\\n");
        break;
      case '\t':
        sanitized.append("\\t");
        break;
      case '\'':
        sanitized.append("\\'");
        break;
      default:
        sanitized.push_back(ch);
        break;
    }
  }
  return sanitized;
}

std::vector<std::string_view> SplitLines(std::string_view text) {
  std::vector<std::string_view> lines;
  while (!text.empty()) {
    const std::size_t newline = text.find('\n');
    std::string_view line = newline == std::string_view::npos ? text : text.substr(0, newline);
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    lines.push_back(line);
    if (newline == std::string_view::npos) {
      break;
    }
    text.remove_prefix(newline + 1);
  }
  return lines;
}

void LogError(std::string_view category, std::string_view error, const ErrorContext& context) {
  const auto lines = SplitLines(error);
  if (lines.empty()) {
    SPDLOG_ERROR("[lua] {}: unknown error", category);
    return;
  }

  std::string output = fmt::format("{}: {}", category, lines.front());
  std::optional<std::string> unnamed_lifecycle_description;
  const std::string lifecycle_description = fmt::format("in function '{}'", SanitizeContextValue(context.subject));
  for (std::size_t index = 1; index < lines.size(); ++index) {
    std::string line(lines[index]);
    if (context.operation == "lifecycle hook" && !context.subject.empty()) {
      if (unnamed_lifecycle_description) {
        const std::size_t description = line.find(*unnamed_lifecycle_description);
        if (description != std::string::npos) {
          line.replace(description, unnamed_lifecycle_description->size(), lifecycle_description);
        }
      } else if (line.find("\t[C]:") == std::string::npos) {
        const std::size_t anonymous = line.find("in anonymous function");
        const std::size_t unnamed = line.find("in function <");
        const std::size_t description = anonymous != std::string::npos ? anonymous : unnamed;
        if (description != std::string::npos) {
          unnamed_lifecycle_description = line.substr(description);
          line.replace(description, unnamed_lifecycle_description->size(), lifecycle_description);
        }
      }
    }
    output.push_back('\n');
    output.append(line);
  }
  SPDLOG_ERROR("[lua] {}", output);
}

}  // namespace

void InstallErrorHandler(sol::state& lua) {
  lua_State* state = lua.lua_state();
  lua_pushcfunction(state, &CaptureError);
  sol::reference handler(state, -1);
  lua_pop(state, 1);
  sol::protected_function::set_default_handler(handler);
}

void LogRuntimeError(std::string_view error, const ErrorContext& context) {
  LogError("Runtime error", error, context);
}

void LogSyntaxError(std::string_view error, const ErrorContext& context) {
  LogError("Syntax error", error, context);
}

}  // namespace lua::diagnostics
