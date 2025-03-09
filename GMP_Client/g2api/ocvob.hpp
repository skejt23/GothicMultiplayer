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
#ifndef __API_G2_OCVOB_H__
#define __API_G2_OCVOB_H__
#include "ztypes.hpp"
#include "macros.hpp"
#include "zcobject.hpp"
#include "zcvob.hpp"

class oCVob : public zCVob
{	
public:
	int GetInstance(void){XCALL(0x0077D2E0);};
	static void ClearDebugList(void){XCALL(0x0077B730);};
};
#endif  //__API_G2_OCVOB_H__