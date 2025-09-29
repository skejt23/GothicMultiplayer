
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

#include "..\CObjectMenu.h"
#include "load.h"

enum CurrentMode { EDITING, TESTING, FREEROAM, OBJECTMENU };
enum SpawnMode { MOBS, PARTICLES, ITEMS };

void RenderEvent(void);

class CBuilder {
private:
  zCVisual* TargetVisual;  // ASC, MDS or 3DS
  oCMobInter* CurrentInter;
  zCAICamera* cam;
  float CamDistHuman;
  float CamDistEditor;
  std::vector<zSTRING> BuildMessages;
  std::vector<zSTRING> MobNames;
  std::vector<zSTRING> ParticleNames;
  zSTRING PlacedTest;
  zSTRING VisualName;
  int activemobname;
  int activeparticlename;
  int activeitemname;
  zVEC3 LastPos;
  zVEC3 LastTrafo;
  zSTRING TextPosPrint;
  zSTRING IdText;
  zVEC3 CurPos;
  zVEC3 CurAngle;
  int DirectionAngle;
  char buffer[32];
  std::vector<zCVob*> VobsInWorld;
  CurrentMode Mode;
  SpawnMode SMode;
  zCView* MobsWin;
  zCView* ParticleWin;
  zCView* ItemsWin;
  Info info;
  float MoveSpeed;
  bool MobCollision;
  CObjectMenu* ObjMenu;
  unsigned char CurrentMobType;
  zMAT4 LastMatrix;

public:
  std::vector<Info> SpawnedVobs;

  CBuilder();
  ~CBuilder();
  void Render();
  double CalculateRadians(double degree);
  void SpawnObject();
  void PrintItemIds();
  void ClearAfterObjMenu();
  void ClearUpBoxes();
  void CreateModeBoxes();
  void CollisionInObjectsEnabled(bool isit);
  void LoadAllVobsFromTheWorld();
  void CheckMsgSize();
  void FillMobNames();
  void FillParticleNames();
  void DeleteAllNpcs();
  void DisableAllItemsInWorld();
  void RecreateMobInstance(bool RemoveOld, bool DoNotSave);
  static void SaveVobMatrix(zCVob* Vob, zMAT4& Matrix);
  static void RestoreVobMatrix(zCVob* Vob, zMAT4& Matrix);
};