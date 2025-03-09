/*////////////////////////////////////////////////////////////////////////////
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
#ifndef __API_G2_INPUT_H__
#define __API_G2_INPUT_H__
#include "ztypes.hpp"
#include "macros.hpp"

// -- Key codes from Microsoft DirectX 7 SDK
#define KEY_ESCAPE		  0x01
#define KEY_1			   0x02
#define KEY_2			   0x03
#define KEY_3			   0x04
#define KEY_4			   0x05
#define KEY_5			   0x06
#define KEY_6			   0x07
#define KEY_7			   0x08
#define KEY_8			   0x09
#define KEY_9			   0x0A
#define KEY_0			   0x0B
#define KEY_MINUS		   0x0C	/* - on main keyboard */
#define KEY_EQUALS		  0x0D
#define KEY_BACK			0x0E	/* backspace */
#define KEY_TAB			 0x0F
#define KEY_Q			   0x10
#define KEY_W			   0x11
#define KEY_E			   0x12
#define KEY_R			   0x13
#define KEY_T			   0x14
#define KEY_Y			   0x15
#define KEY_U			   0x16
#define KEY_I			   0x17
#define KEY_O			   0x18
#define KEY_P			   0x19
#define KEY_LBRACKET		0x1A
#define KEY_RBRACKET		0x1B
#define KEY_RETURN		  0x1C	/* Enter on main keyboard */
#define KEY_LCONTROL		0x1D
#define KEY_A			   0x1E
#define KEY_S			   0x1F
#define KEY_D			   0x20
#define KEY_F			   0x21
#define KEY_G			   0x22
#define KEY_H			   0x23
#define KEY_J			   0x24
#define KEY_K			   0x25
#define KEY_L			   0x26
#define KEY_SEMICOLON	   0x27
#define KEY_APOSTROPHE	  0x28
#define KEY_GRAVE		   0x29	/* accent grave */
#define KEY_LSHIFT		  0x2A
#define KEY_BACKSLASH	   0x2B
#define KEY_Z			   0x2C
#define KEY_X			   0x2D
#define KEY_C			   0x2E
#define KEY_V			   0x2F
#define KEY_B			   0x30
#define KEY_N			   0x31
#define KEY_M			   0x32
#define KEY_COMMA		   0x33
#define KEY_PERIOD		  0x34	/* . on main keyboard */
#define KEY_SLASH		   0x35	/* / on main keyboard */
#define KEY_RSHIFT		  0x36
#define KEY_MULTIPLY		0x37	/* * on numeric keypad */
#define KEY_LMENU		   0x38	/* left Alt */
#define KEY_SPACE		   0x39
#define KEY_CAPITAL		 0x3A
#define KEY_F1			  0x3B
#define KEY_F2			  0x3C
#define KEY_F3			  0x3D
#define KEY_F4			  0x3E
#define KEY_F5			  0x3F
#define KEY_F6			  0x40
#define KEY_F7			  0x41
#define KEY_F8			  0x42
#define KEY_F9			  0x43
#define KEY_F10			 0x44
#define KEY_NUMLOCK		 0x45
#define KEY_SCROLL		  0x46	/* Scroll Lock */
#define KEY_NUMPAD7		 0x47
#define KEY_NUMPAD8		 0x48
#define KEY_NUMPAD9		 0x49
#define KEY_SUBTRACT		0x4A	/* - on numeric keypad */
#define KEY_NUMPAD4		 0x4B
#define KEY_NUMPAD5		 0x4C
#define KEY_NUMPAD6		 0x4D
#define KEY_ADD			 0x4E	/* + on numeric keypad */
#define KEY_NUMPAD1		 0x4F
#define KEY_NUMPAD2		 0x50
#define KEY_NUMPAD3		 0x51
#define KEY_NUMPAD0		 0x52
#define KEY_DECIMAL		 0x53	/* . on numeric keypad */
#define KEY_OEM_102		 0x56	/* <> or \| on RT 102-key keyboard (Non-U.S.) */
#define KEY_F11			 0x57
#define KEY_F12			 0x58
#define KEY_F13			 0x64	/*					 (NEC PC98) */
#define KEY_F14			 0x65	/*					 (NEC PC98) */
#define KEY_F15			 0x66	/*					 (NEC PC98) */
#define KEY_KANA			0x70	/* (Japanese keyboard)			*/
#define KEY_ABNT_C1		 0x73	/* /? on Brazilian keyboard */
#define KEY_CONVERT		 0x79	/* (Japanese keyboard)			*/
#define KEY_NOCONVERT	   0x7B	/* (Japanese keyboard)			*/
#define KEY_YEN			 0x7D	/* (Japanese keyboard)			*/
#define KEY_ABNT_C2		 0x7E	/* Numpad . on Brazilian keyboard */
#define KEY_NUMPADEQUALS	0x8D	/* = on numeric keypad (NEC PC98) */
#define KEY_PREVTRACK	   0x90	/* Previous Track (KEY_CIRCUMFLEX on Japanese keyboard) */
#define KEY_AT			  0x91	/*					 (NEC PC98) */
#define KEY_COLON		   0x92	/*					 (NEC PC98) */
#define KEY_UNDERLINE	   0x93	/*					 (NEC PC98) */
#define KEY_KANJI		   0x94	/* (Japanese keyboard)			*/
#define KEY_STOP			0x95	/*					 (NEC PC98) */
#define KEY_AX			  0x96	/*					 (Japan AX) */
#define KEY_UNLABELED	   0x97	/*						(J3100) */
#define KEY_NEXTTRACK	   0x99	/* Next Track */
#define KEY_NUMPADENTER	 0x9C	/* Enter on numeric keypad */
#define KEY_RCONTROL		0x9D
#define KEY_MUTE			0xA0	/* Mute */
#define KEY_CALCULATOR	  0xA1	/* Calculator */
#define KEY_PLAYPAUSE	   0xA2	/* Play / Pause */
#define KEY_MEDIASTOP	   0xA4	/* Media Stop */
#define KEY_VOLUMEDOWN	  0xAE	/* Volume - */
#define KEY_VOLUMEUP		0xB0	/* Volume + */
#define KEY_WEBHOME		 0xB2	/* Web home */
#define KEY_NUMPADCOMMA	 0xB3	/* , on numeric keypad (NEC PC98) */
#define KEY_DIVIDE		  0xB5	/* / on numeric keypad */
#define KEY_SYSRQ		   0xB7
#define KEY_RMENU		   0xB8	/* right Alt */
#define KEY_PAUSE		   0xC5	/* Pause */
#define KEY_HOME			0xC7	/* Home on arrow keypad */
#define KEY_UP			  0xC8	/* UpArrow on arrow keypad */
#define KEY_PRIOR		   0xC9	/* PgUp on arrow keypad */
#define KEY_LEFT			0xCB	/* LeftArrow on arrow keypad */
#define KEY_RIGHT		   0xCD	/* RightArrow on arrow keypad */
#define KEY_END			 0xCF	/* End on arrow keypad */
#define KEY_DOWN			0xD0	/* DownArrow on arrow keypad */
#define KEY_NEXT			0xD1	/* PgDn on arrow keypad */
#define KEY_INSERT		  0xD2	/* Insert on arrow keypad */
#define KEY_DELETE		  0xD3	/* Delete on arrow keypad */
#define KEY_LWIN			0xDB	/* Left Windows key */
#define KEY_RWIN			0xDC	/* Right Windows key */
#define KEY_APPS			0xDD	/* AppMenu key */
#define KEY_POWER		   0xDE	/* System Power */
#define KEY_SLEEP		   0xDF	/* System Sleep */
#define KEY_WAKE			0xE3	/* System Wake */
#define KEY_WEBSEARCH	   0xE5	/* Web Search */
#define KEY_WEBFAVORITES	0xE6	/* Web Favorites */
#define KEY_WEBREFRESH	  0xE7	/* Web Refresh */
#define KEY_WEBSTOP		 0xE8	/* Web Stop */
#define KEY_WEBFORWARD	  0xE9	/* Web Forward */
#define KEY_WEBBACK		 0xEA	/* Web Back */
#define KEY_MYCOMPUTER	  0xEB	/* My Computer */
#define KEY_MAIL			0xEC	/* Mail */
#define KEY_MEDIASELECT	 0xED	/* Media Select */
// Alternate names for keys, to facilitate transition from DOS.
#define KEY_BACKSPACE	   KEY_BACK			/* backspace */
#define KEY_NUMPADSTAR	  KEY_MULTIPLY		/* * on numeric keypad */
#define KEY_LALT			KEY_LMENU		   /* left Alt */
#define KEY_CAPSLOCK		KEY_CAPITAL		 /* CapsLock */
#define KEY_NUMPADMINUS	 KEY_SUBTRACT		/* - on numeric keypad */
#define KEY_NUMPADPLUS	  KEY_ADD			 /* + on numeric keypad */
#define KEY_NUMPADPERIOD	KEY_DECIMAL		 /* . on numeric keypad */
#define KEY_NUMPADSLASH	 KEY_DIVIDE		  /* / on numeric keypad */
#define KEY_RALT			KEY_RMENU		   /* right Alt */
#define KEY_UPARROW		 KEY_UP			  /* UpArrow on arrow keypad */
#define KEY_PGUP			KEY_PRIOR		   /* PgUp on arrow keypad */
#define KEY_LEFTARROW	   KEY_LEFT			/* LeftArrow on arrow keypad */
#define KEY_RIGHTARROW	  KEY_RIGHT		   /* RightArrow on arrow keypad */
#define KEY_DOWNARROW	   KEY_DOWN			/* DownArrow on arrow keypad */
#define KEY_PGDN			KEY_NEXT			/* PgDn on arrow keypad */
// Alternate names for keys originally not used on US keyboards.
#define KEY_CIRCUMFLEX	  KEY_PREVTRACK	   /* Japanese keyboard */

// g2 option keys
#define G2_OPTION_KEY_END			zSTRING("keyEnd")
#define G2_OPTION_KEY_HEAL			zSTRING("keyHeal")
#define G2_OPTION_KEY_POTION		zSTRING("keyPotion")
#define G2_OPTION_KEY_LOCK_TARGET	zSTRING("keyLockTarget")
#define G2_OPTION_KEY_PARADE		zSTRING("keyParade")
#define G2_OPTION_KEY_ACTION_RIGHT	zSTRING("keyActionRight")
#define G2_OPTION_KEY_ACTION_LEFT	zSTRING("keyActionLeft")
#define G2_OPTION_KEY_UP			zSTRING("keyUp")
#define G2_OPTION_KEY_DOWN			zSTRING("keyDown")
#define G2_OPTION_KEY_LEFT			zSTRING("keyLeft")
#define G2_OPTION_KEY_RIGHT			zSTRING("keyRight")
#define G2_OPTION_KEY_STRAFE_LEFT	zSTRING("keyStrafeLeft")
#define G2_OPTION_KEY_STRAFE_RIGHT	zSTRING("keyStrafeRight")
#define G2_OPTION_KEY_ACTION		zSTRING("keyAction")
#define G2_OPTION_KEY_SLOW			zSTRING("keySlow")
#define G2_OPTION_KEY_SMOVE			zSTRING("keySMove")
#define G2_OPTION_KEY_WEAPON		zSTRING("keyWeapon")
#define G2_OPTION_KEY_SNEAK			zSTRING("keySneak")
#define G2_OPTION_KEY_LOOK			zSTRING("keyLook")
#define G2_OPTION_KEY_LOOK_FP		zSTRING("keyLookFP")
#define G2_OPTION_KEY_INVENTORY		zSTRING("keyInventory")
#define G2_OPTION_KEY_SHOW_STATUS	zSTRING("keyShowStatus")
#define G2_OPTION_KEY_SHOW_LOG		zSTRING("keyShowLog")
#define G2_OPTION_KEY_SHOW_MAP		zSTRING("keyShowMap")

// g2 logical keys
enum LogicalKey : zWORD
{
	LOGICAL_KEY_LEFT			= 1,	// turn left
	LOGICAL_KEY_RIGHT,					// turn right
	LOGICAL_KEY_UP,						// forward  / menu up
	LOGICAL_KEY_DOWN,					// backward / menu down
	LOGICAL_KEY_ACTION,					// action
	LOGICAL_KEY_SLOW,
	LOGICAL_KEY_ACTION2,				// alternative action
	LOGICAL_KEY_WEAPON,					// toggle fight/normal mode
	LOGICAL_KEY_SMOVE			= 11,
	LOGICAL_KEY_SMOVE2,
	LOGICAL_KEY_SHIFT,
	LOGICAL_KEY_END,
	LOGICAL_KEY_INVENTORY,				// open/close inventory
	LOGICAL_KEY_LOOK,
	LOGICAL_KEY_SNEAK,
	LOGICAL_KEY_STRAFELEFT,				// strafe left
	LOGICAL_KEY_STRAFERIGHT,			// strafe right
	LOGICAL_KEY_SCREEN_STATUS,			// open/close character status
	LOGICAL_KEY_SCREEN_LOG,				// open/close quest log
	LOGICAL_KEY_SCREEN_MAP,				// open/close map
	LOGICAL_KEY_LOOK_FP,
	LOGICAL_KEY_LOCK_TARGET,
	LOGICAL_KEY_PARADE,
	LOGICAL_KEY_ACTIONLEFT,
	LOGICAL_KEY_ACTIONRIGHT,
	LOGICAL_KEY_LAME_POTION,
	LOGICAL_KEY_LAME_HEAL
};

class zCInput
{
private:
	char _data[0x58];
public:
	zINT AnyKeyPressed(void){XCALL(0x004D51F0);};
	void ClearKeyBuffer(void){XCALL(0x004D55D0);};
	zFLOAT GetAutoRepeatDelayRate(void){XCALL(0x004D5320);};
	zBYTE GetChar(void){XCALL(0x004D5510);};
	zBYTE GetChar(zINT pA, zINT pB){XCALL(0x004D5370);};
	zINT GetMouseButtonPressedLeft(void){XCALL(0x004D5790);};
	zINT GetMouseButtonPressedMid(void){XCALL(0x004D57A0);};
	zINT GetMouseButtonPressedRight(void){XCALL(0x004D57B0);};
	void GetMouseFlipXY(zINT& x, zINT& y){XCALL(0x004D5850);};
	zINT GetMouseIdle(void){XCALL(0x004D4CA0);};
	void GetMousePos(zFLOAT& pA, zFLOAT& pB, zFLOAT& pC){XCALL(0x004D5730);};	//3 args?
	void GetMouseSensitivity(zFLOAT& pA, zFLOAT& pB){XCALL(0x004D5810);};
	zFLOAT GetState(int pA){XCALL(0x004D4D30);};
	zINT GetToggled(int pA){XCALL(0x004D5020);};
	zINT KeyPressed(zINT dx7_key){XCALL(0x004D51C0);};
	zINT KeyToggled(zINT dx7_key){XCALL(0x004D51D0);};
	void ProcessInputEvents(void){XCALL(0x004D5700);};
	void ResetRepeatKey(zINT pA){XCALL(0x004D5330);};
	void SetAutoRepeatDelayRate(zFLOAT pA, zFLOAT pB){XCALL(0x004D5300);};
	void SetFocusWindow(HWND pA){XCALL(0x004D4A00);};	//is there any else window except main window?
	void SetAutoRepeatDelayRate(zINT pA, zINT pB){XCALL(0x004D54D0);};
	void SetMouseFlipXY(zINT x, zINT y){XCALL(0x004D5830);};
	zINT SetState(int pA, zINT pB){XCALL(0x004D4E00);};
	//.text:004CC020 ; int __stdcall zCInput__BindOption(int, int, int, int, int, int, void *Memory, int, int)
	// WTF happened to that parameters list?!?
	/** Binds the keys in keyList (if any) and the keys defined in the option to the logicalKey.
	* This method unbinds all existing key bindings before applying the new ones.
	* @param option		Option in gothic2.ini where to get keys from (can be invalid)
	* @param logicalKey	Logical key to which the keys are getting bound to
	* @param keyList	List of additional keys (can be empty)
	*/
	void BindOption(const zSTRING option, LogicalKey logicalKey, zCArray<zWORD> keyList){XCALL(0x004CC020)};
	// emulating missing method
	/** Unbinds the specified logical key (action)
	* @param logicalKey	Logical key to unbind
	*/
	void Unbind(LogicalKey logicalKey){BindOption(zSTRING(""), logicalKey, zCArray<zWORD>());};
	// emulating missing method
	void Bind(LogicalKey logicalKey, zCArray<zWORD> keyList){BindOption(zSTRING(""), logicalKey, keyList);};
	int IsBinded(LogicalKey logicalKey, zWORD key){XCALL(0x004CC370)};
	int IsBindedToggled(LogicalKey logicalKey, zWORD key){XCALL(0x004CC470)};
	LogicalKey GetFirstBindedLogicalKey(zWORD key){XCALL(0x004CC5D0)};
	void BindKeys(int){XCALL(0x004CFE00)}; //(int key) I guess|Pampi
	static zWORD GetControlValueByName(const zSTRING&){XCALL(0x004D15D0)};
	static zSTRING GetNameByControlValue(zWORD){XCALL(0x004D1680)};
	void SetKey(zINT pA, zINT pB){XCALL(0x004D54D0);};
	zCInput(HWND pA){XCALL(0x004D4A10);};
	inline static zCInput*	GetInput(){ return *(zCInput**)0x008D1650; };
};
#endif  //__API_G2_INPUT_H__
