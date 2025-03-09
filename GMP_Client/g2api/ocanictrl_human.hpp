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

#ifndef __OCANICTRL_HUMAN_H_INCLUDED__
#define __OCANICTRL_HUMAN_H_INCLUDED__

#include "ztypes.hpp"
#include "zcaiplayer.hpp"

class zCAICamera;
class zCVob;

#define ANI_NUM 10
#define ANI_HITLIMB_MAX 4
#define ANI_MAX_HIT 10

/** Insert description. */
class oCAniCtrl_Human : public zCAIPlayer
{
public:
	float	angle_slide1;
	float	angle_slide2;
	float	angle_heading;
	float	angle_horiz;
	float	angle_ground;

	oCNpc*  npc;
	zCVob*	targetVob;			 
	zCVob*	stopTurnVob;
	int		stopTurnVobSign;		  

	zCModelNodeInst* hitlimb[ANI_HITLIMB_MAX];

	int		actionMode;

	int		wmode;
	int		wmode_last;
	int		wmode_selected;
	int		changeweapon;
 
	int		walkmode;
	int		nextwalkmode; 
	int		lastRunWalkSneak;
	int		always_walk;
	int		do_jump;
	int		defaultSurfaceAlign;
	float	autoRollDirection;				 

	float	lookTargetx;				  
	float	lookTargety;			   

	int		distance;
	zVEC3	hitpos;
	zSTRING limbname;
	float	paradeBeginFrame;
	float	paradeEndFrame;		  

	struct 
	{
		char	canEnableNextCombo	: 1;
		char	endCombo			: 1;
		char	comboCanHit			: 1;
		char	hitPosUsed			: 1;
		char	hitGraphical		: 1;
		char	canDoCollisionFX	: 1;
	} bitfield;  

	int		comboNr;
	int		comboMax;
	float	lastHitAniFrame;	  
	int		hitAniID;
	zCVob*	hitTarget;	   

	typedef struct 
	{
		zTModelAniID	hitAni;							 
		int			 hitOptFrame;
		int			 hitEndFrame;
		int			 comboStartFrame;
		int			 comboEndFrame;
		int			 comboDir;
	} oTComboInfo;

	oTComboInfo comboInfo[ANI_MAX_HIT]; //ANI_MAX_HIT = 10

	float anioffset_lastper;
	float anioffset_thisper;
	float anioffset;			  
	int anioffset_ani;

	// *****************************************************************************************

	// Normal
	int s_dead1; int s_dead2;
	int s_hang;  int t_hang_2_stand;

	int s_run			   [ANI_NUM];int t_run_2_runl	  [ANI_NUM];  int t_runl_2_run	[ANI_NUM];
	int s_runl			  [ANI_NUM];int t_runl_2_runr	 [ANI_NUM];  int t_runr_2_runl   [ANI_NUM];
	int s_runr			  [ANI_NUM];int t_runr_2_run	  [ANI_NUM];
	int t_runturnl		  [ANI_NUM];int t_runturnr		[ANI_NUM];
	int t_runstrafel		[ANI_NUM];int t_runstrafer	  [ANI_NUM];

	int t_run_2_runbl	   [ANI_NUM];int t_runbl_2_run	 [ANI_NUM];
	int s_runbl			 [ANI_NUM];int t_runbl_2_runbr   [ANI_NUM];  int t_runbr_2_runbl [ANI_NUM];
	int s_runbr			 [ANI_NUM];int t_runbr_2_run	 [ANI_NUM];

	int t_jumpb			 [ANI_NUM];

	// Transitions
	int t_run_2_walk		[ANI_NUM];int t_walk_2_run	  [ANI_NUM];
	int t_run_2_sneak	   [ANI_NUM];int t_sneak_2_run	 [ANI_NUM];

	int s_walk			  [ANI_NUM];int t_walk_2_walkl	[ANI_NUM];  int t_walkl_2_walk  [ANI_NUM];
	int s_walkl			 [ANI_NUM];int t_walkl_2_walkr   [ANI_NUM];  int t_walkr_2_walkl [ANI_NUM];
	int s_walkr			 [ANI_NUM];int t_walkr_2_walk	[ANI_NUM];
	int t_walkturnl		 [ANI_NUM];int t_walkturnr	   [ANI_NUM];
	int t_walkstrafel	   [ANI_NUM];int t_walkstrafer	 [ANI_NUM];

	int t_walk_2_walkbl	 [ANI_NUM];int t_walkbl_2_walk   [ANI_NUM];
	int s_walkbl			[ANI_NUM];int t_walkbl_2_walkbr [ANI_NUM];  int t_walkbr_2_walkbl[ANI_NUM];
	int s_walkbr			[ANI_NUM];int t_walkbr_2_walk   [ANI_NUM];

	int t_runl_2_jump	   [ANI_NUM];int t_runr_2_jump	 [ANI_NUM];
	int t_jump_2_runl	   [ANI_NUM];

	int t_stand_2_jumpuplow; int s_jumpuplow; int t_jumpuplow_2_stand;
	int t_stand_2_jumpupmid; int s_jumpupmid; int t_jumpupmid_2_stand;

	int s_sneak			 [ANI_NUM];  int t_sneak_2_sneakl	[ANI_NUM];  int t_sneakl_2_sneak	[ANI_NUM];
	int s_sneakl			[ANI_NUM];  int t_sneakl_2_sneakr   [ANI_NUM];  int t_sneakr_2_sneakl   [ANI_NUM];
	int s_sneakr			[ANI_NUM];  int t_sneakr_2_sneak	[ANI_NUM];
	int t_sneakturnl		[ANI_NUM];  int t_sneakturnr		[ANI_NUM];
	int t_sneakstrafel	  [ANI_NUM];  int t_sneakstrafer	  [ANI_NUM];

	int t_sneak_2_sneakbl   [ANI_NUM];  int t_sneakbl_2_sneak   [ANI_NUM];
	int s_sneakbl		   [ANI_NUM];  int t_sneakbl_2_sneakbr [ANI_NUM];  int t_sneakbr_2_sneakbl [ANI_NUM];
	int s_sneakbr		   [ANI_NUM];  int t_sneakbr_2_sneak   [ANI_NUM];

	int t_walkl_2_aim	   [ANI_NUM];  int t_walkr_2_aim	   [ANI_NUM];
	int t_walk_2_aim		[ANI_NUM];  int s_aim			   [ANI_NUM];  int t_aim_2_walk	[ANI_NUM];
	int t_hitl			  [ANI_NUM];  int t_hitr			  [ANI_NUM];  int t_hitback	   [ANI_NUM];
	int t_hitf			  [ANI_NUM];  int s_hitf			  [ANI_NUM];
	int t_aim_2_defend	  [ANI_NUM];  int s_defend			[ANI_NUM];  int t_defend_2_aim  [ANI_NUM];
	int t_paradeL		   [ANI_NUM];  int t_paradeM		   [ANI_NUM];  int t_paradeS	   [ANI_NUM];
	int t_hitfrun		   [ANI_NUM];

	int t_stumble;  int t_stumbleb; int t_fallen_2_stand; int t_fallenb_2_stand;

	int t_walk_2_walkwl	 ;int t_walkwl_2_walk		;
	int s_walkwl			;int t_walkwl_2_walkwr	  ;   int t_walkwr_2_walkwl;
	int s_walkwr			;int t_walkwr_2_walk;

	int t_walk_2_walkwbl	;int t_walkwbl_2_walk	   ;
	int s_walkwbl		   ;int t_walkwbl_2_walkwbr	;   int t_walkwbr_2_walkwbl;
	int s_walkwbr		   ;int t_walkwbr_2_walk;

	int _s_walk			 ;int _t_walk_2_walkl		;   int _t_walkl_2_walk;
	int _s_walkl			;int _t_walkl_2_walkr	   ;   int _t_walkr_2_walkl;
	int _s_walkr			;int _t_walkr_2_walk		;
	int _t_turnl			;int _t_turnr			   ;
	int _t_strafel		  ;int _t_strafer;		

	int _t_walk_2_walkbl	;int _t_walkbl_2_walk	   ;
	int _s_walkbl		   ;int _t_walkbl_2_walkbr	 ;   int _t_walkbr_2_walkbl;
	int _s_walkbr		   ;int _t_walkbr_2_walk;				  

	int s_jumpstand		 ;int t_stand_2_jumpstand;   int t_jumpstand_2_stand;
	int _t_jumpb			;
	int _t_stand_2_jump	 ;int _s_jump			;   int t_jump_2_stand;
	int _t_stand_2_jumpup   ;int _s_jumpup;

	int _t_jumpup_2_falldn  ;int _t_jump_2_falldn;

	int t_walkwl_2_swimf	;int s_swimf			;int t_swimf_2_walkwl;
	int t_walkwbl_2_swimb   ;int s_swimb			;int t_swimb_2_walkwbl;
	int t_swimf_2_swim	  ;int s_swim			 ;int t_swim_2_swimf;
	int t_swim_2_swimb	  ;int t_swimb_2_swim	 ;int t_warn;
	int t_swim_2_dive	   ;int s_dive			 ;int t_divef_2_swim;
	int t_dive_2_divef	  ;int s_divef			;int t_divef_2_dive;
	int t_dive_2_drowned	;int s_drowned		  ;
	int t_swimturnl		 ;int t_swimturnr		;
	int t_diveturnl		 ;int t_diveturnr;			   

	int _t_walkl_2_aim	  ;int _t_walkr_2_aim	 ;
	int _t_walk_2_aim	   ;int _s_aim			 ;int _t_aim_2_walk;
	int _t_hitl			 ;int _t_hitr			;int _t_hitback;
	int _t_hitf			 ;int _t_hitfstep		;int _s_hitf;
	int _t_aim_2_defend	 ;int _s_defend		  ;int _t_defend_2_aim;
	int _t_paradeL		  ;int _t_paradeM		 ;int _t_paradeS;
	int _t_hitfrun;

	int t_stand_2_iaim	  ;int s_iaim			 ;int t_iaim_2_stand;

	int t_iaim_2_idrop	  ;int s_idrop			;int t_idrop_2_stand;

	int t_iaim_2_ithrow	 ;int s_ithrow		   ;int t_ithrow_2_stand;

	int t_stand_2_iget	  ;int s_iget			 ;int t_iget_2_stand;
	int s_oget;

	int _t_stand_2_torch	;int _s_torch		   ;int _t_torch_2_stand;

	int hitani;

	int help			;int help1				;int help2;
	int s_fall		  ;int s_fallb			;int s_fallen		  ;int s_fallenb;	 int s_falldn;
	int _t_runl_2_jump  ;int _t_runr_2_jump	 ;int _t_jump_2_runl;
	int s_look;int s_point;
	int dummy1;int dummy2;int dummy3;int dummy4;
	int s_weapon[ANI_NUM];
	int togglewalk;

	int t_stand_2_cast;
	int s_cast; 
	int t_cast_2_shoot;
	int t_cast_2_stand;
	int s_shoot;
	int t_shoot_2_stand;

	int dummyLastVar;
public:

	//.text:006ADD40 ; public: int __thiscall oCAniCtrl_Human::IsAiming(void)
	/** Insert description. 
	* @usable Ingame only
	*/
	zINT IsAiming(void) 
	{ 
		XCALL(0x006ADD40) 
	};

	//.text:006B6490 ; public: void __thiscall oCAniCtrl_Human::SetLookAtTarget(class zCVob *)
	/** Insert description. 
	* @usable Ingame only
	*/
	void SetLookAtTarget(zCVob* pA) 
	{ 
		XCALL(0x006B6490) 
	};

	//.text:006B6360 ; public: void __thiscall oCAniCtrl_Human::SetLookAtTarget(class zVEC3 &)
	/** Insert description. 
	* @usable Ingame only
	*/
	void SetLookAtTarget(zVEC3& pos) 
	{ 
		XCALL(0x006B6360) 
	};

	//.text:006ABDB0 ; public: void __thiscall oCAniCtrl_Human::SetAlwaysWalk(int)
	/** Insert description. 
	* @usable Ingame only
	*/
	void SetAlwaysWalk(int) 
	{ 
		XCALL(0x006ABDB0) 
	};

	//.text:006B6310 ; public: void __thiscall oCAniCtrl_Human::SetLookAtTarget(float, float
	/** Insert description. 
	* @usable Ingame only
	*/
	void SetLookAtTarget(float, float) 
	{ 
		XCALL(0x006B6310) 
	};

	void StopLookAtTarget(void){XCALL(0x006B6640)};

	//.text:006B62F0 ; public: void __thiscall oCAniCtrl_Human::LookAtTarget(void)
	/** Insert description. 
	* @usable Ingame only
	*/
	void LookAtTarget(void) 
	{ 
		XCALL(0x006B62F0) 
	};

	void Turn(float, int){XCALL(0x006AE540)};
	void TransitionToCast(void){XCALL(0x006B84A0)};
	void StopTurnAnis(void){XCALL(0x006AE530)};
	void Init(oCNpc*){XCALL(0x006A3D60)};
	void InitAnimations(void){XCALL(0x006A4010)};
	int IsRunning(void){XCALL(0x006AE200)};
	int IsFallen(void){XCALL(0x006ADC30)};
	int IsInWater(void){XCALL(0x006B8A40)};
	void Reset(void){XCALL(0x006A5080)};
	zVEC2 GetLookAtPos()
	{
		return zVEC2(*(float*)((DWORD)this+0x17C),*(float*)((DWORD)this+0x180));
	};
};

/** Insert description. */
class oCAIHuman : public oCAniCtrl_Human
{
	zCList<zCVob>   ignoreVobList;
	zCAICamera*		aiCam;
	
	struct  
	{
		int	forcejump				:1;
		int	lookedAround			:1;
		int	sprintActive			:1;
		int	crawlActive				:1;
		int	showai					:1;
		int	startObserveIntruder	:1;
		int	dontKnowAniPlayed		:1;
		int	spellReleased			:1;
		int	spellCastedLastFrame	:1;
		int	eyeBlinkActivated		:1;
		int	thirdPersonFallback		:1;
	} bitfield;

	float			createFlyDamage;  //zREAL
public:
	//.text:0069B570 ; public: void __thiscall oCAIHuman::SetSprintMode(int)
	/** Insert description. 
	* @usable Ingame only
	*/
	void Init(oCNpc*){XCALL(0x00695390)};
	int CheckActiveSpells(void){XCALL(0x00472100)};
	void SetSprintMode(int enabled) 
	{ 
		XCALL(0x0069B570) 
	};
	void DoAI(zCVob*, zINT&){XCALL(0x0069BAB0)};
};

#endif  //__OCANICTRL_HUMAN_H_INCLUDED__
