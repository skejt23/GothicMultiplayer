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
#include "zcrenderer.hpp"

class zCMesh;

enum zTWeather
{
	ZTWEATHER_SNOW,
	ZTWEATHER_RAIN
};

class zCSkyControler : public zCObject
{
public:
	void SetWeatherTypes(enum zTWeather){XCALL(0x005DF500)};
	void SetFillBackground(int){XCALL(0x005DF440)};
	void SetGlobalSkyScale(float){XCALL(0x005DF4C0)};
	zTWeather GetWeatherType(void){XCALL(0x005DF510)};
	int GetFillBackground(void){XCALL(0x005DF450)};
};

class zCSkyControlerMid : public zCSkyControler
{
public:
	void SetScreenBlendColor(const zCOLOR & color){XCALL(0x005E0610)};
};

class zCSkyControlerIndoor : public zCSkyControlerMid
{
public:
	void SetBackgroundColor(const zCOLOR & color){XCALL(0x005E07F0)};
};

class zCSkyControlerOutdoor : public zCSkyControlerMid
{
public:
	void SetWeatherType(enum zTWeather){XCALL(0x005EB830)};
	zCOLOR GetBackgroundColor(void){XCALL(0x005E6710)}
	float GetFarZ(void){XCALL(0x005E9E60)}

	void SetBackgroundColor(const zCOLOR & color)
	{
		*(zCOLOR*)((DWORD)this+0x594) = color;
	}
	int SetGlobalSkyScale(float){XCALL(0x005E7290)};
	void SetFarZScalability(float){XCALL(0x005E9E70)};

	void SetFarZ(float){XCALL(0x005E9E50)};
	//inline static zCSkyControlerOutdoor* GetCurrent(void) { return *(zCSkyControlerOutdoor**)0x0099AC8C; };
};

class oCBarrier
{
public:
	zCMesh* GetBarrierMesh(){return *(zCMesh**)(this);};
	void RenderLayer(zTRenderContext &, int, int &){XCALL(0x006B9CF0)};
};

class oCSkyControler_Barrier : public zCSkyControlerOutdoor
{
public:
	oCSkyControler_Barrier(){XCALL(0x006BB7B0);};
	oCBarrier* GetBarrier(){return *(oCBarrier**)((DWORD)this+0x6BC);};
	zCMesh* GetSkyMesh(){return *(zCMesh**)(this);};
	inline static oCSkyControler_Barrier* GetCurrent(void) { return *(oCSkyControler_Barrier**)0x0099AC8C; };
};