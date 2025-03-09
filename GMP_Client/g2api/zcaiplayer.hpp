/*/////////////////////////////////////////////////////////////////////////////
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
#ifndef __ZCAIPLAYER_H_INCLUDED__
#define __ZCAIPLAYER_H_INCLUDED__
#include "ztypes.hpp"

class zCModel;
class zCModelNodeInst;

enum zTSurfaceAlignMode 
{					   
	zMV_SURFACE_ALIGN_NONE  = 0, // none - like human
	zMV_SURFACE_ALIGN_NORMAL= 1, // like f.e shadowbeast*
	zMV_SURFACE_ALIGN_HIGH  = 2, // like f.e meatbugs
};

enum zTMovementState 
{
	zMV_STATE_STAND		 = 0,				
	zMV_STATE_FLY		   = 1,				
	zMV_STATE_SWIM		  = 2,				
	zMV_STATE_DIVE		  = 3,				
};

class zCAIBase : public zCObject {};

class zCAIPlayer : public zCAIBase
{
	struct zTConfig
	{
		float zMV_MIN_SPACE_MOVE_FORWARD;
		float zMV_DETECT_CHASM_SCAN_AT;
		float zMV_STEP_HEIGHT;
		float zMV_JUMP_UP_MIN_CEIL;
		float zMV_WATER_DEPTH_KNEE;
		float zMV_WATER_DEPTH_CHEST;
		float zMV_YMAX_SWIM_CLIMB_OUT;
		float zMV_FORCE_JUMP_UP;
		float zMV_YMAX_JUMPLOW; 
		float zMV_YMAX_JUMPMID;
		float zMV_MAX_GROUND_ANGLE_WALK;
		float zMV_MAX_GROUND_ANGLE_SLIDE;
		float zMV_MAX_GROUND_ANGLE_SLIDE2;
		float zMV_DCUL_WALL_HEADING_ANGLE;
		float zMV_DCUL_WALL_HORIZ_ANGLE;
		float zMV_DCUL_GROUND_ANGLE;
	} config;				 

	zCVob*		vob;
	zCModel*	model; 
	zCWorld*	world;

	zPOINT3		centerPos;
	float		feetY;
	float		headY;
	float		aboveFloor;

	int			waterLevel;

	float		velocityLen2;
	zVEC3		velocity;				 

	float		fallDownDistanceY;
	float		fallDownStartY;			  

	zVEC3		slidePolyNormal;
	float		checkWaterCollBodyLen;					   
	zCModelNodeInst*  modelHeadNode;  

	int bitfield[6];

	struct zTBloodVobTrack 
	{
	zCVob*	bloodVob;
	zREAL	alpha;
	};

	zCArray<zTBloodVobTrack>  bloodVobList;

	float		bleedingPerc;	   //zREAL
	zVEC3		bleedingLastPos;	//zVEC3
	float		bleedingNextDist;   //zREAL

	zCVob*		weaponTrailVob;	 //zCVob*

	zCVob*		waterRingVob;	   //zCVob*
	float		waterRingTimer;	 //zREAL

	float		autoRollPos;		 //zREAL	  
	float		autoRollPosDest;	 //zREAL	  
	float		autoRollSpeed;	   //zREAL	  
	float		autoRollMaxAngle;	//zREAL

	BYTE	dummyLastVar;	   //zBYTE
	zSTRING bloodDefaultTexName; //zSTRING   
public:
	void SetVob(zCVob*){XCALL(0x0050CAD0)};
	float GetProtoYHeight(void){XCALL(0x00511820)};
	void Begin(zCVob*){XCALL(0x0050E750)};
	void End(void){XCALL(0x0050E8F0)};
};

#endif  //__ZCAIPLAYER_H_INCLUDED__