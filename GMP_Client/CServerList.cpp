
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

#pragma warning(disable : 4101)

#include <httplib.h>

#include "CServerList.h"

#define HTTP_DOMAIN "www.your-site.com"
#define HTTP_LIST_FILE "lista.php"
#define UDP_LIST_ADDRESS "??"  //<- nasz VPS

const char *LIST_CONST_PREFIX = "gmp_list";
#define PREFIX_SIZE 8

CServerList::~CServerList(void) {
  if (!this->server_vector.empty())
    this->server_vector.clear();
}

size_t CServerList::GetListSize() {
  return this->server_vector.size();
}

SServerInfo *CServerList::At(size_t pos) {
  if (pos >= this->server_vector.size())
    return NULL;
  return (SServerInfo *)&this->server_vector[pos];
}

void CServerList::Parse() {
  if (data.empty())
    return;
  if (!server_vector.empty())
    server_vector.clear();
  switch (list_type) {
    case HTTP_LIST: {
      char work_buff[256];
      int count = 0;
      size_t pos = 0;
      sscanf(data.c_str(), "%s%d", work_buff, &count);
      if (memcmp(work_buff, LIST_CONST_PREFIX, strlen(LIST_CONST_PREFIX)) || (count == 0)) {
        data.clear();
        return;
      }
      pos = data.find_first_of(' ', PREFIX_SIZE + 1) + 1;
      SServerInfo tmp_sv_inf;
      char chBuffer[3][128];
      do {
        memset(chBuffer[0], 0, 3 * 128);
        tmp_sv_inf.Clear();
        sscanf(data.c_str() + pos, "%s%s%hu%hu%s%hu", chBuffer[0], chBuffer[1], &tmp_sv_inf.num_of_players, &tmp_sv_inf.max_players, chBuffer[2],
               &tmp_sv_inf.port);
        for (int i = 0; i < 128; i++) {
          if (chBuffer[0][i] == 7)
            chBuffer[0][i] = 0x20;
          if (chBuffer[1][i] == 7)
            chBuffer[1][i] = 0x20;
          if (chBuffer[2][i] == 7)
            chBuffer[2][i] = 0x20;
        }
        tmp_sv_inf.ip = chBuffer[0];
        tmp_sv_inf.name = chBuffer[1];
        tmp_sv_inf.map = chBuffer[2];
        server_vector.push_back(tmp_sv_inf);
        for (int i = 0; i < 6; i++) pos = data.find_first_of(' ', pos) + 1;
      } while (--count);
    } break;
    case UDP_LIST:
      break;
    default:
      printf("SNE exception executed!\n");
      // should never execute
      break;
  }
  data.clear();
}

bool CServerList::ReceiveListHttp() {
  httplib::Client cli(HTTP_DOMAIN);
  auto res = cli.Get(HTTP_LIST_FILE);
  data = res.value().body;

  error_code = 0;
  list_type = HTTP_LIST;
  Parse();
  return true;
}

bool CServerList::ReceiveListUDP() {
  return true;
}

const char *CServerList::GetLastError() {
  char *ret_val = NULL;
  return (const char *)ret_val;
}

void SServerInfo::Clear() {
  if (!this->ip.IsEmpty())
    this->ip.Clear();
  if (!this->name.IsEmpty())
    this->name.Clear();
  if (!this->map.IsEmpty())
    this->map.Clear();
  if (!this->server_website.IsEmpty())
    this->map.Clear();
}