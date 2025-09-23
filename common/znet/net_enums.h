
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

namespace Net {
enum PacketReliability { UNRELIABLE, UNRELIABLE_SEQUENCED, RELIABLE, RELIABLE_ORDERED, RELIABLE_SEQUENCED };

enum PacketPriority {
  IMMEDIATE_PRIORITY,
  HIGH_PRIORITY,
  MEDIUM_PRIORITY,
  LOW_PRIORITY,
};

enum PacketID {
  WL_PREPARE_TO_JOIN,
  WL_JOIN_TO_GAME,
  WL_INGAME,
  ID_CONNECTION_ATTEMPT_FAILED = 17,
  ID_ALREADY_CONNECTED,
  ID_NEW_INCOMING_CONNECTION,
  ID_NO_FREE_INCOMING_CONNECTIONS,
  ID_DISCONNECTION_NOTIFICATION,
  ID_CONNECTION_LOST,
  ID_CONNECTION_BANNED,
  ID_INVALID_PASSWORD,
  ID_INCOMPATIBLE_PROTOCOL_VERSION,
  ID_IP_RECENTLY_CONNECTED,
  ID_TIMESTAMP,
  PT_MSG = 135,
  PT_REQUEST_FILE_LENGTH,
  PT_REQUEST_FILE_PART,
  PT_INITIAL_INFO,
  PT_JOIN_GAME,
  PT_ACTUAL_STATISTICS,  // <- chyba tutaj będe musiał dodac optymalizacje gdyż nie potrzeba nam wszystkich informacji o
                         // graczu który jest od nas dalej niż 5000.0f
  PT_EXISTING_PLAYERS,         // Packet contains information about all other players. Send to the new joining player.
  PT_HP_DIFF,
  PT_MAP_ONLY,
  PT_COMMAND,  // administrowanie
  PT_WHISPER,
  PT_EXTENDED_4_SCRIPTS,  // jak juz kiedys wdrozymy skrypty
  PT_SRVMSG,
  PT_LEFT_GAME,
  PT_GAME_INFO,
  PT_DODIE,
  PT_RESPAWN,
  PT_DROPITEM,
  PT_TAKEITEM,
  PT_CASTSPELL,
  PT_CASTSPELLONTARGET,
  PT_VOICE,
  PT_DISCORD_ACTIVITY,
};

inline const char* PacketIDToString(PacketID id) {
  switch (id) {
    case WL_PREPARE_TO_JOIN:
      return "WL_PREPARE_TO_JOIN";
    case WL_JOIN_TO_GAME:
      return "WL_JOIN_TO_GAME";
    case WL_INGAME:
      return "WL_INGAME";
    case ID_CONNECTION_ATTEMPT_FAILED:
      return "ID_CONNECTION_ATTEMPT_FAILED";
    case ID_ALREADY_CONNECTED:
      return "ID_ALREADY_CONNECTED";
    case ID_NEW_INCOMING_CONNECTION:
      return "ID_NEW_INCOMING_CONNECTION";
    case ID_NO_FREE_INCOMING_CONNECTIONS:
      return "ID_NO_FREE_INCOMING_CONNECTIONS";
    case ID_DISCONNECTION_NOTIFICATION:
      return "ID_DISCONNECTION_NOTIFICATION";
    case ID_CONNECTION_LOST:
      return "ID_CONNECTION_LOST";
    case ID_CONNECTION_BANNED:
      return "ID_CONNECTION_BANNED";
    case ID_INVALID_PASSWORD:
      return "ID_INVALID_PASSWORD";
    case ID_INCOMPATIBLE_PROTOCOL_VERSION:
      return "ID_INCOMPATIBLE_PROTOCOL_VERSION";
    case ID_IP_RECENTLY_CONNECTED:
      return "ID_IP_RECENTLY_CONNECTED";
    case ID_TIMESTAMP:
      return "ID_TIMESTAMP";
    case PT_MSG:
      return "PT_MSG";
    case PT_REQUEST_FILE_LENGTH:
      return "PT_REQUEST_FILE_LENGTH";
    case PT_REQUEST_FILE_PART:
      return "PT_REQUEST_FILE_PART";
    case PT_INITIAL_INFO:
      return "PT_INITIAL_INFO";
    case PT_JOIN_GAME:
      return "PT_JOIN_GAME";
    case PT_ACTUAL_STATISTICS:
      return "PT_ACTUAL_STATISTICS";
    case PT_EXISTING_PLAYERS:
      return "PT_EXISTING_PLAYERS";
    case PT_HP_DIFF:
      return "PT_HP_DIFF";
    case PT_MAP_ONLY:
      return "PT_MAP_ONLY";
    case PT_COMMAND:
      return "PT_COMMAND";
    case PT_WHISPER:
      return "PT_WHISPER";
    case PT_EXTENDED_4_SCRIPTS:
      return "PT_EXTENDED_4_SCRIPTS";
    case PT_SRVMSG:
      return "PT_SRVMSG";
    case PT_LEFT_GAME:
      return "PT_LEFT_GAME";
    case PT_GAME_INFO:
      return "PT_GAME_INFO";
    case PT_DODIE:
      return "PT_DODIE";
    case PT_RESPAWN:
      return "PT_RESPAWN";
    case PT_DROPITEM:
      return "PT_DROPITEM";
    case PT_TAKEITEM:
      return "PT_TAKEITEM";
    case PT_CASTSPELL:
      return "PT_CASTSPELL";
    case PT_CASTSPELLONTARGET:
      return "PT_CASTSPELLONTARGET";
    case PT_VOICE:
      return "PT_VOICE";
    case PT_DISCORD_ACTIVITY:
      return "PT_DISCORD_ACTIVITY";
  }
  return "UNKNOWN";
}

}  // namespace Net
