
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
#include <string>
#include <vector>

#include "ZenGin/zGothicAPI.h"

struct SServerInfo {
  Gothic_II_Addon::zSTRING name, ip, map, server_website;
  unsigned short num_of_players, max_players, hour, minute, port;
  void Clear();
};

class CServerList {
private:
  unsigned char list_type;
  unsigned char error_code;
  std::string data;

  std::vector<SServerInfo> server_vector;

  enum {
    HTTP_LIST = 0,
    UDP_LIST = 1,  // lista z master serwera
  };

  void Parse();

public:
  ~CServerList(void);
  bool ReceiveListHttp(void);
  bool ReceiveListUDP(void);
  size_t GetListSize(void);
  SServerInfo* At(size_t pos);
  const char* GetLastError(void);
};
