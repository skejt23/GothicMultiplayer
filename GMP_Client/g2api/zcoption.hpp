/*////////////////////////////////////////////////////////////////////////////

This file is part of the G2Ext SDK headers.

//////////////////////////////////////////////////////////////////////////////

The G2Ext SDK headers

Copyright © 2009, 2010 by Paindevs and Patrick Vogel

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
#include "zfile.hpp"

// Option Sections
#define zOPT_SEC_VIDEO *(zSTRING*)(0x008CD474)

// Option Paths
enum zTOptionPaths
{
	SYSTEM,
	WEB,
	SAVES,
	WORK_DATA,
	WORK_DATA_ANIMS,
	WORK_DATA_CUTSCENES,
	WORK_DATA_SCRIPTS_CONTENT_CUTSCENE,
	WORK_DATA_MESHES,
	WORK_DATA_SCRIPTS,
	WORK_DATA_SOUND,
	WORK_DATA_VIDEO,
	WORK_DATA_MUSIC,
	WORK_DATA_TEXTURES,
	WORK_DATA_TEXTURES_DESKTOP,
	WORK_DATA_WORLDS,
	WORK_DATA_PRESETS,
	WORK_TOOLS_DATA,
	WORK_DATA_ANIMS_COMPILED,
	WORK_DATA_MESHES_COMPILED,
	WORK_DATA_SCRIPTS_COMPILED,
	WORK_DATA_TEXTURES_COMPILED,
	WORK_TOOLS,
	INTERN,
	GOTHIC2_MAIN_DIR = 24
};

class zCOptionSection
{
public:
	zCOptionSection(const zSTRING & pA){XCALL(0x00460080);};
	~zCOptionSection(){XCALL(0x00460240);};
};

class zCOption
{
private:
	char data[664];
public:
	zCOption(void){XCALL(0x00460350);};
	int ChangeDir(enum zTOptionPaths){XCALL(0x00465160);};
	zFILE* GetDir(enum zTOptionPaths){XCALL(0x00465250);};
	zSTRING & GetDirString(enum zTOptionPaths){XCALL(0x00465260);};
	short GetNumEntries(zCOptionSection*){XCALL(0x00462A20);};
	zCOptionSection* GetSectionByName(const zSTRING& section, int){XCALL(0x00463000);};
	int Load(zSTRING file){XCALL(0x004607B0);};
	int Save(zSTRING file){XCALL(0x004616C0);};
	int Init(zSTRING cmd, bool extra_infos = true){XCALL(0x00463C20);};
	int SectionExists(const zSTRING& section){XCALL(0x00463250);};
	int RemoveSection(const zSTRING& section){XCALL(0x004632B0);};
	int EntryExists(const zSTRING& section, const char* entry){XCALL(0x00462950);};
	int RemoveEntry(const zSTRING& section, const char* entry){XCALL(0x00462A70);};
	int WriteBool(const zSTRING& section, const char* entry, int value, int p1 = 0){XCALL(0x00461DE0);};
	int WriteDWord(const zSTRING& section, const char* entry, DWORD value, int p1 = 0){XCALL(0x00461E60);};
	int WriteInt(const zSTRING& section, const char* entry, int value, int p1 = 0){XCALL(0x00461E30);};
	int WriteRaw(const zSTRING& section, const char* entry, void* buffer, int size, int p1 = 0){XCALL(0x004634E0);};
	int WriteReal(const zSTRING& section, const char* entry, float value, int p1 = 0){XCALL(0x00461F90);};
	int WriteString(const zSTRING& section, const char* entry, zSTRING value, int p1 = 0){XCALL(0x00461FD0);};
	int WriteBool(const zSTRING& section, const zSTRING& entry, zBOOL value, int p1 = 0){XCALL(0x004638A0);};
	int WriteDWord(const zSTRING& section, const zSTRING& entry, DWORD value, int p1 = 0){XCALL(0x004638E0);};
	int WriteInt(const zSTRING& section, const zSTRING& entry, int value, int p1 = 0){XCALL(0x004638C0);};
	int WriteRaw(const zSTRING& section, const zSTRING& entry, void* buffer, int size, int p1 = 0){XCALL(0x004639E0);};
	int WriteReal(const zSTRING& section, const zSTRING& entry, float value, int p1 = 0){XCALL(0x00463900);};
	int WriteString(const zSTRING& section, const zSTRING& entry, zSTRING value, int p1 = 0){XCALL(0x00463930);};
	int ReadBool(const zSTRING& section, const char* entry, int default_value = false){XCALL(0x00462160);};
	DWORD ReadDWord(const zSTRING& section, const char* entry, DWORD default_value = 0){XCALL(0x004624F0);};
	int ReadInt(const zSTRING& section, const char* entry, int default_value = 0){XCALL(0x00462390);};
	int ReadRaw(const zSTRING& section, const char* entry, void*& buffer, void* default_value = NULL, int defaultsize = 0){XCALL(0x004636C0);};
	float ReadReal(const zSTRING& section, const char* entry, float default_value = 0.0f){XCALL(0x00462650);};
	zSTRING ReadString(const zSTRING& section, const char* entry, const char* default_value = ""){XCALL(0x004627E0);};
	zBOOL ReadBool(const zSTRING& section, const zSTRING& entry, int default_value = 0){XCALL(0x00463A00);};
	DWORD ReadDWord(const zSTRING& section, const zSTRING& entry, DWORD default_value = 0){XCALL(0x00463A40);};
	int ReadInt(const zSTRING& section, const zSTRING& entry, int default_value = 0){XCALL(0x00463A20);};
	int ReadRaw(const zSTRING& section, const zSTRING& entry, void* default_value = NULL, int defaultsize = 0){XCALL(0x00463AD0);};
	float ReadReal(const zSTRING& section, const zSTRING& entry, float default_value = 0.0f){XCALL(0x00463A60);};
	zSTRING ReadReal(const zSTRING& section, const zSTRING& entry, const char* default_value = ""){XCALL(0x00463A90);};
	int Parm(const zSTRING& param){XCALL(0x00465270);};
	zSTRING ParmValue(const zSTRING& param){XCALL(0x00465380);};
	float ParmValueFloat(const zSTRING& param){XCALL(0x00465940);};
	int ParmValueInt(const zSTRING& param){XCALL(0x00465780);};
	zSTRING ParmValueRaw(const zSTRING& param){XCALL(0x00465580);};
	inline static zCOption* GetOptionGame() { return *(zCOption**)0x008CD98C; };
	inline static zCOption* GetOption() { return *(zCOption**)0x008CD988; };
};
