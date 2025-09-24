
/*
MIT License

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

#include "function_bind.h"

#include <spdlog/spdlog.h>

#include "game_server.h"

#include <fstream>
using namespace std;

namespace {

sol::optional<std::string> GetOptionalString(const sol::table& table, const char* lowerKey, const char* upperKey) {
  if (auto value = table.get<sol::optional<std::string>>(lowerKey); value) {
    return value;
  }
  return table.get<sol::optional<std::string>>(upperKey);
}

}  // namespace


// Functions
int Function_Log(string name, string text) {
  ofstream logfile;
  logfile.open(name, std::ios_base::app);
  if (logfile.is_open()) {
    logfile << text << "\n";
    logfile.close();
  }
  return 0;
}

void Function_SetDiscordActivity(const sol::table& params) {
  if (!g_server) {
    SPDLOG_WARN("Cannot update Discord activity before the server is initialized");
    return;
  }

  auto activity = g_server->GetDiscordActivity();

  if (auto state = GetOptionalString(params, "state", "State")) {
    activity.state = *state;
  }
  if (auto details = GetOptionalString(params, "details", "Details")) {
    activity.details = *details;
  }
  if (auto largeImageKey = GetOptionalString(params, "largeImageKey", "LargeImageKey")) {
    activity.large_image_key = *largeImageKey;
  }
  if (auto largeImageText = GetOptionalString(params, "largeImageText", "LargeImageText")) {
    activity.large_image_text = *largeImageText;
  }
  if (auto smallImageKey = GetOptionalString(params, "smallImageKey", "SmallImageKey")) {
    activity.small_image_key = *smallImageKey;
  }
  if (auto smallImageText = GetOptionalString(params, "smallImageText", "SmallImageText")) {
    activity.small_image_text = *smallImageText;
  }

  g_server->UpdateDiscordActivity(activity);
}

void Function_SendServerMessage(const std::string& message) {
  if (!g_server) {
    SPDLOG_WARN("Cannot send server message before the server is initialized");
    return;
  }

  g_server->SendServerMessage(message);
}

// Register Functions
void lua::bindings::BindFunctions(sol::state& lua) {
  lua["Log"] = Function_Log;
  lua["SetDiscordActivity"] = Function_SetDiscordActivity;
  lua["SendServerMessage"] = Function_SendServerMessage;
}