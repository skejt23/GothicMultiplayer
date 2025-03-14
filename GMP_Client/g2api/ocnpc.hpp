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

#include "ztypes.hpp"
#include "zstring.hpp"

#include "zcobject.hpp"
#include "ocvob.hpp"
#include "ocitem.hpp"
#include "ocnpcinventory.hpp"
#include "ocanictrl_human.hpp"
#include "zcaicamera.hpp"
#include "ocspell.hpp"

class oCAskBox;
class oCInfo;
class oCMission;
class oCNews;
class oCNpc;
class oCNpcMessage;
class oCNpcTalent;
class oCNpcTimedOverlay;
class oCRtnEntry;
class oCSpell;
class oCVisualFX;
class zCVisual;
class TNpcSlot;
class zCPlayerGroup;
class zCRoute;
class zCParticleFX;
class zTSoundHandle;
class oTDirectionInfo;
class zCModel;
class oCMobInter;
class oCMob;

enum oCNpc_Attribute
{
	NPC_ATR_HITPOINTS, 
	NPC_ATR_HITPOINTS_MAX,
	NPC_ATR_MANA,
	NPC_ATR_MANA_MAX,
	NPC_ATR_STRENGTH,
	NPC_ATR_DEXTERITY,
	NPC_ATR_REGENERATEHP,
	NPC_ATR_REGENERATEMANA,
	NPC_ATR_MAX
};

enum oCNpc_Hitchance
{
	NPC_HITCHANCE_1H,
	NPC_HITCHANCE_2H,
	NPC_HITCHANCE_BOW,
	NPC_HITCHANCE_CROSSBOW,
	NPC_HITCHANCE_MAX
};

enum oCNpc_Attitude
{ 
	NPC_ATTITUDE_HOSTILE, 
	NPC_ATTITUDE_ANGRY, 
	NPC_ATTITUDE_NEUTRAL, 
	NPC_ATTITUDE_FRIENDLY
};

enum oCNpc_WeaponMode
{
	NPC_WEAPON_NONE = 0,
	NPC_WEAPON_FIST = 1,
	NPC_WEAPON_DAG  = 2,
	NPC_WEAPON_1HS	= 3,
	NPC_WEAPON_2HS  = 4, 
	NPC_WEAPON_BOW	= 5,
	NPC_WEAPON_CBOW = 6,
	NPC_WEAPON_MAG	= 7,
	NPC_WEAPON_MAX  = 8
};

class oCNewsMemory 
{
protected:
	int				knownNews_vtbl;
public:
	zCList <oCNews> iknow;
};

struct TNpcAIState 
{
	int	 index;									  // 0x05A4 int
	int	 loop;									   // 0x05A8 int
	int	 end;										// 0x05AC int
	int	 timeBehaviour;							  // 0x05B0 int
	float   restTime;								   // 0x05B4 zREAL
	int	 phase;									  // 0x05B8 int
	zBOOL   valid;									  // 0x05BC zBOOL
	zSTRING name;									   // 0x05C0 zSTRING
	float   stateTime;								  // 0x05D4 zREAL
	int	 prgIndex;								   // 0x05D8 int
	zBOOL   isRtnState;								 // 0x05DC zBOOL
};


struct oCNpc_States 
{
public:
	int			state_vtbl;									 // 0x0588
	zSTRING		state_name;									 // 0x058C zSTRING
	oCNpc*		state_npc;									  // 0x05A0 oCNpc*
	TNpcAIState current_state;
	TNpcAIState next_state;
	int			lastAIState;										 // 0x061C int
	zBOOL		hasRoutine;										  // 0x0620 zBOOL
	zBOOL		rtnChanged;										  // 0x0624 zBOOL
	int			rtnBefore;										   // 0x0628 oCRtnEntry*
	int			rtnNow;											  // 0x062C oCRtnEntry*
	zCRoute*	rtnRoute;											// 0x0630 zCRoute*
	zBOOL		rtnOverlay;										  // 0x0634 zBOOL
	int			rtnOverlayCount;									 // 0x0638 int
	int			walkmode_routine;									// 0x063C int
	zBOOL		weaponmode_routine;								  // 0x0640 zBOOL
	zBOOL		startNewRoutine;									 // 0x0644 zBOOL
	int			aiStateDriven;									   // 0x0648 int
	zVEC3		aiStatePosition;									 // 0x064C zVEC3
	oCNpc*		parOther;											// 0x0658 oCNpc*
	oCNpc*		parVictim;										   // 0x065C oCNpc*
	oCItem*		parItem;											 // 0x0660 oCItem*
	int			rntChangeCount;									  // 0x0664 int
	
	void ClearAIState(void) {XCALL(0x0076D6E0);}
	void EndCurrentState(void) {XCALL(0x0076D880);}
	int GetLastState(void) {XCALL(0x0076E8B0);}
	int GetState(void) {XCALL(0x0076C020);}
	zSTRING GetRoutineName(void) {XCALL(0x076E180);}
};

struct oSDamageDescriptor
{
	int				dwFieldsValid;
	zCVob*			pVobAttacker;
	oCNpc*			pNpcAttacker;
	zCVob*			pVobHit;
	oCVisualFX*		pFXHit;
	oCItem*			pItemWeapon;
	zUINT32			nSpellID;
	zUINT32			nSpellCat;
	zUINT32			nSpellLevel;
	int				enuModeDamage;
	int				enuModeWeapon;
	zUINT32			aryDamage[8];
	zREAL			fDamageTotal;
	zREAL			fDamageMultiplier;
	zVEC3			vecLocationHit;
	zVEC3			vecDirectionFly;
	zSTRING			strVisualFX;
	zREAL			fTimeDuration;
	zREAL			fTimeInterval;
	zREAL			fDamagePerInterval;
	zBOOL			bDamageDontKill;

	struct
	{					 
		zUINT32			bOnce			: 1;
		zUINT32			bFinished		: 1;
		zUINT32			bIsDead			: 1;
		zUINT32			bIsUnconscious	: 1;
		zUINT32			lReserved		: 28;
	};

	zREAL			fAzimuth;
	zREAL			fElevation;
	zREAL			fTimeCurrent;
	zREAL			fDamageReal;
	zREAL			fDamageEffective;
	zUINT32			aryDamageEffective[8];
	zCVob*			pVobParticleFX;
	zCParticleFX*	pParticleFX;
	oCVisualFX*		pVisualFX;
};

class oTRobustTrace 
{
protected:
	int				_bitfield;											 // 0x04C4 oCNpc_oTRobustTrace_bitfield_Xxx
public:
	zVEC3			targetPos;											 // 0x04C8 zVEC3
	zCVob*   		targetVob;											 // 0x04D4 zCVob*
	zCVob*   		obstVob;											   // 0x04D8 zCVob*
	float	 		targetDist;											// 0x04DC zREAL
	float	 		lastTargetDist;										// 0x04E0 zREAL
	float	 		maxTargetDist;										 // 0x04E4 zREAL
	float	 		dirTurn;											   // 0x04E8 zREAL
	float	 		timer;												 // 0x04EC zREAL
	zVEC3	   	dirFirst;											  // 0x04F0 zVEC3
	float	 		dirLastAngle;										  // 0x04FC zREAL
	zCArray<oTDirectionInfo*> lastDirections;
	int				frameCtr;											  // 0x050C int
	zVEC3			targetPosArray[5];									 // 0x0510 zVEC3[5]
	int				targetPosCounter;									  // 0x054C int
	int				targetPosIndex;										// 0x0550 int
	float			checkVisibilityTime;								   // 0x0554 zREAL
	float			positionUpdateTime;									// 0x0558 zREAL
	float			failurePossibility;									// 0x055C zREAL
};

class oCMagFrontier 
{
public:
	oCVisualFX*		warningFX;
	oCVisualFX*		shootFX;
	oCNpc*			npc;
protected:
	int				_bitfield;
};

class oCNpc : public oCVob
{
public:
	int				idx;
	zSTRING			name[5];
	zSTRING			slot;															// 0x0188 zSTRING
	zSTRING			effect;															// 0x019C zSTRING
	int				npcType;														// 0x01B0 int
	int				variousFlags;													// 0x01B4 int
	int				attribute[8];													// 0x01B8 int[NPC_ATR_MAX]
	int				hitChance[5];													// 0x01D8 int[NPC_HITCHANCE_MAX]
	int				protection[8];													// 0x01EC int[oEDamageIndex_MAX]
	int				damage[8];														// 0x020C int[oEDamageIndex_MAX]
	int				damagetype;														// 0x022C int
	int				guild;															// 0x0230 int
	int				level;															// 0x0234 int
	int				mission[5];														// 0x0238 int[NPC_MIS_MAX]
	int				fighttactic;													// 0x024C int
	int				fmode;															// 0x0250 int
	int				voice;															// 0x0254 int
	int				voicePitch;														// 0x0258 int
	int				mass;															// 0x025C int
	int				daily_routine;													// 0x0260 int
	int				startAIState;													// 0x0264 int
	zSTRING			spawnPoint;														// 0x0268 zSTRING
	int				spawnDelay;														// 0x027C int
	int				senses;															// 0x0280 int
	int				senses_range;													// 0x0284 int
	int				aiscripts[100];													// 0x0288 int[100]
	zSTRING			wpname;															// 0x0418 zSTRING
	zUINT32			experience_points;												// 0x042C zUINT32
	zUINT32			experience_points_next_level;									// 0x0430 zUINT32
	zUINT32			learn_points;													// 0x0434 zUINT32
	int				bodyStateInterruptableOverride;									// 0x0438 int
	int				noFocus;														// 0x043C zBOOL
	int				parserEnd;														// 0x0440 int

	int				bloodEnabled;											   // 0x0444 int
	int				bloodDistance;											 // 0x0448 int
	int				bloodAmount;											   // 0x044C int
	int				bloodFlow;												 // 0x0450 int
	zSTRING			bloodEmitter;											  // 0x0454 zSTRING
	zSTRING			bloodTexture;											  // 0x0468 zSTRING
	int				didHit;													// 0x047C zBOOL
	int				didParade;												 // 0x0480 zBOOL
	int				didShoot;												  // 0x0484 zBOOL
	int				hasLockedEnemy;											// 0x0488 zBOOL
	zBOOL			isDefending;											   // 0x048C zBOOL
	zBOOL			wasAiming;												 // 0x0490 zBOOL
	int				lastAction;												// 0x0494 oCNpc::TFAction
	oCNpc*			enemy;													 // 0x0498 oCNpc*
	int				speedTurn;												 // 0x049C zREAL
	int				foundFleePoint;											// 0x04A0 zBOOL
	int				reachedFleePoint;										  // 0x04A4 zBOOL
	zVEC3			vecFlee;												   // 0x04A8 zVEC3
	zVEC3			posFlee;												   // 0x04B4 zVEC3
	void*			waypointFlee;											  // 0x04C0 zCWaypoint*
	oTRobustTrace   rbt;
	zCList<oCNpcTimedOverlay> timedOverlays;
	zCArray<oCNpcTalent*> talents;
	int				spellMana;										// 0x0574 int
	oCMagFrontier	magFrontier;
	oCNpc_States	state;
	oCNpcInventory  inventory2;										
	oCItemContainer* trader;										  // 0x0734 oCItemContainer*
	oCNpc*			tradeNpc;										 // 0x0738 oCNpc*
	float			rangeToPlayer;									// 0x073C zREAL
	//zCArray<zTSoundHandle> listOfVoiceHandles;
	char			x[20];
	int				voiceIndex;									   // 0x074C int
	zCArray<oCVisualFX*> effectList;
	int				bitfield[5];									  // 0x075C oCNpc_bitfieldX_Xxx
	int				instanz;										  // 0x0770 int
	zSTRING			mds_name;										 // 0x0774 zSTRING
	zSTRING			body_visualName;								  // 0x0788 zSTRING
	zSTRING			head_visualName;								  // 0x079C zSTRING
	zVEC3			model_scale;									  // 0x07B0 VEC3
	float			model_fatness;									// 0x07BC zREAL
	int				namenr;										   // 0x07C0 int
	float			hpHeal;										   // 0x07C4 zREAL
	float			manaHeal;										 // 0x07C8 zREAL
	float			swimtime;										 // 0x07CC zREAL
	float			divetime;										 // 0x07D0 zREAL
	float			divectr;										  // 0x07D4 zREAL
	zCVob*			fireVob;										  // 0x07D8 zCVob*
	int				fireDamage;									   // 0x07DC int
	float			fireDamageTimer;								  // 0x07E0 zREAL
	int				attitude;										 // 0x07E4 int
	int				tmp_attitude;									 // 0x07E8 int
	float			attTimer;										 // 0x07EC zREAL
	int				knowsPlayer;									  // 0x07F0 int
	int				percList[66];									 // 0x07F4 TNpcPerc[NPC_PERC_MAX] { int percID; int percFunc }
	int				percActive;									   // 0x08FC int
	float			percActiveTime;								   // 0x0900 zREAL
	float			percActiveDelta;								  // 0x0904 zREAL
	zBOOL			overrideFallDownHeight;						   // 0x0908 zBOOL
	float			fallDownHeight;								   // 0x090C zREAL
	int				fallDownDamage;								   // 0x0910 int
	oCMag_Book*		mag_book;										 // 0x0914 oCMag_Book*
	zCList<oCSpell> activeSpells;
	int				lastHitSpellID;								   // 0x0920 int
	int				lastHitSpellCat;								  // 0x0924 int
	zCArray<zSTRING> activeOverlays;
	int				askbox;										   // 0x0934 oCAskBox*
	int				askYes;										   // 0x0938 int
	int				askNo;											// 0x093C int
	float			canTalk;										  // 0x0940 zREAL
	oCNpc*			talkOther;										// 0x0944 oCNpc*
	oCInfo*			info;											 // 0x0948 oCInfo*
	oCNews*			news;											 // 0x094C oCNews*
	int				curMission;									   // 0x0950 oCMission*
	oCNewsMemory	knownNews;
	zCVob*			carry_vob;										// 0x0960 zCVob*
	oCMobInter*		interactMob;									  // 0x0964 oCMobInter*
	oCItem*			interactItem;									 // 0x0968 oCItem*
	int				interactItemCurrentState;						 // 0x096C int
	int	 		interactItemTargetState;						  // 0x0970 int
	int	  		script_aiprio;									// 0x0974 int
	int	 		old_script_state;								 // 0x0978 int
	oCAIHuman* human_ai;										 // 0x097C oCAIHuman*
	oCAniCtrl_Human* anictrl;										  // 0x0980 oCAniCtrl_Human*
	int	 		route;											// 0x0984 zCRoute*
	float			damageMul;										// 0x0988 zREAL
	oCNpcMessage*	csg;											  // 0x098C oCNpcMessage*
	oCNpcMessage*	lastLookMsg;									  // 0x0990 oCNpcMessage*
	oCNpcMessage*	lastPointMsg;									 // 0x0994 oCNpcMessage*
	zCArray<zCVob*> vobList;
	float			vobcheck_time;									// 0x09A4 zREAL
	float			pickvobdelay;									 // 0x09A8 zREAL
	zCVob*			focus_vob;										// 0x09AC zCVob*
	zCArray<TNpcSlot*> invSlot;
	zCArray<TNpcSlot*> tmpSlotList;
	float   		fadeAwayTime;									 // 0x09C8 zREAL
	float   		respawnTime;									  // 0x09CC zREAL
	float   		selfDist;										 // 0x09D0 zREAL
	int	 		fightRangeBase;								   // 0x09D4 int
	int	 		fightRangeFist;								   // 0x09D8 int
	int	 		fightRangeG;									  // 0x09DC int
	float			fight_waitTime;								   // 0x09E0 zREAL
	int	 		fight_waitForAniEnd;							  // 0x09E4 zTModelAniID
	float   		fight_lastStrafeFrame;							// 0x09E8 zREAL
	int	 		soundType;										// 0x09EC int
	zCVob*  		soundVob;										 // 0x09F0 zCVob*
	zVEC3   		soundPosition;									// 0x09F4 zVEC3
	zCPlayerGroup*  playerGroup;									  // 0x0A00 zCPlayerGroup*

public:
	int AssessPlayer_S(oCNpc*){XCALL(0x0075A740)};
	int CanUse(oCItem*){XCALL(0x007319B0)};
	void CleanUp(void){XCALL(0x0072E410)};
	void ClearAIVARS(void){
		for(int i = 0; i < 100; i++){
			aiscripts[i] = 0;
		}
	};
	// Clears Event Manager.
	void ClearEM(void){XCALL(0x00746400)};
	void CompleteHeal(void){XCALL(0x00736720)};
	void ChangeAttribute(int atr, int howmuch){XCALL(0x0072FF60)};
	void DoDie(oCNpc* Murderer = NULL){XCALL(0x00736760)};
	void EmergencyResetPos(zVEC3& pos){XCALL(0x006827D0)};
	int GetAIState(void){XCALL(0x0073EFE0);};
	int GetAttribute(zINT Index){XCALL(0x0072FF20);};
	oCNpc_Attitude GetAttitude(oCNpc* other){XCALL(0x0072FAB0);};
	oCAniCtrl_Human* GetAnictrl(void){XCALL(0x00734B40);};
	oCItem* GetFromInv(zINT p1, zINT p2){XCALL(0x00749180);};
	zINT GetGuild(void){XCALL(0x00730750)};
	zSTRING GetGuildName(void){XCALL(0x0072F690)};
	zINT GetInstance(void){XCALL(0x0072E380)};
	int GetNextWeaponMode(int, int, int){XCALL(0x00739A30)};
	zINT IsDead(void){XCALL(0x00736740)};
	int IsNear(oCNpc*){XCALL(0x0073FCC0)};
	int IsMovLock(void){XCALL(0x007380E0)};
	oCSpell* IsSpellActive(int){XCALL(0x0073D340)};
	zINT IsUnconscious(void){XCALL(0x00736750)};
	void InitModel(void){XCALL(0x00738480)};
	void InsertActiveSpell(oCSpell*){XCALL(0x0073D070)};
	void OpenInventory(zINT p1){XCALL(0x00762250);};
	void OpenScreen_Map(zINT p1){XCALL(0x0073D8D0);};
	void OpenScreen_Status(void){XCALL(0x0073D980)};
	void SetMovLockReal(zINT enabled){XCALL(0x007380B0);};
	void SetMovLock(int enabled){
		if(enabled){ if(this->GetBodyState() != 10 && this->GetBodyState() != 8) this->SetMovLockReal(enabled);}
		else this->SetMovLockReal(enabled);
	}
	void SetAttribute(zINT Index, zINT Value){XCALL(0x0072FFF0);};
	void SetGuild(zINT GuildId){XCALL(0x00730760);};
	void SetToFightMode(oCItem *, int p1){XCALL(0x0073A740);};
	void SetRightHand(oCVob *){XCALL(0x0073B1C0);};
	void SetLeftHand(oCVob *){XCALL(0x0073B0C0);};
	int HasInHand(oCVob *){XCALL(0x007374B0);};
	void DropAllInHand(void){XCALL(0x007375E0);};
	oCItem* GetRightHand(void){XCALL(0x0073AB50);};
	oCItem* GetLeftHand(void){XCALL(0x0073ABE0);};
	void ResetPos(zVEC3& pos){XCALL(0x006824D0);};
	int HasInHand(int a){XCALL(0x00737360);};
	oCNpc_WeaponMode GetWeaponMode(){XCALL(0x00738C40);};
	void SetWeaponMode2(const zSTRING& wm){XCALL(0x00738C60);};
	void SetWeaponMode2(oCNpc_WeaponMode wm){XCALL(0x00738E80);};
	void SetWeaponMode(oCNpc_WeaponMode wm){XCALL(0x00739940);};
	int FightAttackMelee(int){XCALL(0x0067EEC0);};
	oCMobInter* FindMobInter(const zSTRING & ScemeName){XCALL(0x0073FE70);};
	void EV_DrawWeapon(oCItem *){XCALL(0x0074CC10);};
	int GetActiveSpellNr(void){XCALL(0x0073CF60);};
	zCVob* GetFocusVob(void){XCALL(0x00732BB0);};
	oCNpc* GetFocusNpc(void){XCALL(0x00732BF0);};
	zVEC3 GetAngle(void)
	{
		return zVEC3(this->trafoObjToWorld.m[0][2], this->trafoObjToWorld.m[1][2], this->trafoObjToWorld.m[2][2]);
	};
	zVEC3 GetAngleN(void)
	{
		return zVEC3(this->trafoObjToWorld.m[2][0], this->trafoObjToWorld.m[0][0], this->trafoObjToWorld.m[1][0]);
	};
	float GetAngleX(void){return float(this->trafoObjToWorld.m[0][2]);};
	float GetAngleZ(void){return float(this->trafoObjToWorld.m[1][2]);};
	float GetAngleY(void){return float(this->trafoObjToWorld.m[2][2]);};
	float GetAngleNY(void){return float(this->trafoObjToWorld.m[0][0]);};
	float GetAngleNX(void){return float(this->trafoObjToWorld.m[2][0]);};
	float GetAngleNZ(void){return float(this->trafoObjToWorld.m[1][0]);};
	void SetAngle(float x, float y, float z, float nx, float ny, float nz)
	{
		this->trafoObjToWorld.m[0][2] = x;
		this->trafoObjToWorld.m[1][2] = z;
		this->trafoObjToWorld.m[2][2] = y;
		this->trafoObjToWorld.m[0][0] = ny;
		this->trafoObjToWorld.m[2][0] = nx;
		this->trafoObjToWorld.m[1][0] = nz;
		/*this->trafoObjToWorld.m[3][2] = xy;
		this->trafoObjToWorld.m[4][2] = xz;
		this->trafoObjToWorld.m[5][2] = xx;*/
	};
	void SetAngleAlt(float x, float y, float z)
	{
		this->trafoObjToWorld.m[0][2] = x;
		this->trafoObjToWorld.m[2][2] = y;
		this->trafoObjToWorld.m[4][2] = z;
	};
	zVEC3 GetPosition(void){return zVEC3(this->trafoObjToWorld.m[0][3], this->trafoObjToWorld.m[1][3], this->trafoObjToWorld.m[2][3]);};
	void SetPosition(float x, float y, float z)
	{
		this->trafoObjToWorld.m[0][3] = x;
		this->trafoObjToWorld.m[1][3] = y;
		this->trafoObjToWorld.m[2][3] = z;
	};
	void SetHeadVisual(const zSTRING& HeadName){*(zSTRING*)((DWORD)this+0x79C) = HeadName;};
	zSTRING& GetName() { return this->name[0]; };
	zINT GetEXP(void) { return this->experience_points; };
	void SetEXP(int x) { this->experience_points = x; };
	zINT GetEXPNext(void) { return this->experience_points_next_level; };
	void SetEXPNext(int x) { this->experience_points_next_level = x; };
	zINT GetLevel(void) { return this->level; };
	void SetLevel(int x) { this->level = x; };
	zINT GetLearningPoints(void) { return this->learn_points; };
	void SetLearningPoints(int x) { this->learn_points = x; };
	zINT GetHealth(void) { return this->attribute[NPC_ATR_HITPOINTS]; };
	void SetHealth(int x) { this->attribute[NPC_ATR_HITPOINTS] = x; };
	zINT GetMaxHealth(void) { return this->attribute[NPC_ATR_HITPOINTS_MAX]; };
	void SetMaxHealth(int x) { this->attribute[NPC_ATR_HITPOINTS_MAX] = x; };
	zINT GetMana(void) { return this->attribute[NPC_ATR_MANA]; };
	void SetMana(int x) { this->attribute[NPC_ATR_MANA] = x; };
	zINT GetMaxMana(void) { return this->attribute[NPC_ATR_MANA_MAX]; };
	void SetMaxMana(int x) { this->attribute[NPC_ATR_MANA_MAX] = x; };
	zINT GetStrength(void) { return this->attribute[NPC_ATR_STRENGTH]; };
	void SetStrength(int x) { this->attribute[NPC_ATR_STRENGTH] = x; };
	zINT GetDexterity(void) { return this->attribute[NPC_ATR_DEXTERITY]; };
	void SetDexterity(int x) { this->attribute[NPC_ATR_DEXTERITY] = x; };
	zFLOAT GetFatness() { return this->model_fatness; };
	void SetFatness(zFLOAT x) { this->model_fatness = x; };
	zCModel* GetModel(){XCALL(0x00738720)};
	oCMag_Book* GetSpellBook(){XCALL(0x0073EA00)};
	oCItem* GetSpellItem(int SpellId){XCALL(0x00680270)};
	void Disable(){XCALL(0x00745A20)};
	void DestroySpellBook(){XCALL(0x0075F0C0)};
	void MakeSpellBook(){XCALL(0x0075F040)};
	int MoveNpc_S(oCNpc*){XCALL(0x0075D350)};
	void Enable(zVEC3& pos){XCALL(0x00745D40)};
	void SetBodyState(int Value){XCALL(0x00745D40)};
	int GetBodyState(void){XCALL(0x0075EAE0)};
	void SetTradeNpc(oCNpc *){XCALL(0x006BD560)};
	int MoveMob_S(oCMob*){XCALL(0x0075D250)};
	void SetFlag(int flag){XCALL(0x007309C0)};
	int HasFlag(int flag){XCALL(0x007309E0)};
	void SetName(const zSTRING& Name)
	{
		this->name[0].Clear();
		this->name[0].Insert(0, Name);
	};
	void OnDamage(zCVob*, zCVob*, float, int, zVEC3&){XCALL(0x0067B860)};
	void OnDamage(oSDamageDescriptor &){XCALL(0x006660E0)};
	void Equip(class oCItem *){XCALL(0x00739C90)};
	void EquipArmor(oCItem *){XCALL(0x0073A490)};
	void EquipBestArmor(void){XCALL(0x0074F0B0)};
	void EquipBestWeapon(int){XCALL(0x0074EF30)};
	void EquipFarWeapon(oCItem *){XCALL(0x0073A310)};
	void EquipItem(oCItem *){XCALL(0x007323C0)};
	void UnequipItem(oCItem *){XCALL(0x007326C0)};
	oCItem* GetEquippedArmor(void){XCALL(0x00737B50)};
	oCItem* GetEquippedMeleeWeapon(void){XCALL(0x00737930)};
	oCItem* GetEquippedRangedWeapon(void){XCALL(0x00737A40)};
	void SetInteractItem(oCItem *){XCALL(0x0074ACC0)};
	void DoSetToFightMode(oCItem *){XCALL(0x00745350)};
	void Burn(int, float){XCALL(0x00735520)};
	void StopBurn(void){XCALL(0x007357A0)};
	void EquipWeapon(oCItem *){XCALL(0x0073A030)};
	void DoDropVob(zCVob *){XCALL(0x00744DD0)};
	void Fleeing(void){XCALL(0x006820C0)};
	void KillActiveSpells(void){XCALL(0x0073D2B0)};
	void DoTakeVob(zCVob *){XCALL(0x007449C0)};
	void DoShootArrow(int){XCALL(0x007446B0)};
	void OpenSpellBook(int){XCALL(0x0073E990)};
	void RefreshNpc(void){XCALL(0x00742110)};
	int ReadySpell(int, int){XCALL(0x006802E0)};
	int UnreadySpell(void){XCALL(0x00680480)};
	void DoThrowVob(zCVob *, float){XCALL(0x007450B0)};
	void Shrink(void){XCALL(0x0072CA20)};
	void UnShrink(void){XCALL(0x0072CBA0)};
	void SetActiveSpellinfo(int){XCALL(0x0073CF20)};
	void SetModelScale(const zVEC3 &){XCALL(0x0072D7B0)};
	void SetVisualNpc(zCVisual *){XCALL(0x0072E3F0)};
	void SetInteractMob(oCMobInter*){XCALL(0x0074AB00)};
	oCMobInter* GetInteractMob(void){XCALL(0x0074ACA0)};
	void SetAdditionalVisuals(const zSTRING & bodymesh, int bodytexture, int skincolor, const zSTRING & headmesh, int facetexture, int teethtexture, int armorinstance = -1){XCALL(0x00738350)};
	/* Example jak uzyc SetAdditionalVisuals
	oCNpc::GetHero()->SetAdditionalVisuals("HUM_BODY_NAKED0", 2, 0, "HUM_HEAD_BALD", 1, 0, -1);
	HUM_HEAD_BALD - model glowy
	int bodytexture mozna znalezc w skryptach w pliku AI_Constants.d, np const int BodyTex_L = 2; czyli dajemy do argumentu int 1
	int facetexture rowniez mozna znalezc w pliku AI_Constants.d, np. const int Face_B_Gorn = 17	; czyli w argument facetexture dajemy 17
	*/
	int GetOverlay(const zSTRING & mdsname){XCALL(0x00730010)};
	void ApplyOverlay(const zSTRING & mdsname){XCALL(0x0072D2C0)};
	void ApplyTimedOverlayMds(const zSTRING & mdsname, float time){XCALL(0x00756890)};
	void RemoveOverlay(const zSTRING & mdsname){XCALL(0x0072D5C0)};
	/*
	Example jak uzyc ApplyOverlay
	oCNpc::GetHero()->ApplyOverlay("HUMANS_RELAXED.MDS");
	sprawia ze nasz hero chodzi zrelaksowany
	*
	oCNpc::GetHero()->ApplyOverlay("HUMANS_MILITIA.MDS");
	sprawia ze nasz hero chodzi jak zolnierz
	inne HUMANS_BABE.MDS, HUMANS_TIRED.MDS, HUMANS_ARROGANCE.MDS, HUMANS_MAGE.MDS
	*/
	// SETS THIS NPC AS PLAYER
	void SetAsPlayer(void){XCALL(0x007426A0)};
	void SetHead(void){XCALL(0x007380F0)};
	zSTRING GetVisualHead(void){XCALL(0x00738810)};
	oCNpcInventory* GetInventory()
	{
		return &this->inventory2;
	};
	oCAniCtrl_Human* GetAnimationControl()
	{
		return *(oCAniCtrl_Human**)((DWORD)this+0x97C);
	};
	oCAIHuman* GetAIHuman()
	{
		return *(oCAIHuman**)((DWORD)this+0x980);
	};
	
	static void SetGodMode(int enabled)
	{
		*(int*)0x00AB2660 = enabled;
	}
	static void SetHero(oCNpc* hero)
	{
		*(oCNpc**)0x00AB2684 = hero;
		zCAICamera::GetCurrent()->SetTarget(hero);
	}
	inline static oCNpc* GetHero() { return *(oCNpc**)0x00AB2684; };
	
	void CloseInventory() { XCALL(0x00762410); };
	void SetShowNews(int) { XCALL(0x0075F480); };
	void SetTalentSkill(int, int) { XCALL(0x00730F60); };
	int GetTalentSkill(int) { XCALL(0x007317F0); };
	void SetTalentValue(int, int) { XCALL(0x00730BE0); };
	int GetTalentValue(int) { XCALL(0x00730DC0); };
	void GetSwimDiveTime(float &, float &, float &) {XCALL(0x00741FA0);}
	void SetSwimDiveTime(float, float) {XCALL(0x00741F70);}
};
