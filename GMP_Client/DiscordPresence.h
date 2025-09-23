
/*
MIT License

Copyright (c) 2025 Gothic Multiplayer Team.

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

#include <memory>
#include <string>

#ifndef DISCORD_RICH_PRESENCE_ENABLED
#define DISCORD_RICH_PRESENCE_ENABLED 0
#endif

#ifndef DISCORD_APPLICATION_ID
#define DISCORD_APPLICATION_ID 0
#endif

namespace discord {
class Core;
struct Activity;
}  // namespace discord

namespace DiscordGMP {
bool Initialize(long long clientId);

void Shutdown();

void PumpCallbacks();

void UpdateActivity(const std::string& state, const std::string& details, int64_t startTimestamp = 0, int64_t endTimestamp = 0,
                    const std::string& largeImageKey = "", const std::string& largeImageText = "", const std::string& smallImageKey = "",
                    const std::string& smallImageText = "");

void ClearActivity();
}  // namespace DiscordGMP