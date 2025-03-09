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

#include "ztypes.hpp"
#include "macros.hpp"

class oCWorldTimer
{
private:
	char m_data[8];
public:
	int GetDay(void){XCALL(0x00780DD0)};
	float GetFullTime(void){XCALL(0x00780E80)};
	void GetTime(int & hour, int & minute){XCALL(0x00780DF0)};
	zSTRING GetTimeString(void){XCALL(0x00780EC0)};
	int IsDay(void){XCALL(0x00781280)};
	int IsLater(int hour, int minute){XCALL(0x00781110)};
	int IsLaterEqual(int hour, int minute){XCALL(0x00781150)};
	int IsNight(void){XCALL(0x00781300)};
	int IsTimeBetween(int p1, int p2, int p3, int p4){XCALL(0x00781190)};
	void SetDay(int day){XCALL(0x00780DE0)};
	void SetFullTime(float p1){XCALL(0x00780E90)};
	void SetTime(int hour, int minute){XCALL(0x00780E40)};
	void Timer(void){XCALL(0x00780D80)};
	oCWorldTimer(void){XCALL(0x00780D60)};
};
