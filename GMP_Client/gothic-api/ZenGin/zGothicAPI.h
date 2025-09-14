// Supported with union (c) 2018-2023 Union team
// Licence: GNU General Public License

#ifndef __ZGOTHIC_API_H__
#define __ZGOTHIC_API_H__

#define cimport extern "C" __declspec( dllimport )
#define cexport extern "C" __declspec( dllexport )
#define cppimport __declspec( dllimport )
#define cppexport __declspec( dllexport )

// #pragma comment (lib, "ZenGin/Shw32.lib") // Handles in xmake.lua
cimport void* shi_malloc( unsigned int sz );
cimport void* shi_calloc( unsigned int num, unsigned int sz );
cimport void* shi_realloc( void* mem, unsigned int sz );
cimport void shi_free( void* mem );
cimport void shi_delete( void* mem );
cimport unsigned int shi_msize( void* mem );
cimport int shi_MemInitDefaultPool();

#define Engine_G1  1
#define Engine_G1A 2
#define Engine_G2  3
#define Engine_G2A 4

int GetGameVersion();

// gothic api for Gothic I v1.08k_mod
#define ENGINE Engine_G1
// #include "Gothic_I_Classic/G1.h"
#undef ENGINE

// gothic api for Gothic Sequel v1.12f (bin)
#define ENGINE Engine_G1A
// #include "Gothic_I_Addon/G1A.h"
#undef ENGINE

// gothic api for Gothic II classic v1.30
#define ENGINE Engine_G2
// #include "Gothic_II_Classic/G2.h"
#undef ENGINE

// gothic api for Gothic II NOTR v2.6 (fix)
#define ENGINE Engine_G2A
#include "Gothic_II_Addon/G2A.h"
using namespace Gothic_II_Addon;
#undef ENGINE

template<typename T>
T zSwitch( T g1, T g1a, T g2, T g2a ) {
  static int version = GetGameVersion();
	switch( version ) {
		case 1: return g1;
		case 2: return g1a;
		case 3: return g2;
		case 4: return g2a;
	}

	static T def;
	return def;
}

template<typename T>
T zSwitch( T g1, T g2a ) {
  static int version = GetGameVersion();
	switch( version ) {
		case 1: return g1;
		case 4: return g2a;
	}

	static T def;
	return def;
}

#endif // __ZGOTHIC_API_H__