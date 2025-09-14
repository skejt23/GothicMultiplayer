
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

#pragma warning(disable : 4996)
#include "save.h"

using namespace std;
vector<unsigned char> wb_data;

BOOL SaveWorld::FolderExists(LPCTSTR FolderName) {
  return GetFileAttributes(FolderName) != INVALID_FILE_ATTRIBUTES;
}

int SaveWorld::SaveBuilderMap(vector<Info>& Vobs, const char* MapName) {
  if (Vobs.size() < 1)
    return 0;
  wb_data.clear();
  if (!FolderExists(".\\Multiplayer\\WorldBuilder\\Maps")) {
    LPSECURITY_ATTRIBUTES attr;
    attr = NULL;
    // CreateDirectory(L".\\Multiplayer\\WorldBuilder", attr);
    CreateDirectory(".\\Multiplayer\\WorldBuilder\\Maps", attr);
  }
  int size;
  zSTRING tempz;
  string temp;
  temp = ogame->GetGameWorld()->GetWorldFilename().ToChar();
  size = temp.size();
  wb_data.push_back(size);
  for (int l = 0; l < size; l++) {
    wb_data.push_back((unsigned char)temp[l]);
  }
  temp.clear();
  for (int i = 0; i < (int)Vobs.size(); i++) {
    if (Vobs[i].Vob) {
      switch (Vobs[i].Type) {
        case TYPE_MOB:
          wb_data.push_back(0xF1);
          break;
        case TYPE_PARTICLE:
          wb_data.push_back(0xF2);
          break;
        case TYPE_MOBNOCOLLIDE:
          wb_data.push_back(0xF3);
          break;
      };
      size = Vobs[i].VisualName.size();
      wb_data.push_back(size);
      for (int l = 0; l < size; l++) {
        wb_data.push_back((unsigned char)Vobs[i].VisualName[l]);
      }
      zVEC3 Pos = Vobs[i].Vob->GetPositionWorld();
      unsigned char* fx = (unsigned char*)&Pos[VX];
      unsigned char* fy = (unsigned char*)&Pos[VY];
      unsigned char* fz = (unsigned char*)&Pos[VZ];
      wb_data.push_back(fx[0]);
      wb_data.push_back(fx[1]);
      wb_data.push_back(fx[2]);
      wb_data.push_back(fx[3]);
      wb_data.push_back(fy[0]);
      wb_data.push_back(fy[1]);
      wb_data.push_back(fy[2]);
      wb_data.push_back(fy[3]);
      wb_data.push_back(fz[0]);
      wb_data.push_back(fz[1]);
      wb_data.push_back(fz[2]);
      wb_data.push_back(fz[3]);
      unsigned char* fr = (unsigned char*)&Vobs[i].Vob->trafoObjToWorld.v[0][0];
      wb_data.push_back(fr[0]);
      wb_data.push_back(fr[1]);
      wb_data.push_back(fr[2]);
      wb_data.push_back(fr[3]);
      fr = (unsigned char*)&Vobs[i].Vob->trafoObjToWorld.v[1][0];
      wb_data.push_back(fr[0]);
      wb_data.push_back(fr[1]);
      wb_data.push_back(fr[2]);
      wb_data.push_back(fr[3]);
      fr = (unsigned char*)&Vobs[i].Vob->trafoObjToWorld.v[2][0];
      wb_data.push_back(fr[0]);
      wb_data.push_back(fr[1]);
      wb_data.push_back(fr[2]);
      wb_data.push_back(fr[3]);
      fr = (unsigned char*)&Vobs[i].Vob->trafoObjToWorld.v[0][2];
      wb_data.push_back(fr[0]);
      wb_data.push_back(fr[1]);
      wb_data.push_back(fr[2]);
      wb_data.push_back(fr[3]);
      fr = (unsigned char*)&Vobs[i].Vob->trafoObjToWorld.v[1][2];
      wb_data.push_back(fr[0]);
      wb_data.push_back(fr[1]);
      wb_data.push_back(fr[2]);
      wb_data.push_back(fr[3]);
      fr = (unsigned char*)&Vobs[i].Vob->trafoObjToWorld.v[2][2];
      wb_data.push_back(fr[0]);
      wb_data.push_back(fr[1]);
      wb_data.push_back(fr[2]);
      wb_data.push_back(fr[3]);
      if (Vobs[i].Type != TYPE_PARTICLE) {
        fr = (unsigned char*)&Vobs[i].Vob->trafoObjToWorld.v[0][1];
        wb_data.push_back(fr[0]);
        wb_data.push_back(fr[1]);
        wb_data.push_back(fr[2]);
        wb_data.push_back(fr[3]);
        fr = (unsigned char*)&Vobs[i].Vob->trafoObjToWorld.v[1][1];
        wb_data.push_back(fr[0]);
        wb_data.push_back(fr[1]);
        wb_data.push_back(fr[2]);
        wb_data.push_back(fr[3]);
        fr = (unsigned char*)&Vobs[i].Vob->trafoObjToWorld.v[2][1];
        wb_data.push_back(fr[0]);
        wb_data.push_back(fr[1]);
        wb_data.push_back(fr[2]);
        wb_data.push_back(fr[3]);
        if (Vobs[i].MType != MOB_NORMAL) {
          if (Vobs[i].MType == MOB_DOOR || Vobs[i].MType == MOB_LADDER || Vobs[i].MType == MOB_CONTAINER) {
            printf("\nPushing, MType : %d", Vobs[i].MType);
            wb_data.push_back(Vobs[i].MType);
          }
        }
      }
    }
  }
  const char* FileName = ".\\Multiplayer\\WorldBuilder\\Maps\\";
  char buffer[128];
  tempz = MapName;
  tempz.Upper();
  sprintf(buffer, "%s%s.WBM", FileName, tempz.ToChar());
  FILE* file = fopen(buffer, "wb");
  fwrite(&wb_data[0], 1, wb_data.size(), file);
  fclose(file);
  tempz.Clear();
  return 1;
};