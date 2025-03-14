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
#include "macros.hpp"
#include "zcobject.hpp"
#include "ocvob.hpp"
#include "zcvob.hpp"

class oCNpc;
class zCView;
class zCViewBase;
class zCWorld;

enum oTSndMaterial
{
	MAT_WOOD,
	MAT_STONE,
	MAT_METAL,
	MAT_LEATHER,
	MAT_CLAY,
	MAT_GLAS
};

/** Insert description. */
class oCItem : public oCVob
{
public:
	int		idx;
	zSTRING name;
	zSTRING nameID;
	int		hp;
	int		hp_max;
	int	 mainflags;
	int		flags;
	int		weight;
	int	 value;

	// -- weapons		
	int	 damageType;
	int	 damageTotal;
	int	 damage[8];

	// -- armor 
	int	 wear;
	int	 protection[8];

	// -- food
	int	 nutrition;

	// -- use attributes
	int	 cond_atr[3];
	int	 cond_value[3];

	// -- attributes that will be changed on equip
	int	 change_atr[3];
	int	 change_value[3];

	// -- parser functions
	int	 magic;
	int	 on_equip;
	int	 on_unequip;	
	int	 on_state[4];			

	// -- owner									
	int		owner;			//	owner: npc instance
	int		ownerGuild;		//	owner: guild
	int		disguiseGuild;

	// -- visual
	zSTRING	visual;

	// -- change of mesh on equip
	zSTRING visual_change;	//	ASC file
	zSTRING effect;			//  Effect instance

	int		visual_skin;

	zSTRING	scemeName;
	int		material;	
	int		munition;		//	Instance of ammunition

	int 	spell;			
	int		range;			

	int		mag_circle;

	zSTRING	description;
	zSTRING	text[6];
	int		count[6];

	// -- inventory presentation
	int		inv_zbias;								//  far plane (how far the item goes into the room by the z-axis)
	int		inv_rotx;								//  rotation around x-axis (in degrees)
	int 	inv_roty;								//  rotation around y-axis (in degrees)
	int 	inv_rotz;								//  rotation around z-axis (in degrees)
	int 	inv_animate;							//  rotate the item

private:
	char _data[28];

public:
	void Delete(int){XCALL(0x00711440);};
	void CopyDamage(oCItem* p1){XCALL(0x00712360 );};
	zINT GetCount(zINT p1){XCALL(0x00712160);};
	zDWORD GetDamageTypes(void){XCALL(0x00712520);};
	/** This method returns the description text of the item.
	* @return Description text.
	*/
	zSTRING& GetDescription(void){XCALL(0x007120E0);};
	/** This method returns the disguise guild of the item.
	* @return Disguise guild. 
	*/
	zINT GetDisguiseGuild(void){XCALL(0x007127E0);};
	/** This method returns the effect name of the item.
	* @return Effect name. 
	*/
	zSTRING GetEffectName(void){XCALL(0x00712B30);};
	/** This method returns the full damage of the item.
	* @return Full damage. 
	*/
	zINT GetFullDamage(void){XCALL(0x00712500);};
	/** This method returns the full protection of the item.
	* @return Full protection. 
	*/
	zINT GetFullProtection(void){XCALL(0x00712340);};
	/** This method returns the instance of the item.
	* @return Item instance. 
	*/
	zINT GetInstance(void){XCALL(0x00711420);};
	/** This method returns the instance name of the item.
	* @return Item instance name. 
	*/
	zSTRING GetInstanceName(void){XCALL(0x00713D30);};
	/** This method returns the instance name of the item.
	* @param  Item instance.
	* @return Item instance name. 
	*/
	zSTRING GetName(zINT instance){XCALL(0x00712880);};
	zSTRING GetSchemeName(void){XCALL(0x00698E50);};
	oTSndMaterial GetSoundMaterial(void){XCALL(0x00711430);};
	zSTRING GetScriptInstance(zSTRING & p1, zINT p2){XCALL(0x00713CC0);};
	zINT GetStateEffectFunc(zINT p1){XCALL(0x00712B80);};
	zINT GetStatus(void){XCALL(0x007128D0);};
	zSTRING & GetText(zINT p1){XCALL(0x007120F0);};
	zINT GetValue(void){XCALL(0x00712650);};
	zSTRING GetVisualChange(void){XCALL(0x00712A40);};
	zINT GetWeight(void){XCALL(0x007126B0);};
	void Init(void){XCALL(0x00711970);};
	zINT IsDeadly(void){XCALL(0x00712550);};
	zINT IsOwnedByGuild(zINT p1){XCALL(0x007127C0);};
	zINT IsOwnedByNpc(zINT p1){XCALL(0x007127A0);};
	zINT MultiSlot(void){XCALL(0x007125A0);};
	void RemoveEffect(void){XCALL(0x00712C00);};
	void RenderItem(zINT p1, zINT p2){XCALL(0x007133C0);};
	zINT RenderItem(zCWorld*, zCViewBase*, zFLOAT p3){XCALL(0x00713AC0);};
	void RenderItemPlaceCamera(zINT p1, zFLOAT p2){XCALL(0x00713800);};
	void RotateForInventory(zFLOAT p1){XCALL(0x00712F10);};
	void RotateInInventory(void){XCALL(0x007132E0);};
	virtual zINT SetByScriptInstance(const zSTRING* p1, zINT p2){XCALL(0x00712710);};
	void SetFlag(zINT p1){XCALL(0x00712710);};
	static void SetLightingSwell(zINT p1){XCALL(0x00711270);};
	virtual zINT SetVisual(zCVisual* p1){XCALL(0x00711910);};
	oCItem* SplitItem(zINT p1){XCALL(0x00712BA0);};
	zINT SplitSlot(void){XCALL(0x00712610);};
	zINT TwoHanded(void){XCALL(0x00712730);};
	int HasFlag(int){XCALL(0x007126D0);};
	void UseItem(void){XCALL(0x00712A30);};
	oCItem(zINT p1, zINT p2){XCALL(0x007115D0);};
	oCItem(void){XCALL(0x00711290);};
	oCItem(zSTRING & p1, zINT p2){XCALL(0x00711470);};
	zSTRING GetItemName(void){return this->name;};
	void SetValue(int price){this->value = price;};
	void CreateVisual(void){XCALL(0x00711930);};
	void SetAmount(int amount){*(int*)((DWORD)this+0x32C) = amount;}; // Ilosc przedmiotow jaka faktycznie znajduje sie w oCItem
	int GetAmount(void){return *(int*)((DWORD)this+0x32C);};
	void SetItemName(const zSTRING& Name)
	{
		this->name.Clear();
		this->name.Insert(0,Name);
	}
	void ClearItemName(){
		this->name.Clear();
	}
	static int GetCategory(oCItem*){XCALL(0x0070D950);};
};

class oCItemContainer
{
protected:
	int		_vtbl;
public:
	void*	contents;
	oCNpc*	npc;
	zSTRING titleText;
	int		invMode;
	int		selectedItem;
	int		offset;
	int		maxSlotsCol;
	int		imaxSlotsColScr;
	int		maxSlotsRow;
	int		maxSlotsRowScr;
	int		maxSlots;
	int	 marginTop;
	int	 marginLeft;
	zBOOL   frame;
	zBOOL   right;
	zBOOL   ownList;
	zBOOL   prepared;
	zBOOL   passive;
	int	 TransferCount;
	zCView* viewTitle;
	zCView* viewBack;
	zCView* viewItem;
	zCView* viewItemActive;
	zCView* viewItemHightlighted;
	zCView* viewItemActiveHighlighted;
	zCView* viewItemInfo;
	zCView* viewItemInfoItem;
	zCView* textView;
	zCView* viewArrowAtTop;
	zCView* viewArrowAtBottom;
	zCWorld*rndWorld;
	int	 posx;
	int	 posy;
	zBOOL   m_bManipulateItemsDisabled;
	zBOOL   m_bCanTransferMoreThanOneItem;

public:
	virtual void Activate(void){XCALL(0x00709230);};
	virtual zINT CanManipulateItems(void){XCALL(0x007050F0);};
	virtual zINT CanTransferMoreThanOneItem(void){XCALL(0x00705120);};
	virtual void Close(void){XCALL(0x00708F30);};
	static void Container_Draw(void){XCALL(0x00704B90);};
	static void Container_PrepareDraw(void){XCALL(0x00704B80);};
	static oCItem* CreateCurrencyItem(int pA){XCALL(0x00704AA0);};
	virtual void Deactivate(void){XCALL(0x00709290);};
	void DisableTransferMoreThanOneItem(int pA){XCALL(0x00705100);};
	static zINT GetCurrencyInstance(void){XCALL(0x00704A00);};
	static zSTRING & GetCurrencyInstanceName(void){XCALL(0x00704810);};
	static short GetInvMaxColumns(void){XCALL(0x00704610);};
	static short GetInvMaxRows(void){XCALL(0x00704660);};
	static zINT GetInvSplitScreen(void){XCALL(0x007045E0);};
	zINT GetMode(void){XCALL(0x007050A0);};
	virtual zSTRING GetName(void){XCALL(0x00704F10);};
	virtual oCItem* GetSelectedItem(void){XCALL(0x007092C0);};
	virtual zINT GetSelectedItemCount(void){XCALL(0x007092F0);};
	virtual void GetSize(zINT & p1, zINT & p2){XCALL(0x00706A70);};
	virtual short GetTransferCount(void){XCALL(0x00705130);};
	static zFLOAT GetValueMultiplier(void){XCALL(0x007046F0);};
	virtual void IncTransferCount(int p1){XCALL(0x00705150);};
	virtual oCItem* Insert(oCItem* p1){XCALL(0x00709360);};
	virtual zINT IsActive(void){XCALL(0x007050D0);};
	virtual zINT IsEmpty(void){XCALL(0x00709E10);};
	virtual zINT IsOpen(void){XCALL(0x00709200);};
	virtual zINT IsPassive(void){XCALL(0x00708F20);};
	virtual zINT IsSplitScreen(void){XCALL(0x00709E40);};
	virtual void Open(zINT p1, zINT p2, zINT p3){XCALL(0x00709E40);};
	virtual void OpenPassive(zINT p1, zINT p2, zINT p3){XCALL(0x007086D0);};
	virtual void Remove(oCItem* p1){XCALL(0x00709430);};
	virtual void Remove(oCItem* p1, zINT p2){XCALL(0x007094E0);};
	virtual void RemoveByPtr(oCItem* p1, zINT p2){XCALL(0x007094D0);};
	virtual void RemoveByPtr(oCItem* p1){XCALL(0x00704B50);};
	virtual void SetMode(int p1){XCALL(0x007050B0);};
	virtual void SetName(zSTRING & p1){XCALL(0x00704F60);};
	void SetTransferCount(int p1){XCALL(0x00705140);};
	zCWorld* GetRenderWorld() { return this->rndWorld; };
	static short  TransferCountToAmount(int p1){XCALL(0x007046B0);};
	zINT TransferItem(void ** p1, zINT p2, zINT p3){XCALL(0x00709F40);};
	oCItemContainer(void){XCALL(0x00704D00);};
};
