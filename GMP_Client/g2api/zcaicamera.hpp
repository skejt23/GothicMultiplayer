
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

#ifndef __API_G2_ZCAICAMERA_H__
#define __API_G2_ZCAICAMERA_H__

#include "InjectMage.h"
class zCAIBase;
class zCMovementTracker;

// Camera modes
#define CamModNormal *(zSTRING*)(0x008CE9F0)
#define CamModJump *(zSTRING*)(0x008CE9C8)
#define CamModFirstPerson *(zSTRING*)(0x008CEA18)
#define CamModLook *(zSTRING*)(0x008CEA04)
#define CamModMelee *(zSTRING*)(0x008CEA2C)
#define CamModRun *(zSTRING*)(0x008CEA40)
#define CamModFocus *(zSTRING*)(0x008CEA54)
#define CamModMagic *(zSTRING*)(0x008CEA68)
#define CamModClimb *(zSTRING*)(0x008CEA7C)
#define CamModSwim *(zSTRING*)(0x008CEA90)
#define CamModMeleeMult *(zSTRING*)(0x008CEAA4)
#define CamModThrow *(zSTRING*)(0x008CE938)
#define CamModInventory *(zSTRING*)(0x008CE924)
#define CamModRanged *(zSTRING*)(0x008CE910)
#define CamModShoulder *(zSTRING*)(0x008CE8FC)
#define CamModDive *(zSTRING*)(0x008CE8E8)
#define CamModRangedShrt *(zSTRING*)(0x008CE8D4)
#define CamModFall *(zSTRING*)(0x008CE8C0)
#define CamModDeath *(zSTRING*)(0x008CE8AC)
#define CamModLookBack *(zSTRING*)(0x008CE894)
#define CamModDialog *(zSTRING*)(0x008CE87C)

class zCAICamera : public zCAIBase
{
public:
	static zCAICamera* GetCurrent(void){XCALL(0x0049FD20)}; // Gets current class instance
	static zCAICamera* Create(void){XCALL(0x004A0180)}; 
	float GetMaxRange(void){XCALL(0x004A09B0)};
	void SetMaxRange(float range) // default 10.0f
	{
		*(float*)((DWORD)this+0x34) = range;
	}
	void SetCameraAngle(float Angle)
	{
		*(float*)((DWORD)this+0x44) = Angle;
	}
	float GetCameraAngle()
	{
		return *(float*)((DWORD)this+0x44);
	}
	void SetDistance(float distance)
	{
		*(float*)((DWORD)this+0x284) = distance;
	}
	zCMovementTracker* GetTracker(void)
	{
		return *(zCMovementTracker**)((DWORD)this+0x264);
	}
	float GetDistance(void){return *(float*)((DWORD)this+0x284);};
	void StartUp(void){XCALL(0x004A0230)};
	void SetMode(zSTRING& mode, zCArray<zCVob*>){XCALL(0x004A09C0)}; 
	void SetCameraMode(zSTRING& mode) // use this instead of SetMode, for arguments use something from Camera modes list which is located on top.
	{
		zCArray<zCVob*> nofuckingidea;
		SetMode(mode,nofuckingidea);
	}
	static void SetLookingOnNpcCamera(bool Arg)
	{
		if(Arg) EraseMemory(0x69D701 ,0x90, 2);
		else {
			BYTE buffer[]={0x74, 0x15};
			WriteMemory(0x69D701, buffer, 2);
		}
	};
	void SetRotateEnabled(int enabled){XCALL(0x004A54E0)};
	void SetVob(zCVob*){XCALL(0x004A0E60)};
	void SetTarget(zCVob*){XCALL(0x004A1120)};
	void DoAI(zCVob*, zINT&){XCALL(0x004A4FB0)};
	void CleanUp(void){XCALL(0x004A0850)};
	zSTRING GetMode(void){XCALL(0x004A0D40)};
	int IsModeActive(zSTRING& mode){XCALL(0x004A0D90)};
};


#endif