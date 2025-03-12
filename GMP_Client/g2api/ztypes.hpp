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

#include "osheader.hpp"

#define NULL 0
#define zNEW(x) (new x)

struct zVEC2;
struct zVEC3;
struct zVEC4;
struct zMAT3;
struct zMAT4;

typedef int				zINT;
typedef int				zBOOL;
typedef float			zFLOAT;
typedef float			zREAL;
typedef unsigned char	zBYTE; 
typedef unsigned short	zWORD;  
typedef unsigned long	zDWORD;
typedef float			zREAL;  
typedef unsigned int	zUINT32;
typedef int				zTFrameCtr;
typedef int				zTModelAniID;
typedef zVEC3			zPOINT3;

/** 2D Vector. */
struct zVEC2
{
	union
	{
		struct 
		{
			float x, y;
		};
		float f[2];
	};

	// -- constructors
	/** Insert description. 
	*/
	zVEC2();

	/** Insert description. 
	*/
	zVEC2( float _x, float _y );

	/** Insert description. 
	*/
	zVEC2( const float* pf );

	/** Insert description. 
	*/
	zVEC2( const zVEC2& v );

	// -- functions
	/** Insert description. 
	*/
	void Clear();

	/** Insert description. 
	*/
	void Set( float _x, float _y );

	// -- casts
	/** Insert description. 
	*/
	operator float* ();

	/** Insert description. 
	*/
	operator const float* ();

	// -- assignment operators
	/** Insert description. 
	*/
	zVEC2& operator += ( const zVEC2 & v );

	/** Insert description. 
	*/
	zVEC2& operator -= ( const zVEC2 & v );

	/** Insert description. 
	*/
	zVEC2& operator *= ( float f );

	/** Insert description. 
	*/
	zVEC2& operator /= ( float f );

	// -- unary operators
	/** Insert description. 
	*/
	zVEC2  operator + () const;

	/** Insert description. 
	*/
	zVEC2  operator - () const;

	// -- binary operators
	/** Insert description. 
	*/
	zVEC2  operator + ( const zVEC2& v ) const;

	/** Insert description. 
	*/
	zVEC2  operator - ( const zVEC2& v ) const;

	/** Insert description. 
	*/
	zVEC2  operator * ( float f ) const;

	/** Insert description. 
	*/
	zVEC2  operator / ( float f ) const;

	// -- logical operators
	/** Insert description. 
	*/
	bool operator == ( const zVEC2& v ) const;
	/** Insert description. 
	*/
	bool operator != ( const zVEC2& v ) const;
};

/** 3D Vector. */
struct zVEC3
{
	union
	{
		struct 
		{
			float x, y, z;
		};
		float f[3];
	};

	// -- constructors
	/** Insert description. 
	*/
	zVEC3();

	/** Insert description. 
	*/
	zVEC3( float _x, float _y, float _z );

	/** Insert description. 
	*/
	zVEC3( const float* pf );

	/** Insert description. 
	*/
	zVEC3( const zVEC3& v );

	// -- functions
	/** Insert description. 
	*/
	inline void Clear();

	/** Insert description. 
	*/
	inline void Set( float _x, float _y, float _z );

	// -- casts
	/** Insert description. 
	*/
	operator float* ();

	/** Insert description. 
	*/
	operator const float* ();

	// -- assignment operators
	/** Insert description. 
	*/
	zVEC3& operator += ( const zVEC3 & v );

	/** Insert description. 
	*/
	zVEC3& operator -= ( const zVEC3 & v );

	/** Insert description. 
	*/
	zVEC3& operator *= ( float f );

	/** Insert description. 
	*/
	zVEC3& operator /= ( float f );

	// -- unary operators
	/** Insert description. 
	*/
	zVEC3  operator + () const;

	/** Insert description. 
	*/
	zVEC3  operator - () const;

	// -- binary operators
	/** Insert description. 
	*/
	zVEC3  operator + ( const zVEC3& v ) const;

	/** Insert description. 
	*/
	zVEC3  operator - ( const zVEC3& v ) const;

	/** Insert description. 
	*/
	zVEC3  operator * ( float f ) const;

	/** Insert description. 
	*/
	zVEC3  operator / ( float f ) const;

	// -- logical operators
	/** Insert description. 
	*/
	bool operator == ( const zVEC3& v ) const;
	/** Insert description. 
	*/
	bool operator != ( const zVEC3& v ) const;
};

/** 4D Vector */
struct zVEC4
{
	union
	{
		struct 
		{
			float x, y, z, w;
		};
		float f[4];
	};

	// -- constructors
	/** Insert description. 
	*/
	zVEC4();

	/** Insert description. 
	*/
	zVEC4( float _x, float _y, float _z, float _w );

	/** Insert description. 
	*/
	zVEC4( const float* pf );

	/** Insert description. 
	*/
	zVEC4( const zVEC4& v );

	// -- functions
	/** Insert description. 
	*/
	inline void Clear();

	/** Insert description. 
	*/
	inline void Set( float _x, float _y, float _z, float _w );

	// -- casts
	/** Insert description. 
	*/
	operator float* ();

	/** Insert description. 
	*/
	operator const float* ();

	// -- assignment operators
	/** Insert description. 
	*/
	zVEC4& operator += ( const zVEC4 & v );

	/** Insert description. 
	*/
	zVEC4& operator -= ( const zVEC4 & v );

	/** Insert description. 
	*/
	zVEC4& operator *= ( float f );

	/** Insert description. 
	*/
	zVEC4& operator /= ( float f );

	// -- unary operators
	/** Insert description. 
	*/
	zVEC4  operator + () const;

	/** Insert description. 
	*/
	zVEC4  operator - () const;

	// -- binary operators
	/** Insert description. 
	*/
	zVEC4  operator + ( const zVEC4& v ) const;

	/** Insert description. 
	*/
	zVEC4  operator - ( const zVEC4& v ) const;

	/** Insert description. 
	*/
	zVEC4  operator * ( float f ) const;

	/** Insert description. 
	*/
	zVEC4  operator / ( float f ) const;

	// -- logical operators
	/** Insert description. 
	*/
	bool operator == ( const zVEC4& v ) const;
	/** Insert description. 
	*/
	bool operator != ( const zVEC4& v ) const;
};

/** 3x4 Matrix */
struct zMAT3
{
	union {
		struct {
			float	_11, _12, _13, _14;
			float	_21, _22, _23, _24;
			float	_31, _32, _33, _34;
		};
		float m[3][4];
	};

	/** Insert description. 
	*/
	zMAT3() {};

	/** Insert description. 
	*/
	zMAT3( const zMAT3 & _m );

	/** Insert description. 
	*/
	zMAT3(float f11, float f12, float f13, float f14,
		float f21, float f22, float f23, float f24,
		float f31, float f32, float f33, float f34);

	//  -- assignment operators
	/** Insert description. 
	*/
	zMAT3& operator *= ( const zMAT3 & _m );

	/** Insert description. 
	*/
	zMAT3& operator /= ( const zMAT3 & _m );

	/** Insert description. 
	*/
	zMAT3& operator += ( const zMAT3 & _m );

	/** Insert description. 
	*/
	zMAT3& operator -= ( const zMAT3 & _m );

	/** Insert description. 
	*/
	zMAT3& operator *= ( float f );
	/** Insert description. 
	*/
	zMAT3& operator /= ( float f );

	// -- unary operators
	/** Insert description. 
	*/
	zMAT3  operator + () const;

	/** Insert description. 
	*/
	zMAT3  operator - () const;

	// -- binary operators
	/** Insert description. 
	*/
	zMAT3 operator * ( const zMAT3 & _m );

	/** Insert description. 
	*/
	zMAT3 operator / ( const zMAT3 & _m );

	/** Insert description. 
	*/
	zMAT3 operator + ( const zMAT3 & _m );

	/** Insert description. 
	*/
	zMAT3 operator - ( const zMAT3 & _m );

	/** Insert description. 
	*/
	zMAT3 operator * ( float f );

	/** Insert description. 
	*/
	zMAT3 operator / ( float f );

	// -- logical operators
	/** Insert description. 
	*/
	bool operator == ( const zMAT3 & _m ) const;

	/** Insert description. 
	*/
	bool operator != ( const zMAT3 & _m ) const;
};

/** 4x4 Matrix */
struct zMAT4
{
	union {
		struct {
			float	_11, _12, _13, _14;
			float	_21, _22, _23, _24;
			float	_31, _32, _33, _34;
			float	_41, _42, _43, _44;
		};
		float m[4][4];
	};

	/** Insert description. 
	*/
	zMAT4() {};

	/** Insert description. 
	*/
	zMAT4( const zMAT4 & _m );

	/** Insert description. 
	*/
	zMAT4( float f11, float f12, float f13, float f14,
		float f21, float f22, float f23, float f24,
		float f31, float f32, float f33, float f34,
		float f41, float f42, float f43, float f44 );

	//  -- assignment operators
	/** Insert description. 
	*/
	zMAT4& operator *= ( const zMAT4 & _m );

	/** Insert description. 
	*/
	zMAT4& operator /= ( const zMAT4 & _m );

	/** Insert description. 
	*/
	zMAT4& operator += ( const zMAT4 & _m );

	/** Insert description. 
	*/
	zMAT4& operator -= ( const zMAT4 & _m );

	/** Insert description. 
	*/
	zMAT4& operator *= ( float f );

	/** Insert description. 
	*/
	zMAT4& operator /= ( float f );

	// -- unary operators
	/** Insert description. 
	*/
	zMAT4  operator + () const;
	/** Insert description. 
	*/
	zMAT4  operator - () const;

	// -- binary operators
	/** Insert description. 
	*/
	zMAT4 operator * ( const zMAT4 & _m );

	/** Insert description. 
	*/
	zMAT4 operator / ( const zMAT4 & _m );

	/** Insert description. 
	*/
	zMAT4 operator + ( const zMAT4 & _m );

	/** Insert description. 
	*/
	zMAT4 operator - ( const zMAT4 & _m );

	/** Insert description. 
	*/
	zMAT4 operator * ( float f );

	/** Insert description. 
	*/
	zMAT4 operator / ( float f );

	// -- logical operators
	/** Insert description. 
	*/
	bool operator == ( const zMAT4 & _m ) const;

	/** Insert description. 
	*/
	bool operator != ( const zMAT4 & _m ) const;

	zVEC3 GetTranslation() const { return zVEC3(m[0][3], m[1][3], m[2][3]); };
};

/** Insert description. */
struct zTBBox3D 
{
	zVEC3 bbox3D_mins;
	zVEC3 bbox3D_maxs;
};

/** Insert description. */
struct zTBSphere3D 
{
	zVEC3 bsphere3D_center;
	float bsphere3D_radius;
};

/** Array class wrapper
* @attention: The classes zCList, zCArray, zCListSort, zCArraySort, zCTree, etc. are not intended for regular usage. Please use the STL equivalents for your coding!
*/
template<class T>
class zCArray
{
protected:
	T*  m_array;
	int m_numAlloc;
	int m_numInArray;

private:
	void Reallocate(const int nSize)
	{
		if(nSize == 0)
			return;

		T* pArray = new T[this->m_numAlloc+nSize];

		if(this->m_numInArray > 0)
		{
			for(int i = 0; i < this->m_numInArray; i++)
				pArray[i] = m_array[i];
		};

		delete [] this->m_array;
		this->m_numAlloc += nSize;
		this->m_array	= pArray;
	};	

public:
	/** Insert description. 
	*/
	zCArray()
	{
		this->m_array		= NULL;
		this->m_numAlloc	= 0;
		this->m_numInArray	= 0;
	};

	/** Insert description. 
	*/
	~zCArray()
	{
		this->Clear();
	};

	/** Insert description. 
	*/
	const T& operator [] (const unsigned int pos) const
	{
		if((int)pos <= this->m_numInArray)
			return this->m_array[pos];
	};

	/** Insert description. 
	*/
	T& operator [] (const unsigned int pos)
	{
		if((int)pos <= this->m_numInArray)
			return this->m_array[pos];
	};

	/** Insert description. 
	*/
	void Clear()
	{
		this->m_numAlloc	= 0;
		this->m_numInArray	= 0;
		if(this->m_array != NULL)
		{
			delete[] this->m_array;
			this->m_array = NULL;
		};
	};

	/**
	 * @brief Adds an element to the zCArray.
	 * 
	 * This method adds an element to the zCArray. If the preallocated memory of the zCArray
	 * is full then this method will allocate one additional field to store the the new element.
	 * This behaviour saves memory but is very inefficient when adding many elements as memory
	 * is allocated on every method call when the preallocated area is full.
	 *
	 * This method can be interleaved with PushBackFast.
	 */
	void PushBack(const T& in)
	{
		if((this->m_numInArray + 1) > this->m_numAlloc) // -- PB throws around with memory. But we don't, so we use the allocated space and don't allocate more and more memory... (memory leak!?)
			Reallocate(1);

		this->m_array[this->m_numInArray++] = in;
	};

	/**
	* @brief Adds an element to the zCArray.
	* 
	* This method adds an element to the zCArray. If the preallocated memory of the zCArray
	* is full then this method will double the size of the preallocated memory. This might lead
	* to the worst case situation where twice as much memory than needed is used for this zCArray.
	* Use this method in often created/short lived zCArrays.
	*
	* This method can be interleaved with PushBack.
	*/
	void PushBackFast(const T& in)
	{
		if((this->m_numInArray + 1) > this->m_numAlloc) // -- Let's throw around with memory like PB does for performance's sake.
			Reallocate(this->m_numAlloc*2);

		this->m_array[this->m_numInArray++] = in;
	};

	/** Insert description. 
	*/
	const T& GetItem(const unsigned int pos)
	{
		if((pos <= this->m_numInArray) && (pos <= this->m_numAlloc))
			return this->m_array[pos];
	};

	/** Insert description. 
	*/
	unsigned int GetSize(void)
	{
		return (unsigned int)this->m_numInArray;
	};

	/** Insert description. 
	*/
	int Search(const T& item)
	{
		for (size_t i = 0; i < GetSize(); i++)
		{
			if (m_array[i] == item)
				return i;
		}

		return -1;
	};

	/** Insert description. 
	*/
	bool IsInList(const T& item)
	{
		for (size_t i = 0; i < GetSize(); i++)
		{
			if (m_array[i] == item)
				return true;
		}

		return false;
	};
};

/** List class wrapper
* @attention: The classes zCList, zCArray, zCListSort, zCArraySort, zCTree, etc. are not intended for regular usage. Please use the STL equivalents for your coding!
*/
template<class T>
class zCList
{
protected:
	T*  data;
	zCList<T>* next;
public:
	/** Insert description. 
	*/
	zCList()
	{
		this->data = NULL;
		this->next = NULL;		
	};

	/** Insert description. 
	*/
	zCList(T* p)
	{
		this->data = p;
		this->next = NULL;
	};

	/** Insert description. 
	*/
	~zCList()
	{
		OBJ_DELETE(this->next);
	};

	/** Insert description. 
	*/
	inline bool IsInList(T* p) 
	{
		if(this->data == p)
		{
			return true;
		};

		if(!next)
		{
			return false;
		};

		return next->IsInList(p);
	};

	/** Insert description. 
	*/
	inline void Insert(T* p)
	{
		if(!next)
		{
			next = new zCList(p);
		}
		else
		{
			next->Insert(p);
		};
	};

	/** Insert description. 
	*/
	inline const T* Get(UINT idx)
	{
		zCList<T>* temp = this;
		UINT i = 0;

		while((temp = temp->next) != NULL)
		{
			if(i == idx) return temp->data;
			i++;
		};

		return NULL;
	};

	/** Insert description. 
	*/
	inline zCList<T>* GetNext() { return this->next; };

	/** Insert description. 
	*/
	inline T* GetData() { return this->data; };

	/** Insert description.
	 */
	inline UINT GetSize() {
		zCList<T>* temp = this;
		UINT i = 0;

		while ((temp = temp->next) != nullptr) {
			i++;
		};

		return i;
	};

  /** Insert description. 
	*/
	inline void SetData(T* p)
	{
		this->data = p;
	};
};

/** Sorted array class wrapper
* @attention: The classes zCList, zCArray, zCListSort, zCArraySort, zCTree, etc. are not intended for regular usage. Please use the STL equivalents for your coding!
*/
template<class T>
class zCArraySort
{
protected:
	T*				array;
	int				numAlloc;
	int				numInArray;
	int(*compare)(T *ele1,T *ele2);
};

/** Sorted list class wrapper
* @attention: The classes zCList, zCArray, zCListSort, zCArraySort, zCTree, etc. are not intended for regular usage. Please use the STL equivalents for your coding!
*/
template<class T>
class zCListSort
{
protected:
	int(*compare)(T *ele1,T *ele2);
	T*				data;
	zCListSort<T>*	next;
private:
	inline void _Insert(T* p, void* cmp) // Keep eventual compare function!
	{
		if(!next)
		{
			next = new zCListSort(p);
			next->compare = cmp;
		}
		else
		{
			next->_Insert(p, cmp);
		};
	};
public:
	/** Insert description. 
	*/
	T& operator [](int idx)
	{
		return *this->Get(idx);
	};
public:
	/** Insert description. 
	*/
	zCListSort()
	{
		this->data = NULL;
		this->next = NULL;		
	};

	/** Insert description. 
	*/
	zCListSort(T* p)
	{
		this->data = p;
		this->next = NULL;
	};

	/** Insert description. 
	*/
	~zCListSort()
	{
		OBJ_DELETE(this->next);
	};

	/** Insert description. 
	*/
	inline bool IsInList(T* p) 
	{
		if(this->data == p)
		{
			return true;
		};

		if(!next)
		{
			return false;
		};

		return next->IsInList(p);
	};

	/** Insert description. 
	*/
	inline void Insert(T* p)
	{
		next->_Insert(p, this->compare);
	};

	/** Insert description. 
	*/
	inline const T* Get(UINT idx)
	{
		zCListSort<T>* temp = this;
		UINT i = 0;

		while((temp = temp->next) != NULL)
		{
			if(i == idx) return temp->data;
			i++;
		};

		return NULL;
	};

	/** Insert description. 
	*/
	inline zCListSort<T>* GetNext() { return this->next; };

	/** Insert description. 
	*/
	inline T* GetData() { return this->data; };

	/** Insert description. 
	*/
	inline UINT GetSize()
	{
		zCListSort<T>* temp = this;
		UINT i = 0;

		while((temp = temp->next) != NULL)
		{
			i++;
		};

		return i;
	};

	/** Insert description. 
	*/
	inline void SetData(T* p)
	{
		this->data = p;
	};

	/** Insert description. 
	*/
	inline void SetCompareFunction(int(*cmp)(T*, T*))
	{
		this->compare = cmp;
	};
};

/** Tree
* @attention: The classes zCList, zCArray, zCListSort, zCArraySort, zCTree, etc. are not intended for regular usage. Please use the STL equivalents for your coding!
*/
template<class T>
class zCTree
{
	zCTree<T>*	parent;
	zCTree<T>*	firstChild;
	zCTree<T>*	next;
	zCTree<T>*	prev;
	T*			data;
};

/** ZenGin color type 
* @note: The ZenGin uses a BGRA colour type! (Don't mix it up with RGBA!)
*
*/
class zCOLOR
{
public:
	union {
		struct {
			BYTE	b;
			BYTE	g;
			BYTE	r;
			BYTE	a;
		};
		DWORD color;
	};

	// -- constructors

	/** Insert description. 
	*/
	zCOLOR();

	/** Insert description. 
	*/
	zCOLOR(zBYTE _r, zBYTE _g, zBYTE _b); // RGB

	/** Insert description. 
	*/
	zCOLOR(zBYTE _r, zBYTE _g, zBYTE _b, zBYTE _a); // RGBA

	/** Insert description. 
	*/
	zCOLOR(zDWORD _color); // BGRA

	// -- functions

	/** Insert description. 
	*/
	void Clear();

	/** Insert description. 
	*/
	void SetRGBA(BYTE _r, BYTE _g, BYTE _b, BYTE _a);

	/** Insert description. 
	*/
	void SetBGRA(BYTE _b, BYTE _g, BYTE _r, BYTE _a);

	/** Insert description. 
	*/
	void SetR(BYTE _c);

	/** Insert description. 
	*/
	void SetG(BYTE _c);

	/** Insert description. 
	*/
	void SetB(BYTE _c);

	/** Insert description. 
	*/
	void SetA(BYTE _c);

	/** Insert description. 
	*/
	BYTE GetR(void);

	/** Insert description. 
	*/
	BYTE GetG(void);

	/** Insert description. 
	*/
	BYTE GetB(void);

	/** Insert description. 
	*/
	BYTE GetA(void);
};

#include "ztypes.inl"

#include "zstring.hpp"
#include "zcobject.hpp"
