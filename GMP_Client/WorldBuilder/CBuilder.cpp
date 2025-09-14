
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
#pragma warning(disable : 4244)
#pragma once

// Includes
#include "CBuilder.h"

#include <fstream>
#include <iostream>
#include <vector>

#include "..\HooksManager.h"
#include "..\patch.h"
#include "math.h"
//

// Global
using namespace std;
#define PI 3.14159265
extern bool WritingMapSave;
constexpr const char* fhuge = "FONT_OLD_20_WHITE.TGA";
constexpr const char* placed = " Placed.";
constexpr const char* PlayerSpawned = "Player spawned in vob position.";
extern CBuilder* Builder;
//
void RenderEvent() {
  if (Builder)
    Builder->Render();
};

CBuilder::CBuilder()  // Constructor
{
  MobsWin = NULL;
  ParticleWin = NULL;
  ItemsWin = NULL;
  ObjMenu = NULL;
  FillMobNames();
  FillParticleNames();
  SMode = MOBS;
  CurrentMobType = MOB_NORMAL;
  CreateModeBoxes();
  DeleteAllNpcs();
  DirectionAngle = 90;
  DisableAllItemsInWorld();
  CurrentInter = new oCMobInter();
  VisualName = MobNames[0];
  activeparticlename = 0, activemobname = 0, MoveSpeed = 1;
  LoadAllVobsFromTheWorld();
  TargetVisual = zCVisual::LoadVisual(MobNames[0]);
  CurrentInter->SetVisual(TargetVisual);
  player->SetPositionWorld(zVEC3(0, 0, 0));
  player->SetMovLock(1);
  ogame->GetWorld()->AddVob(CurrentInter);
  cam = zCAICamera::GetCurrent();
  cam->SetTarget(CurrentInter);
  oCNpc::godmode = 1;
  Patch::SetLookingOnNpcCamera(true);
  ogame->GetWorld()->vobFarClipZ = 30000;
  ogame->GetWorld()->skyControlerOutdoor->SetFarZScalability(3);
  CamDistHuman = cam->camDistOffset;
  cam->maxRange = 30.0f;
  cam->camDistOffset = 7.0f;
  MobCollision = true;
  Patch::PlayerInterfaceEnabled(false);
  BuildMessages.push_back(zSTRING("World Builder 0.7 Alpha By Skejt23"));
  Mode = EDITING;
  HooksManager::GetInstance()->AddHook(HT_RENDER, (DWORD)RenderEvent, false);
};

CBuilder::~CBuilder() {
  oCNpc::godmode = 0;
  ClearUpBoxes();
  Patch::SetLookingOnNpcCamera(false);
  cam->SetTarget(player);
  CurrentInter->RemoveVobFromWorld();
  for (int i = 0; i < (int)SpawnedVobs.size(); i++) {
    SpawnedVobs[i].Vob->RemoveVobFromWorld();
  }
  SpawnedVobs.clear();
};

void CBuilder::Render() {
  if (!this)
    return;
  Patch::PlayerInterfaceEnabled(false);
  screen->SetFont("FONT_DEFAULT.TGA");
  if ((int)BuildMessages.size() > 0)
    for (int v = 0; v < (int)BuildMessages.size(); v++) screen->Print(0, v * 200, BuildMessages[v]);
  switch (Mode) {
    case EDITING:  // EDITING START
      if (zinput->KeyToggled(KEY_T) && !WritingMapSave) {
        Mode = TESTING;
        LastPos = CurrentInter->GetPositionWorld();
        SaveVobMatrix(CurrentInter, LastMatrix);
        player->SetMovLock(0);
        ClearUpBoxes();
        Patch::SetLookingOnNpcCamera(false);
        CollisionInObjectsEnabled(true);
        CheckMsgSize();
        BuildMessages.push_back(zSTRING("Test is starting!"));
        CamDistEditor = cam->camDistOffset;
        cam->camDistOffset = CamDistHuman;
        cam->SetTarget(player);
        CurrentInter->RemoveVobFromWorld();
      }
      if (zinput->KeyToggled(KEY_F1)) {
        if (SpawnedVobs.size() > 0) {
          CheckMsgSize();
          sprintf(buffer, "Undo. Removed : %s", SpawnedVobs.back().VisualName.c_str());
          PlacedTest = buffer;
          BuildMessages.push_back(PlacedTest);
          PlacedTest.Clear();
          SpawnedVobs.back().Vob->RemoveVobFromWorld();
          SpawnedVobs.erase(SpawnedVobs.end() - 1);
        }
      }
      if (zinput->KeyToggled(KEY_F2)) {
        if (SpawnedVobs.size() > 0) {
          ObjMenu = new CObjectMenu();
          Mode = OBJECTMENU;
          LastPos = CurrentInter->GetPositionWorld();
          SaveVobMatrix(CurrentInter, LastMatrix);
          CurrentInter->RemoveVobFromWorld();
          ClearUpBoxes();
        }
      }
      if (zinput->KeyToggled(KEY_HOME)) {
        if (CurrentMobType >= MOB_DOOR)
          CurrentMobType = MOB_NORMAL;
        else
          CurrentMobType++;
        RecreateMobInstance(true, false);
        cam->SetTarget(CurrentInter);
      }
      if (zinput->KeyToggled(KEY_END)) {
        MobCollision = !MobCollision;
      }
      if (zinput->KeyPressed(KEY_NUMPAD8)) {
        CurrentInter->RotateWorldZ(1);
      }
      if (zinput->KeyPressed(KEY_NUMPAD2)) {
        CurrentInter->RotateWorldZ(-1);
      }
      if (zinput->KeyPressed(KEY_NUMPAD4)) {
        CurrentInter->RotateWorldX(1);
      }
      if (zinput->KeyPressed(KEY_NUMPAD6)) {
        CurrentInter->RotateWorldX(-1);
      }
      if (zinput->KeyToggled(KEY_NUMPAD1)) {
        if (MoveSpeed > 1)
          MoveSpeed--;
        else if (MoveSpeed > 0.25f && MoveSpeed <= 1)
          MoveSpeed -= 0.25f;
      }
      if (zinput->KeyToggled(KEY_NUMPAD3)) {
        if (MoveSpeed < 6 && MoveSpeed >= 1)
          MoveSpeed++;
        else if (MoveSpeed >= 0.25f && MoveSpeed < 1)
          MoveSpeed += 0.25f;
      }
      if (zinput->KeyPressed(KEY_LEFT)) {
        DirectionAngle++;
        CurrentInter->RotateWorldY(-1);
      }
      if (zinput->KeyPressed(KEY_RIGHT)) {
        DirectionAngle--;
        CurrentInter->RotateWorldY(1);
      }
      if (zinput->KeyPressed(KEY_UP)) {
        double radian = CalculateRadians(DirectionAngle);
        float x, y;
        if (cam->camDistOffset < 7) {
          x = ((float)cosf(radian) * 6) * MoveSpeed;
          y = ((float)sinf(radian) * 6) * MoveSpeed;
        } else {
          x = ((float)cosf(radian) * cam->camDistOffset + 1) * MoveSpeed;
          y = ((float)sinf(radian) * cam->camDistOffset + 1) * MoveSpeed;
        }
        CurrentInter->MoveWorld(x, 0, y);
      }
      if (zinput->KeyPressed(KEY_DOWN)) {
        double radian = CalculateRadians(DirectionAngle);
        float x, y;
        if (cam->camDistOffset < 7) {
          x = (-(float)cosf(radian) * 6) * MoveSpeed;
          y = (-(float)sinf(radian) * 6) * MoveSpeed;
        } else {
          x = (-(float)cosf(radian) * cam->camDistOffset + 1) * MoveSpeed;
          y = (-(float)sinf(radian) * cam->camDistOffset + 1) * MoveSpeed;
        }
        CurrentInter->MoveWorld(x, 0, y);
      }
      if (zinput->KeyPressed(KEY_DELETE)) {
        double radian = CalculateRadians(DirectionAngle + 90);
        float x, y;
        if (cam->camDistOffset < 7) {
          x = ((float)cosf(radian) * 6) * MoveSpeed;
          y = ((float)sinf(radian) * 6) * MoveSpeed;
        } else {
          x = ((float)cosf(radian) * cam->camDistOffset + 1) * MoveSpeed;
          y = ((float)sinf(radian) * cam->camDistOffset + 1) * MoveSpeed;
        }
        CurrentInter->MoveWorld(x, 0, y);
      }
      if (zinput->KeyPressed(KEY_PGDN)) {
        double radian = CalculateRadians(DirectionAngle - 90);
        float x, y;
        if (cam->camDistOffset < 7) {
          x = ((float)cosf(radian) * 6) * MoveSpeed;
          y = ((float)sinf(radian) * 6) * MoveSpeed;
        } else {
          x = ((float)cosf(radian) * cam->camDistOffset + 1) * MoveSpeed;
          y = ((float)sinf(radian) * cam->camDistOffset + 1) * MoveSpeed;
        }
        CurrentInter->MoveWorld(x, 0, y);
      }
      if (zinput->KeyPressed(KEY_X)) {
        short y = 6 * MoveSpeed;
        CurrentInter->MoveWorld(0, y, 0);
      }
      if (zinput->KeyPressed(KEY_Z)) {
        short y = -6 * MoveSpeed;
        CurrentInter->MoveWorld(0, y, 0);
      }
      if (zinput->KeyToggled(KEY_G)) {
        auto pos = CurrentInter->GetPositionWorld();
        player->ResetPos(pos);
        player->SetMovLock(1);
        CheckMsgSize();
        BuildMessages.push_back(PlayerSpawned);
      }
      if (zinput->KeyToggled(KEY_NUMPAD0)) {
        CurrentInter->ResetRotationsWorld();
        zCAICamera::GetCurrent()->bestRotY = 90.0f;
        DirectionAngle = 90;
        CheckMsgSize();
        BuildMessages.push_back(zSTRING("Rotation has been reset."));
      }
      if (zinput->KeyToggled(KEY_Q)) {
        switch (SMode) {
          case MOBS:
            activemobname--;
            if (activemobname < 0)
              activemobname = (int)MobNames.size() - 1;
            VisualName = MobNames[activemobname];
            TargetVisual = zCVisual::LoadVisual(MobNames[activemobname]);
            CurrentInter->SetVisual(TargetVisual);
            break;
          case PARTICLES:
            activeparticlename--;
            if (activeparticlename < 0)
              activeparticlename = (int)ParticleNames.size() - 1;
            VisualName = ParticleNames[activeparticlename];
            TargetVisual = zCParticleFX::Load(ParticleNames[activeparticlename]);
            zCParticleFX* ParTemp = (zCParticleFX*)TargetVisual;
            if (!ParTemp->emitter->ppsIsLooping)
              ParTemp->emitter->ppsIsLooping = 1;
            CurrentInter->SetVisual(TargetVisual);
        }
      }
      if (zinput->KeyToggled(KEY_E)) {
        switch (SMode) {
          case MOBS:
            activemobname++;
            if (activemobname > (int)MobNames.size() - 1)
              activemobname = 0;
            VisualName = MobNames[activemobname];
            TargetVisual = zCVisual::LoadVisual(MobNames[activemobname]);
            CurrentInter->SetVisual(TargetVisual);
            break;
          case PARTICLES:
            activeparticlename++;
            if (activeparticlename > (int)ParticleNames.size() - 1)
              activeparticlename = 0;
            VisualName = ParticleNames[activeparticlename];
            TargetVisual = zCParticleFX::Load(ParticleNames[activeparticlename]);
            zCParticleFX* ParTemp = (zCParticleFX*)TargetVisual;
            if (!ParTemp->emitter->ppsIsLooping)
              ParTemp->emitter->ppsIsLooping = 1;
            CurrentInter->SetVisual(TargetVisual);
            break;
        }
      }
      if (zinput->KeyToggled(KEY_NUMPADENTER) || zinput->KeyToggled(KEY_S)) {
        SpawnObject();
      }
      if (zinput->KeyToggled(KEY_F5)) {
        if (SMode != MOBS) {
          SMode = MOBS;
          CreateModeBoxes();
          VisualName = MobNames[activemobname];
          TargetVisual = zCVisual::LoadVisual(MobNames[activemobname]);
          CurrentInter->SetVisual(TargetVisual);
        }
      }
      if (zinput->KeyToggled(KEY_F6)) {
        if (SMode != PARTICLES) {
          SMode = PARTICLES;
          CreateModeBoxes();
          VisualName = ParticleNames[activeparticlename];
          TargetVisual = zCParticleFX::Load(ParticleNames[activeparticlename]);
          zCParticleFX* ParTemp = (zCParticleFX*)TargetVisual;
          if (!ParTemp->emitter->ppsIsLooping)
            ParTemp->emitter->ppsIsLooping = 1;
          CurrentInter->SetVisual(TargetVisual);
        }
      }
      /*if(zinput->KeyToggled(KEY_F7)){
              if(SMode != ITEMS){
                      SMode = ITEMS;
                      CreateModeBoxes();
              }
      }*/
      // Loop things
      cam->maxRange = 30.0f;
      CurPos = CurrentInter->GetPositionWorld();
      PrintItemIds();
      break;       // EDITING END
    case TESTING:  // TESTING START
      if (zinput->KeyToggled(KEY_T) && !WritingMapSave) {
        player->SetMovLock(1);
        player->SetCollDet(0);
        Mode = EDITING;
        CreateModeBoxes();
        Patch::SetLookingOnNpcCamera(true);
        CollisionInObjectsEnabled(false);
        RecreateMobInstance(false, true);
        CamDistHuman = cam->camDistOffset;
        cam->SetTarget(CurrentInter);
        cam->camDistOffset = CamDistEditor;
        CheckMsgSize();
        BuildMessages.push_back(zSTRING("Test has ended."));
      }
      // Loop things
      CurPos = player->GetPositionWorld();
      cam->maxRange = 10.0f;
      break;  // TESTING END
    case OBJECTMENU:
      if (ObjMenu)
        ObjMenu->Draw();
      if (zinput->KeyToggled(KEY_F2) || zinput->KeyPressed(KEY_ESCAPE)) {
        zinput->ClearKeyBuffer();
        ClearAfterObjMenu();
      }
      break;  // OBJECTMENU END
  }
  if (Mode != OBJECTMENU) {
    sprintf_s(buffer, "X : %d", static_cast<int>(CurPos[VX]));
    TextPosPrint = buffer;
    screen->Print(6300, 3100, TextPosPrint);
    sprintf_s(buffer, "Z : %d", static_cast<int>(CurPos[VY]));
    TextPosPrint = buffer;
    screen->Print(6300, 3300, TextPosPrint);
    sprintf_s(buffer, "Y : %d", static_cast<int>(CurPos[VZ]));
    TextPosPrint = buffer;
    screen->Print(6300, 3500, TextPosPrint);
    sprintf_s(buffer, "Speed: %4.2fx", MoveSpeed);
    TextPosPrint = buffer;
    screen->Print(6300, 3700, TextPosPrint);
    if (MobCollision)
      TextPosPrint = "Mob Collision: ON";
    else
      TextPosPrint = "Mob Collision: OFF";
    screen->Print(6300, 3900, TextPosPrint);
    switch (CurrentMobType) {
      case MOB_NORMAL:
        TextPosPrint = "Mob Type: NORMAL";
        screen->Print(6300, 4100, TextPosPrint);
        break;
      case MOB_DOOR:
        TextPosPrint = "Mob Type: DOOR";
        screen->Print(6300, 4100, TextPosPrint);
        break;
      case MOB_CONTAINER:
        TextPosPrint = "Mob Type: CONTAINER";
        screen->Print(6300, 4100, TextPosPrint);
        break;
      case MOB_LADDER:
        TextPosPrint = "Mob Type: LADDER";
        screen->Print(6300, 4100, TextPosPrint);
        break;
    };
  }
};

double CBuilder::CalculateRadians(double degree) {
  double radian = 0;
  radian = degree * (PI / 180);
  return radian;
};

void CBuilder::RecreateMobInstance(bool RemoveOld, bool DoNotSave) {
  if (!DoNotSave) {
    LastPos = CurrentInter->GetPositionWorld();
    SaveVobMatrix(CurrentInter, LastMatrix);
    if (RemoveOld)
      CurrentInter->RemoveVobFromWorld();
  }
  switch (CurrentMobType) {
    case MOB_NORMAL:
      CurrentInter = new oCMobInter;
      break;
    case MOB_DOOR:
      CurrentInter = new oCMobDoor;
      break;
    case MOB_CONTAINER:
      CurrentInter = new oCMobContainer;
      break;
    case MOB_LADDER:
      CurrentInter = new oCMobLadder;
      break;
  };
  CurrentInter->SetVisual(TargetVisual);
  ogame->GetWorld()->AddVob(CurrentInter);
  CurrentInter->SetPositionWorld(LastPos);
  RestoreVobMatrix(CurrentInter, LastMatrix);
};
void CBuilder::SpawnObject() {
  switch (SMode) {
    case MOBS:
      CheckMsgSize();
      PlacedTest = MobNames[activemobname];
      PlacedTest = PlacedTest + placed;
      BuildMessages.push_back(PlacedTest);
      PlacedTest.Clear();
      LastPos = CurrentInter->GetPositionWorld();
      info.VisualName = MobNames[activemobname].ToChar();
      info.Vob = CurrentInter;
      if (MobCollision)
        info.Type = TYPE_MOB;
      else
        info.Type = TYPE_MOBNOCOLLIDE;
      info.MType = CurrentMobType;
      SpawnedVobs.push_back(info);
      RecreateMobInstance(false, false);
      cam->SetTarget(CurrentInter);
      break;
    case PARTICLES:
      CheckMsgSize();
      PlacedTest = ParticleNames[activeparticlename];
      PlacedTest = PlacedTest + placed;
      BuildMessages.push_back(PlacedTest);
      PlacedTest.Clear();
      LastPos = CurrentInter->GetPositionWorld();
      LastTrafo = CurrentInter->trafoObjToWorld.GetAtVector();
      info.VisualName = ParticleNames[activeparticlename].ToChar();
      info.Vob = CurrentInter;
      info.Type = TYPE_PARTICLE;
      SpawnedVobs.push_back(info);
      CurrentInter = new oCMobInter;
      zCVisual* vis = zCParticleFX::Load(ParticleNames[activeparticlename]);
      CurrentInter->SetVisual(vis);
      ogame->GetWorld()->AddVob(CurrentInter);
      CurrentInter->SetPositionWorld(LastPos);
      CurrentInter->trafoObjToWorld.v[0][2] = LastTrafo[VX];
      CurrentInter->trafoObjToWorld.v[2][2] = LastTrafo[VZ];
      cam->SetTarget(CurrentInter);
      break;
  }
}

void CBuilder::PrintItemIds() {
  switch (SMode) {
    case MOBS:
      if (activemobname >= 0 && activemobname <= (int)MobNames.size() - 1) {
        sprintf_s(buffer, "Id : %d/%d", activemobname, (int)MobNames.size() - 1);
        IdText = buffer;
        screen->Print(0, 1600, IdText);
        screen->Print(0, 1800, MobNames[activemobname]);
      }
      break;
    case PARTICLES:
      if (activeparticlename >= 0 && activeparticlename <= (int)ParticleNames.size() - 1) {
        sprintf_s(buffer, "Id : %d/%d", activeparticlename, (int)ParticleNames.size() - 1);
        IdText = buffer;
        screen->Print(0, 1600, IdText);
        screen->Print(0, 1800, ParticleNames[activeparticlename]);
      }
      break;
  }
}

void CBuilder::ClearAfterObjMenu() {
  Mode = EDITING;
  RestoreVobMatrix(SpawnedVobs[ObjMenu->MenuPos].Vob, ObjMenu->LastAngle);
  CreateModeBoxes();
  RecreateMobInstance(false, true);
  cam->SetTarget(CurrentInter);
  delete ObjMenu;
  ObjMenu = NULL;
}
void CBuilder::ClearUpBoxes() {
  if (MobsWin) {
    screen->RemoveItem(MobsWin);
    delete MobsWin;
    MobsWin = NULL;
  }
  if (ParticleWin) {
    screen->RemoveItem(ParticleWin);
    delete ParticleWin;
    ParticleWin = NULL;
  }
  if (ItemsWin) {
    screen->RemoveItem(ItemsWin);
    delete ItemsWin;
    ItemsWin = NULL;
  }
}

void CBuilder::CreateModeBoxes() {
  if (MobsWin) {
    screen->RemoveItem(MobsWin);
    delete MobsWin;
  }
  if (ParticleWin) {
    screen->RemoveItem(ParticleWin);
    delete ParticleWin;
  }
  if (ItemsWin) {
    screen->RemoveItem(ItemsWin);
    delete ItemsWin;
  }
  MobsWin = new zCView(0, 0, 8192, 8192, VIEW_ITEM);
  MobsWin->SetPos(2650, 0);
  MobsWin->SetSize(800, 250);
  MobsWin->InsertBack(zSTRING("MENU_INGAME.TGA"));
  if (SMode != MOBS) {
    MobsWin->SetFontColor(zCOLOR(255, 255, 255, 200));
    MobsWin->SetTransparency(50);
  }
  MobsWin->Print(500, 350, zSTRING("Mobs (F5)"));
  screen->InsertItem(MobsWin, false);
  ParticleWin = new zCView(0, 0, 8192, 8192, VIEW_ITEM);
  ParticleWin->SetPos(3450, 0);
  ParticleWin->SetSize(1100, 250);
  ParticleWin->InsertBack(zSTRING("MENU_INGAME.TGA"));
  if (SMode != PARTICLES) {
    ParticleWin->SetFontColor(zCOLOR(255, 255, 255, 200));
    ParticleWin->SetTransparency(50);
  }
  ParticleWin->Print(500, 350, zSTRING("Particles (F6)"));
  screen->InsertItem(ParticleWin, false);
  /*ItemsWin = new zCView(0,0,8192,8192,VIEW_ITEM);
  ItemsWin->SetPos(4550,0);
  ItemsWin->SetSize(900,250);
  ItemsWin->InsertBack(zSTRING("MENU_INGAME.TGA"));
  if(SMode != ITEMS){
  ItemsWin->SetFontColor(zCOLOR(255,255,255,200));
  ItemsWin->SetTransparency(50);}
  ItemsWin->Print(500,350,zSTRING("Items (F7)"));
  screen->InsertItem(ItemsWin,false);*/
}

void CBuilder::CollisionInObjectsEnabled(bool isit) {
  if (isit) {
    for (int i = 0; i < (int)SpawnedVobs.size(); i++) {
      if (SpawnedVobs[i].Type == TYPE_MOB)
        SpawnedVobs[i].Vob->SetCollDet(1);
    }
  } else {
    for (int i = 0; i < (int)SpawnedVobs.size(); i++) {
      if (SpawnedVobs[i].Type == TYPE_MOB)
        SpawnedVobs[i].Vob->SetCollDet(0);
    }
  }
};

void CBuilder::SaveVobMatrix(zCVob* Vob, zMAT4& Matrix) {
  Matrix.v[0][0] = Vob->trafoObjToWorld.v[0][0];
  Matrix.v[1][0] = Vob->trafoObjToWorld.v[1][0];
  Matrix.v[2][0] = Vob->trafoObjToWorld.v[2][0];
  Matrix.v[0][2] = Vob->trafoObjToWorld.v[0][2];
  Matrix.v[1][2] = Vob->trafoObjToWorld.v[1][2];
  Matrix.v[2][2] = Vob->trafoObjToWorld.v[2][2];
  Matrix.v[0][1] = Vob->trafoObjToWorld.v[0][1];
  Matrix.v[1][1] = Vob->trafoObjToWorld.v[1][1];
  Matrix.v[2][1] = Vob->trafoObjToWorld.v[2][1];
};

void CBuilder::RestoreVobMatrix(zCVob* Vob, zMAT4& Matrix) {
  Vob->trafoObjToWorld.v[0][0] = Matrix.v[0][0];
  Vob->trafoObjToWorld.v[1][0] = Matrix.v[1][0];
  Vob->trafoObjToWorld.v[2][0] = Matrix.v[2][0];
  Vob->trafoObjToWorld.v[0][2] = Matrix.v[0][2];
  Vob->trafoObjToWorld.v[1][2] = Matrix.v[1][2];
  Vob->trafoObjToWorld.v[2][2] = Matrix.v[2][2];
  Vob->trafoObjToWorld.v[0][1] = Matrix.v[0][1];
  Vob->trafoObjToWorld.v[1][1] = Matrix.v[1][1];
  Vob->trafoObjToWorld.v[2][1] = Matrix.v[2][1];
};

// Load all vobs from the world to vector
void CBuilder::LoadAllVobsFromTheWorld() {
  zCListSort<zCVob>* VobList = ogame->GetGameWorld()->voblist;
  int size = VobList->GetNumInList();
  for (int i = 0; i < size; i++) {
    VobList = VobList->next;
    zCVob* VobInList = VobList->GetData();
    VobsInWorld.push_back(VobInList);
  }
}

void CBuilder::CheckMsgSize() {
  if (BuildMessages.size() > 6)
    BuildMessages.erase(BuildMessages.begin() + 1);
};

// Fills mob names
void CBuilder::FillMobNames() {
  ifstream g2names(".\\Multiplayer\\WorldBuilder\\g2mobs.wb");
  char _buff[1024];
  if (g2names.good()) {
    while (!g2names.eof()) {
      g2names.getline(_buff, 1024);
      MobNames.push_back(zSTRING(_buff));
    }
  }
  g2names.close();
};

void CBuilder::FillParticleNames() {
  ifstream g2particles(".\\Multiplayer\\WorldBuilder\\g2particles.wb");
  char _buff[1024];
  if (g2particles.good()) {
    while (!g2particles.eof()) {
      g2particles.getline(_buff, 1024);
      ParticleNames.push_back(zSTRING(_buff));
    }
  }
  g2particles.close();
};

// Delete all npcs in current world
void CBuilder::DeleteAllNpcs() {
  zCListSort<oCNpc>* NpcList = ogame->GetGameWorld()->voblist_npcs;
  int size = NpcList->GetNumInList();
  for (int i = 0; i < size; i++) {
    NpcList = NpcList->next;
    oCNpc* NpcOnList = NpcList->GetData();
    if (NpcOnList->GetInstance() != 11471)
      NpcOnList->Disable();
  }
  ogame->GetSpawnManager()->SetSpawningEnabled(0);
};

// Disabling all items in current world
void CBuilder::DisableAllItemsInWorld() {
  zCListSort<oCItem>* ItemList = ogame->GetGameWorld()->voblist_items;
  int size = ItemList->GetNumInList();
  oCWorld* World = ogame->GetGameWorld();
  for (int i = 0; i < size; i++) {
    ItemList = ItemList->next;
    oCItem* ItemInList = ItemList->GetData();
    World->DisableVob(ItemInList);
  }
};