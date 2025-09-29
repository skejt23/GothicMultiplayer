// Supported with union (c) 2018-2023 Union team
// Licence: GNU General Public License

#ifndef __ZMEM_POOL_H__VER3__
#define __ZMEM_POOL_H__VER3__

namespace Gothic_II_Addon {

  typedef enum {
    zVOLATILE_OVERFLOW_FORBID,
    zVOLATILE_OVERFLOW_WRAP,
    zVOLATILE_OVERFLOW_RESIZE
  } zTVolatileOverflowMode;

  // sizeof 08h
  class zCMemPoolBlock {
  public:
    zCMemPoolBlock* next; // sizeof 04h    offset 00h
    void* data;           // sizeof 04h    offset 04h

    zCMemPoolBlock() {}
    static void* operator new(size_t size) { return shi_malloc( size ); };
    static void operator delete( void* ) zCall( 0x00566670 );

    // user API
    #include "zCMemPoolBlock.inl"
  };

  // sizeof 24h
  class zCVolatileMemoryBase {
  public:
    zOPERATORS_DECLARATION()

    size_t size;                         // sizeof 04h    offset 04h
    size_t num;                          // sizeof 04h    offset 08h
    size_t nextfree;                     // sizeof 04h    offset 0Ch
    size_t maxAllocated;                 // sizeof 04h    offset 10h
    int maxFill;                         // sizeof 04h    offset 14h
    zTVolatileOverflowMode overflowMode; // sizeof 04h    offset 18h
    int overflow;                        // sizeof 04h    offset 1Ch
    void* data;                          // sizeof 04h    offset 20h

    zCVolatileMemoryBase() {}
    void zCVolatileMemoryBase_OnInit( unsigned int, unsigned int, zTVolatileOverflowMode ) zCall( 0x00566070 );
    void zCVolatileMemoryBase_OnInit( unsigned int, zTVolatileOverflowMode )               zCall( 0x005660D0 );
    zCVolatileMemoryBase( unsigned int a0, unsigned int a1, zTVolatileOverflowMode a2 )    zInit( zCVolatileMemoryBase_OnInit( a0, a1, a2 ));
    zCVolatileMemoryBase( unsigned int a0, zTVolatileOverflowMode a1 )                     zInit( zCVolatileMemoryBase_OnInit( a0, a1 ));
    ~zCVolatileMemoryBase()                                                                zCall( 0x00566110 );
    void Init( unsigned int )                                                              zCall( 0x00566140 );
    void* Alloc()                                                                          zCall( 0x00566160 );
    unsigned int Mark()                                                                    zCall( 0x00566300 );
    void Restore( unsigned int )                                                           zCall( 0x00566310 );
    void MaxFillPercentage( int )                                                          zCall( 0x005663C0 );
    virtual void Destroyed( void* )                                                        zCall( 0x005660C0 );

    // user API
    #include "zCVolatileMemoryBase.inl"
  };

  // sizeof 1Ch
  class zCMemPoolBase {
  public:
    zOPERATORS_DECLARATION()

    size_t size;            // sizeof 04h    offset 00h
    size_t blocksize;       // sizeof 04h    offset 04h
    zCMemPoolBlock* blocks; // sizeof 04h    offset 08h
    void* freeList;         // sizeof 04h    offset 0Ch
    size_t numFree;         // sizeof 04h    offset 10h
    size_t preallocate;     // sizeof 04h    offset 14h
    int forcePrealloc;      // sizeof 04h    offset 18h

    zCMemPoolBase() {}
    void zCMemPoolBase_OnInit( unsigned int )                             zCall( 0x005663D0 );
    void zCMemPoolBase_OnInit( unsigned int, unsigned int, unsigned int ) zCall( 0x00566400 );
    zCMemPoolBase( unsigned int a0 )                                      zInit( zCMemPoolBase_OnInit( a0 ));
    zCMemPoolBase( unsigned int a0, unsigned int a1, unsigned int a2 )    zInit( zCMemPoolBase_OnInit( a0, a1, a2 ));
    ~zCMemPoolBase()                                                      zCall( 0x00566440 );
    void* Alloc()                                                         zCall( 0x005664E0 );
    void Free( void* )                                                    zCall( 0x00566680 );
    void PreAlloc( unsigned int, int )                                    zCall( 0x005666C0 );
    void PoolAdd( void*, int, int )                                       zCall( 0x005668C0 );
    static void SetMemDebug( char const*, char const*, int )              zCall( 0x00566A80 );
    static void DisablePools( int )                                       zCall( 0x00566AA0 );

    // static properties
    static const char*& s_className;
    static const char*& s_fileName;
    static int& s_line;
    static int& s_disablePools;

    // user API
    #include "zCMemPoolBase.inl"
  };

  template <class T> class zCVolatileMemory : public zCVolatileMemoryBase {
    virtual void Destroyed(void* ptr) { reinterpret_cast<T*>(ptr)->~T(); }
  public:
    zCVolatileMemory( size_t num, zTVolatileOverflowMode overflow = zVOLATILE_OVERFLOW_FORBID ) : zCVolatileMemoryBase(sizeof(T), num, overflow) {}
    zCVolatileMemory( zTVolatileOverflowMode overflow = zVOLATILE_OVERFLOW_FORBID ) : zCVolatileMemoryBase(sizeof(T), overflow) {}
    T* CreateNew() { return new(Alloc()) T; }
    T& Element( size_t i ) { return reinterpret_cast<T*>(data)[i]; }
    T& operator[]( size_t i ) { return Element(i); }
    int LastAllocatedIndex() { return nextfree - 1; }
    int Index(T* o) { return ((size_t)(o)-(size_t)(data)) / sizeof(T); }
  };

} // namespace Gothic_II_Addon

#endif // __ZMEM_POOL_H__VER3__