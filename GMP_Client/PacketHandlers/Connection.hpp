
/*
MIT License

Copyright (c) 2022 Gothic Multiplayer Team.

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

#include "../CChat.h"
#include "../CGmpClient.h"
#include "CLocalPlayer.h"

extern CLocalPlayer* LocalPlayer;
namespace Connection {
void OnWhoami(CGmpClient* client, Packet packet) {
  uint64_t playerID;
  memcpy(&playerID, packet.data + 1, sizeof(uint64_t));
  client->network->UpdateMyId(playerID);
}

void OnDisconnectOrLostConnection(CGmpClient* client, Packet packet) {
  client->network->error = packet.data[0];
  SPDLOG_WARN("OnDisconnectOrLostConnection, code: {}", client->network->error);
  client->Disconnect();
  auto pos = oCNpc::GetHero()->GetPosition();
  oCNpc::GetHero()->ResetPos(pos);
  client->network->connection_lost_ = true;
  client->IsInGame = false;
  client->IsReadyToJoin = false;
  CChat::GetInstance()->WriteMessage(NORMAL, false, zCOLOR(255, 0, 0, 255), "%s",
                                     (*client->lang)[CLanguage::DISCONNECTED].ToChar());
}
}  // namespace Connection