/*////////////////////////////////////////////////////////////////////////////

This file is part of the G2Ext SDK headers.

//////////////////////////////////////////////////////////////////////////////

The G2Ext SDK headers

Copyright (c) 2009, 2010 by Paindevs and Patrick Vogel

All Rights reserved.

THE WORK (AS DEFINED BELOW) IS PROVIDED
UNDER THE TERMS OF THIS CREATIVE COMMONS
PUBLIC LICENSE ("CCPL" OR "LICENSE").
THE WORK IS PROTECTED BY COPYRIGHT AND/OR
OTHER APPLICABLE LAW. ANY USE OF THE WORK
OTHER THAN AS AUTHORIZED UNDER THIS LICENSE
OR COPYRIGHT LAW IS PROHIBITED.

BY EXERCISING ANY RIGHTS TO THE WORK PROVIDED
HERE, YOU ACCEPT AND AGREE TO BE BOUND BY THE
TERMS OF THIS LICENSE. TO THE EXTENT THIS
LICENSE MAY BE CONSIDERED TO BE A CONTRACT,
THE LICENSOR GRANTS YOU THE RIGHTS CONTAINED
HERE IN CONSIDERATION OF YOUR ACCEPTANCE OF
SUCH TERMS AND CONDITIONS.

Full license at http://creativecommons.org/licenses/by-nc/3.0/legalcode

// Modified by Gothic Multiplayer Team (pampi, skejt23, mecio)

/////////////////////////////////////////////////////////////////////////////*/

#pragma once


#ifndef __API_G2_OCMOB_H__
#define __API_G2_OCMOB_H__

class oCMob : public zCVob
{
private:	
	zSTRING			name;
	int				bitfield;
	zSTRING			visualDestroyed;
	zSTRING			ownerStr;
	zSTRING			ownerGuildStr;
	int				owner;
	int				ownerGuild;
	int				focusNameIndex;
	zCList<zCVob>   ignoreVobList;
public:
	zVEC3 GetAngle(void)
	{
		return zVEC3(this->trafoObjToWorld.m[0][2], this->trafoObjToWorld.m[1][2], this->trafoObjToWorld.m[2][2]);
	};
	zMAT4 GetMatrixTrafo(void){return this->trafoObjToWorld;};
	void SetMatrixTrafo(zMAT4 mat){this->trafoObjToWorld = mat;};
	void SetAngle(float x, float y, float z)
	{
		this->trafoObjToWorld.m[0][2] = x;
		this->trafoObjToWorld.m[1][2] = z;
		this->trafoObjToWorld.m[2][2] = y;
	};
	void SetAngle(float x, float y)
	{
		this->trafoObjToWorld.m[0][2] = x;
		this->trafoObjToWorld.m[2][2] = y;
	};
	void SetAngle(zVEC3 Pos)
	{
		this->trafoObjToWorld.m[0][2] = Pos.x;
		this->trafoObjToWorld.m[1][2] = Pos.y;
		this->trafoObjToWorld.m[2][2] = Pos.z;
	};
	zSTRING GetMobName(void){XCALL(0x0071BC30)};
	void SetTriggerName(const zSTRING& TriggerName){*(zSTRING*)((DWORD)this+0x190) = TriggerName;};
	zSTRING& GetTriggerName(){ return *(zSTRING*)((DWORD)this+0x190);};
	zSTRING& GetStateFuncName(){ return *(zSTRING*)((DWORD)this+0x1E0);};
	void SetStateFunc(const zSTRING& StateName){*(zSTRING*)((DWORD)this+0x1E0) = StateName;};
	void SetName(const zSTRING & pA){XCALL(0x0071BC10)};
	void SetVisualoCMob(zCVisual *){XCALL(0x0071BBF0)}; // Visual bez interakcji
	static oCMob* _CreateNewInstance(void)
	{
		XCALL(0x00718590);
	}
};

class oCMobInter : public oCMob 
{
public:
	void SetVisualInter(zCVisual *){XCALL(0x0071D3C0)};
	void StartInteraction(oCNpc*){XCALL(0x00721580)};
	void EndInteraction(oCNpc*, int){XCALL(0x00721950)};
	int GetState(void){XCALL(0x00718C80)};
	zSTRING GetScemeName(void){XCALL(0x0071DBE0)};
	bool IsInState(oCNpc*, int){XCALL(0x0071FB00)};
	void Interact(oCNpc*, int, int, int, int, int){XCALL(0x0071F210)};
	void SendEndInteraction(oCNpc*, int, int){XCALL(0x0071F080)};
	void SendStateChange(int, int){XCALL(0x0071ED90)};
	void AI_UseMobToState(oCNpc*, int){XCALL(0x00721F00)};
	void StartStateChange(oCNpc*, int, int){XCALL(0x0071FEA0)};
	void SetMobBodyState(oCNpc*){XCALL(0x0071D610)};
	void SetTempState(int){XCALL(0x0071D540)};
	void SetStateToTempState(void){XCALL(0x0071D590)};
	void SetUseWithItem(const zSTRING & pA){XCALL(0x0071DA00)};
	static oCMobInter* _CreateNewInstance(void)
	{
		XCALL(0x007187E0);
	}
	static int SetAllMobsToState(oCWorld*, const zSTRING & pA, int){XCALL(0x0071CCD0)};

};

class oCMobLadder : public oCMobInter // drabina :D
{
public:
	void StartInteraction(oCNpc *){XCALL(0x007277D0)};
	void EndInteraction(oCNpc *, int){XCALL(0x00727E60)};
	int CanInteractWith(oCNpc *){XCALL(0x007279E0)};
	static oCMobLadder* _CreateNewInstance(void)
	{
		XCALL(0x00719EE0);
	}

};

class oCMobBed : public oCMobInter
{
public:
	static oCMobBed* _CreateNewInstance(void)
	{
		XCALL(0x00718A50);
	}

};

class oCMobSwitch : public oCMobInter
{
public:
	static oCMobSwitch* _CreateNewInstance(void)
	{
		XCALL(0x00718F20);
	}

};

class oCMobWheel : public oCMobInter
{
public:
	static oCMobWheel* _CreateNewInstance(void)
	{
		XCALL(0x00719B90);
	}

};


class oCMobLockable : public oCMobInter
{
public:
	void SetLocked(int islocked){XCALL(0x00719620)};
	void SetKeyInstance(const zSTRING & pA){XCALL(0x00719640)};
	int CanOpen(oCNpc *){XCALL(0x007244F0)};
	int CanInteractWith(oCNpc *){XCALL(0x00723CC0)};
	void Lock(oCNpc *){XCALL(0x007251C0)};
	void Unlock(oCNpc *, int){XCALL(0x00724A70)};
};

class oCMobContainer : public oCMobLockable // Mob z pojemnikiem na przedmioty
{
public:

	void Insert(oCItem *){XCALL(0x00725FC0)};
	void Remove(oCItem *){XCALL(0x00725FF0)};
	void Remove(oCItem *, int amount){XCALL(0x00726080)};
	void Reset(void){XCALL(0x00725E60)};
	int IsIn(int instance){XCALL(0x007264C0)};
	void Open(oCNpc *){XCALL(0x00726500)};
	void Close(oCNpc *){XCALL(0x00726640)};
	static oCMobContainer* _CreateNewInstance(void)
	{
		XCALL(0x00719440);
	}
};

class oCMobDoor : public oCMobLockable // Drzwi
{
public:
	zSTRING GetScemeName(void){XCALL(0x00726D60)};
	int SearchFreePosition(oCNpc *, float){XCALL(0x00726E10)};
	static oCMobDoor* _CreateNewInstance(void)
	{
		XCALL(0x0071A250);
	}
};

#endif