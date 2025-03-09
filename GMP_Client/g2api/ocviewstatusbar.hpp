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
#ifndef __OCVIEWSTATUSBAR_H_INCLUDED__
#define __OCVIEWSTATUSBAR_H_INCLUDED__

#include "ztypes.hpp"
#include "macros.hpp"
#include "zcview.hpp"

class oCViewStatusBar : public zCView
{
private:
	char _data[0x1024];
public:
	oCViewStatusBar(int x1, int y1, int x2, int y2, zTviewID ItemID = VIEW_ITEM){XCALL(0x0043D540);};
	oCViewStatusBar(void){XCALL(0x0043D410);};
	void Init(int pA, int pB, float pC){XCALL(0x0043D620);};
	void SetTextures(zSTRING & pA, zSTRING & pB, zSTRING & pC, zSTRING & pD){XCALL(0x0043D7B0);};
	void SetValue(float pA){XCALL(0x0043DD90);};
	void Init(int pA, int pB, int pC, int pD, zTviewID pE = VIEW_ITEM){XCALL(0x007A5B20);};
	void SetAlphaBlendFunc(zTRnd_AlphaBlendFunc p1){XCALL(0x007A6070);};
	void SetRange(float pA, float pB){XCALL(0x0043DCB0);};
	void SetMaxRange(float pA, float pB){XCALL(0x0043DBA0);};
	void SetPos(int x, int y){XCALL(0x007A75B0);};
	void SetSize(int x, int y){XCALL(0x007A77A0);};
	void GetPos(int & x, int & y){XCALL(0x007A7660);};
	void GetSize(int & x, int & y){XCALL(0x007A7810);};
	void SetPreview(float pA){XCALL(0x0043DD80);};
};
#endif  //__OCVIEWSTATUSBAR_H_INCLUDED__