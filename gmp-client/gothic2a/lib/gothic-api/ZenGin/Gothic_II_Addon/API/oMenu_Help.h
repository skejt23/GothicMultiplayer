// Supported with union (c) 2018-2023 Union team
// Licence: GNU General Public License

#ifndef __OMENU__HELP_H__VER3__
#define __OMENU__HELP_H__VER3__

#include "zMenu.h"

namespace Gothic_II_Addon {

  // sizeof 40h
  struct oSMenuKey {
    zOPERATORS_DECLARATION()

    zSTRING name;         // sizeof 14h    offset 00h
    zSTRING internalName; // sizeof 14h    offset 14h
    zSTRING desc;         // sizeof 14h    offset 28h
    unsigned short key;   // sizeof 02h    offset 3Ch

    oSMenuKey() {}

    // user API
    #include "oSMenuKey.inl"
  };

  // sizeof CD0h
  class oCMenu_Help : public zCMenu {
  public:
    zOPERATORS_DECLARATION()

    zCArray<oSMenuKey> keys; // sizeof 0Ch    offset CC4h

    oCMenu_Help() : zCtor( zCMenu ) {}

    // user API
    #include "oCMenu_Help.inl"
  };

  // sizeof 04h
  class oCHelpScreen {
  public:
    zOPERATORS_DECLARATION()

    oCMenu_Help* help; // sizeof 04h    offset 00h

    oCHelpScreen() {}

    // user API
    #include "oCHelpScreen.inl"
  };

} // namespace Gothic_II_Addon

#endif // __OMENU__HELP_H__VER3__