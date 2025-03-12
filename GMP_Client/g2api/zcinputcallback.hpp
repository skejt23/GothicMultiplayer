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

//to my mamy callback'a na input?
#pragma once
#include "ztypes.hpp"
#include "macros.hpp"

class zCInputCallback
{
public:
	zCInputCallback(void){XCALL(0x007A53F0);};
	int GetEnableHandleEvent(void){XCALL(0x007A5560);};
	virtual int HandleEvent(int key){XCALL(0x0043D4E0);};
	void SetEnableHandleEvent(int pA){XCALL(0x007A54E0);};
	void SetHandleEventTop(void){XCALL(0x007A5470);};
	static void GetInput(void){XCALL(0x007A55C0);};
	static int DoEvents(int pA){XCALL(0x007A5590);};
};
