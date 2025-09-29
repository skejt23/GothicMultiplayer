#include <Windows.h>
#include <stdio.h>

#include "zGothicAPI.h"

#ifdef __G1
#include "Gothic_I_Classic/ClassDef.h"
#include "Gothic_I_Classic/GlobalVariables.h"
#include "Gothic_I_Classic/StaticsProperties.h"
#endif

#ifdef __G1A
#include "Gothic_I_Addon/ClassDef.h"
#include "Gothic_I_Addon/GlobalVariables.h"
#include "Gothic_I_Addon/StaticsProperties.h"
#endif

#ifdef __G2
#include "Gothic_II_Classic/ClassDef.h"
#include "Gothic_II_Classic/GlobalVariables.h"
#include "Gothic_II_Classic/StaticsProperties.h"
#endif

#ifdef __G2A
#include "Gothic_II_Addon/ClassDef.h"
#include "Gothic_II_Addon/GlobalVariables.h"
#include "Gothic_II_Addon/StaticsProperties.h"
#endif


static void WriteLogMessage( const char* message ) {
  DWORD dw;
  WriteConsole( GetStdHandle( STD_OUTPUT_HANDLE ), message, strlen( message ), &dw, nullptr);
}


int GetGameVersion() {
  static const int gameVersion =  [] {
    HMODULE module = GetModuleHandle(nullptr);
    const byte* bytecode = (byte*)module + 0x1000;

    const byte bytecodeForG1[]  = { 0xC7, 0x05, 0xEC, 0xE0, 0x85, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xC3 };
    const byte bytecodeForG1A[] = { 0xA1, 0xD4, 0x36, 0x81, 0x00, 0xA3, 0xFC, 0x24, 0x8A, 0x00, 0xC3 };
    const byte bytecodeForG2[]  = { 0xC7, 0x05, 0x5C, 0x38, 0x8B, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xC3 };
    const byte bytecodeForG2A[] = { 0xC7, 0x05, 0x14, 0x1E, 0x8C, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xC3 };
          
    if ( memcmp( bytecode, bytecodeForG1,  sizeof( bytecodeForG1  ) ) == 0) return Engine_G1;
    if ( memcmp( bytecode, bytecodeForG1A, sizeof(bytecodeForG1A) ) == 0) return Engine_G1A;
    if ( memcmp( bytecode, bytecodeForG2,  sizeof( bytecodeForG2  ) ) == 0) return Engine_G2;
    if ( memcmp( bytecode, bytecodeForG2A, sizeof(bytecodeForG2A) ) == 0) return Engine_G2A;

    return 0;
  }();
  return gameVersion;
}


static const char* GothicNamespace = nullptr;
static size_t GothicNamespaceLength = 0;


inline void DisableNamespace( char* where ) {
  DWORD protection = PAGE_READWRITE;
  VirtualProtect( where, 3, protection, &protection );
  where[1] = '@';
  where[2] = '\0';
  VirtualProtect( where, 3, protection, &protection );
}


inline const char* FindGothicNamespace( const char* str ) {
  size_t length = strlen( str );
  if( length > GothicNamespaceLength ) {
    const char* part = str + length - GothicNamespaceLength;
    if( strcmp( part, GothicNamespace ) == 0 )
      return part;
  }

  return nullptr;
}


static int DynamicCastStrcmp( const char* str1, const char* str2 ) {
  const char* strNamespace = FindGothicNamespace( str1 );
  if( strNamespace != nullptr )
    DisableNamespace( (char*)strNamespace );
  
  strNamespace = FindGothicNamespace( str2 );
  if( strNamespace != nullptr )
    DisableNamespace( (char*)strNamespace );

  return strcmp( str1, str2 );
}


static void ReplaceJumpOffset( unsigned int where, unsigned int proc ) {
  unsigned int* offset = (unsigned int*)(where + 1);
  DWORD protection = PAGE_READWRITE;
  VirtualProtect( offset, 4, protection, &protection);
  *offset = proc - where - 5;
  VirtualProtect( offset, 4, protection, &protection );
}


static void CreateJump( unsigned int where, unsigned int proc ) {
#pragma pack(push, 1)
  struct Jump {
    byte Instruction;
    unsigned int Offset;
  };
#pragma pack(pop)

  Jump* jump = (Jump*)where;

  DWORD protection = PAGE_READWRITE;
  VirtualProtect( jump, sizeof(Jump), protection, &protection);
  jump->Instruction = 0xE9;
  jump->Offset = proc - where - 5;
  VirtualProtect( jump, sizeof(Jump), protection, &protection );
}


#if _DLL == 1
static void* GetDynamicCastAddress() {
  auto rtLocation = (byte*)memmove;
  if( rtLocation[0] == 0xE9 ) {
    auto imp = &rtLocation[5] + *(unsigned int*)&rtLocation[1];
    rtLocation = **(byte***)&imp[2];
  }
  else if( (unsigned short&)rtLocation[0] == 0x25FF ) {
    auto imp = *(unsigned int*)&rtLocation[2];
    rtLocation = *(byte**)imp;
  }

  HMODULE rtModule = nullptr;
  GetModuleHandleEx(
    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
    (LPCTSTR)rtLocation,
    &rtModule );

  if( rtModule == nullptr )
    return nullptr;

  return GetProcAddress( rtModule, "__RTDynamicCast" );
}
#else
extern void* __RTDynamicCast( void*, long, void*, void*, int );
static void* GetDynamicCastAddress() {
  return __RTDynamicCast;
}
#endif


static bool DynamicCastApplyPatch() {
  int gameVersion = GetGameVersion();
  if( gameVersion == 0 ) {
    WriteLogMessage( "Unsupported game version!\n" );
    return false;
  }

  void* rtDynamicCast = GetDynamicCastAddress();
  if( rtDynamicCast == nullptr ) {
    WriteLogMessage( "__RTDynamicCast not found!\n" );
    return false;
  }

  auto strcmpProc = (unsigned int)DynamicCastStrcmp;
  auto dcastproc = (unsigned int)rtDynamicCast;
  if( gameVersion == 1 ) {
    ReplaceJumpOffset( 0x00777249, strcmpProc );
    ReplaceJumpOffset( 0x007772A9, strcmpProc );
    ReplaceJumpOffset( 0x007772DA, strcmpProc );
    ReplaceJumpOffset( 0x00777342, strcmpProc );
    ReplaceJumpOffset( 0x007773A5, strcmpProc );
    ReplaceJumpOffset( 0x007773DA, strcmpProc );
    ReplaceJumpOffset( 0x0077747A, strcmpProc );
    GothicNamespace = "@Gothic_I_Classic@@";
    CreateJump( dcastproc, 0x0077709A );
  }
  else if( gameVersion == 2 ) {
    ReplaceJumpOffset( 0x007BB009, strcmpProc );
    ReplaceJumpOffset( 0x007BB069, strcmpProc );
    ReplaceJumpOffset( 0x007BB09A, strcmpProc );
    ReplaceJumpOffset( 0x007BB102, strcmpProc );
    ReplaceJumpOffset( 0x007BB165, strcmpProc );
    ReplaceJumpOffset( 0x007BB19A, strcmpProc );
    ReplaceJumpOffset( 0x007BB23A, strcmpProc );
    GothicNamespace = "@Gothic_I_Addon@@";
    CreateJump( dcastproc, 0x007BAE5A );
  }
  else if( gameVersion == 3 ) {
    ReplaceJumpOffset( 0x007C4729, strcmpProc );
    ReplaceJumpOffset( 0x007C4789, strcmpProc );
    ReplaceJumpOffset( 0x007C47BA, strcmpProc );
    ReplaceJumpOffset( 0x007C4822, strcmpProc );
    ReplaceJumpOffset( 0x007C4885, strcmpProc );
    ReplaceJumpOffset( 0x007C48BA, strcmpProc );
    ReplaceJumpOffset( 0x007C495A, strcmpProc );
    GothicNamespace = "@Gothic_II_Classic@@";
    CreateJump( dcastproc, 0x007C457A );
  }
  else {
    ReplaceJumpOffset( 0x007D0BE9, strcmpProc );
    ReplaceJumpOffset( 0x007D0C49, strcmpProc );
    ReplaceJumpOffset( 0x007D0C7A, strcmpProc );
    ReplaceJumpOffset( 0x007D0CE2, strcmpProc );
    ReplaceJumpOffset( 0x007D0D45, strcmpProc );
    ReplaceJumpOffset( 0x007D0D7A, strcmpProc );
    ReplaceJumpOffset( 0x007D0E1A, strcmpProc );
    GothicNamespace = "@Gothic_II_Addon@@";
    CreateJump( dcastproc, 0x007D0A3A );
  }

  GothicNamespaceLength = strlen( GothicNamespace );
  return true;
}


bool DynamicCastPatchApplied = DynamicCastApplyPatch();
