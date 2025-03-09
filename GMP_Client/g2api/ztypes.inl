/*////////////////////////////////////////////////////////////////////////////

This file is part of the G2Ext SDK headers.

//////////////////////////////////////////////////////////////////////////////

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

#ifndef __ZTYPES_INL_INCLUDED__
#define __ZTYPES_INL_INCLUDED__

#ifndef INLINE
#ifdef _MSC_VER
#if (_MSC_VER >= 1200)
#define INLINE __forceinline
#else
#define INLINE __inline
#endif
#endif
#endif

///////////////////////////////////////////////////////////////////////////////
//
// 2D VECTOR
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// -- constructors

INLINE zVEC2::zVEC2() 
{ 
	this->Clear(); 
};

INLINE zVEC2::zVEC2(float _x, float _y) 
{ 
	x = _x; 
	y = _y; 
};

INLINE zVEC2::zVEC2(const float* pf) 
{ 
	x = pf[0]; 
	y = pf[1]; 
};

INLINE zVEC2::zVEC2(const zVEC2& v) 
{ 
	x = v.x; 
	y = v.y; 
};

///////////////////////////////////////////////////////////////////////////////
// -- functions

INLINE void zVEC2::Clear() 
{ 
	x = 1.0f; 
	y = 1.0f; 
};

INLINE void zVEC2::Set(float _x, float _y) 
{ 
	x = _x; 
	y = _y; 
};

///////////////////////////////////////////////////////////////////////////////
// -- casts

INLINE zVEC2::operator float* ()
{
	return (float*)&x;
};

INLINE zVEC2::operator const float* ()
{
	return (const float*)&x;
};

///////////////////////////////////////////////////////////////////////////////
// -- assignment operators

INLINE zVEC2& zVEC2::operator += ( const zVEC2 & v )
{
	x += v.x;
	y += v.y;
	return *this;
};

INLINE zVEC2& zVEC2::operator -= ( const zVEC2 & v )
{
	x -= v.x;
	y -= v.y;
	return *this;
};

INLINE zVEC2& zVEC2::operator *= ( float f )
{
	x *= f;
	y *= f;
	return *this;
};

INLINE zVEC2& zVEC2::operator /= ( float f )
{
	float inv = 1.0f / f;
	x *= inv;
	y *= inv;
	return *this;
};

///////////////////////////////////////////////////////////////////////////////
// -- unary operators

INLINE zVEC2 zVEC2::operator + () const
{
	return *this;
};

INLINE zVEC2 zVEC2::operator - () const
{
	return zVEC2(-x, -y);
};

///////////////////////////////////////////////////////////////////////////////
// -- binary operators

INLINE zVEC2 zVEC2::operator + ( const zVEC2& v ) const
{
	return zVEC2(x + v.x, y + v.y);
};

INLINE zVEC2 zVEC2::operator - ( const zVEC2& v ) const
{
	return zVEC2(x - v.x, y - v.y);
};

INLINE zVEC2 zVEC2::operator * ( float f ) const
{
	return zVEC2(x * f, y * f);
};

INLINE zVEC2 zVEC2::operator / ( float f ) const
{
	float inv = 1.0f / f;
	return zVEC2(x * inv, y * inv);
};

///////////////////////////////////////////////////////////////////////////////
// -- logical operators

INLINE bool zVEC2::operator == ( const zVEC2& v ) const
{
	return ( x == v.x && y == v.y );
};

INLINE bool zVEC2::operator != ( const zVEC2& v ) const
{
	return ( x != v.x || y != v.y );
};

///////////////////////////////////////////////////////////////////////////////
//
// 3D VECTOR
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// -- constructors

INLINE zVEC3::zVEC3() 
{ 
	this->Clear(); 
};

INLINE zVEC3::zVEC3(float _x, float _y, float _z) 
{ 
	x = _x; 
	y = _y; 
	z = _z; 
};

INLINE zVEC3::zVEC3(const float* pf) 
{ 
	x = pf[0]; 
	y = pf[1]; 
	z = pf[2]; 
};

INLINE zVEC3::zVEC3(const zVEC3& v) 
{ 
	x = v.x; 
	y = v.y; 
	z = v.z; 
};

///////////////////////////////////////////////////////////////////////////////
// -- functions

INLINE void zVEC3::Clear() 
{ 
	x = 1.0f;
	y = 1.0f; 
	z = 1.0f; 
};

INLINE void zVEC3::Set(float _x, float _y, float _z) 
{ 
	x = _x; 
	y = _y; 
	z = _z; 
};

///////////////////////////////////////////////////////////////////////////////
// -- casts

INLINE zVEC3::operator float* ()
{
	return (float*)&x;
};

INLINE zVEC3::operator const float* ()
{
	return (const float*)&x;
};

///////////////////////////////////////////////////////////////////////////////
// -- assignment operators

INLINE zVEC3& zVEC3::operator += ( const zVEC3 & v )
{
	x += v.x;
	y += v.y;
	z += v.z;
	return *this;
};

INLINE zVEC3& zVEC3::operator -= ( const zVEC3 & v )
{
	x -= v.x;
	y -= v.y;
	z -= v.z;
	return *this;
};

INLINE zVEC3& zVEC3::operator *= ( float f )
{
	x *= f;
	y *= f;
	z *= f;
	return *this;
};

INLINE zVEC3& zVEC3::operator /= ( float f )
{
	float inv = 1.0f / f;
	x *= inv;
	y *= inv;
	z *= inv;
	return *this;
};

///////////////////////////////////////////////////////////////////////////////
// -- unary operators

INLINE zVEC3 zVEC3::operator + () const
{
	return *this;
};

INLINE zVEC3 zVEC3::operator - () const
{
	return zVEC3(-x, -y, -z);
};

///////////////////////////////////////////////////////////////////////////////
// -- binary operators

INLINE zVEC3 zVEC3::operator + ( const zVEC3& v ) const
{
	return zVEC3(x + v.x, y + v.y, z - v.z);
};

INLINE zVEC3 zVEC3::operator - ( const zVEC3& v ) const
{
	return zVEC3(x - v.x, y - v.y, z - v.z);
};

INLINE zVEC3 zVEC3::operator * ( float f ) const
{
	return zVEC3(x * f, y * f, z * f);
};

INLINE zVEC3 zVEC3::operator / ( float f ) const
{
	float inv = 1.0f / f;
	return zVEC3(x * inv, y * inv, z * inv);
};
///////////////////////////////////////////////////////////////////////////////
// -- logical operators

INLINE bool zVEC3::operator == ( const zVEC3& v ) const
{
	return ( x == v.x && y == v.y && z == v.z );
};

INLINE bool zVEC3::operator != ( const zVEC3& v ) const
{
	return ( x != v.x || y != v.y || z != v.z );
};

///////////////////////////////////////////////////////////////////////////////
//
// 4D VECTOR
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// -- constructors

INLINE zVEC4::zVEC4() 
{ 
	this->Clear(); 
};

INLINE zVEC4::zVEC4(float _x, float _y, float _z, float _w) 
{ 
	x = _x;
	y = _y;
	z = _z;
	w = _w;
};

INLINE zVEC4::zVEC4(const float* pf) 
{ 
	x = pf[0];
	y = pf[1];
	z = pf[2];
	w = pf[3];
};

INLINE zVEC4::zVEC4(const zVEC4& v) 
{ 
	x = v.x;
	y = v.y;
	z = v.z;
	w = v.w;
};

///////////////////////////////////////////////////////////////////////////////
// -- functions

INLINE void zVEC4::Clear() 
{ 
	x = 1.0f;
	y = 1.0f; 
	z = 1.0f; 
	w = 1.0f;  
};

INLINE void zVEC4::Set(float _x, float _y, float _z, float _w) 
{ 
	x = _x; 
	y = _y; 
	z = _z; 
	w = _w; 
};

///////////////////////////////////////////////////////////////////////////////
// -- casts

INLINE zVEC4::operator float* ()
{
	return (float*)&x;
};

INLINE zVEC4::operator const float* ()
{
	return (const float*)&x;
};

///////////////////////////////////////////////////////////////////////////////
// -- assignment operators

INLINE zVEC4& zVEC4::operator += ( const zVEC4 & v )
{
	x += v.x;
	y += v.y;
	z += v.z;
	w += v.w;
	return *this;
};

INLINE zVEC4& zVEC4::operator -= ( const zVEC4 & v )
{
	x -= v.x;
	y -= v.y;
	z -= v.z;
	w -= v.w;
	return *this;
};

INLINE zVEC4& zVEC4::operator *= ( float f )
{
	x *= f;
	y *= f;
	z *= f;
	w *= f;
	return *this;
};

INLINE zVEC4& zVEC4::operator /= ( float f )
{
	float inv = 1.0f / f;
	x *= inv;
	y *= inv;
	z *= inv;
	w *= inv;
	return *this;
};

///////////////////////////////////////////////////////////////////////////////
// -- unary operators

INLINE zVEC4 zVEC4::operator + () const
{
	return *this;
};

INLINE zVEC4 zVEC4::operator - () const
{
	return zVEC4(-x, -y, -z, -w);
};

///////////////////////////////////////////////////////////////////////////////
// -- binary operators

INLINE zVEC4 zVEC4::operator + ( const zVEC4& v ) const
{
	return zVEC4(x + v.x, y + v.y, z - v.z, w - v.w);
};

INLINE zVEC4 zVEC4::operator - ( const zVEC4& v ) const
{
	return zVEC4(x - v.x, y - v.y, z - v.z, w - v.w);
};

INLINE zVEC4 zVEC4::operator * ( float f ) const
{
	return zVEC4(x * f, y * f, z * f, w * f);
};

INLINE zVEC4 zVEC4::operator / ( float f ) const
{
	float inv = 1.0f / f;
	return zVEC4(x * inv, y * inv, z * inv, w * inv);
};
///////////////////////////////////////////////////////////////////////////////
// -- logical operators

INLINE bool zVEC4::operator == ( const zVEC4& v ) const
{
	return (x == v.x && y == v.y && z == v.z && w == v.w);
};

INLINE bool zVEC4::operator != ( const zVEC4& v ) const
{
	return (x != v.x || y != v.y || z != v.z  || w != v.w);
};


///////////////////////////////////////////////////////////////////////////////
//
// 4x3 MATRICE
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// -- constructors

INLINE zMAT3::zMAT3( const zMAT3 & _m )
{
	memcpy_s(m, sizeof(zMAT3), &_m, 1); // this is more performant
};

INLINE zMAT3::zMAT3(float f11, float f12, float f13, float f14,
	  float f21, float f22, float f23, float f24,
	  float f31, float f32, float f33, float f34)
{
	m[0][0] = f11; m[0][1] = f12; m[0][2] = f13; m[0][3] = f14;
	m[1][0] = f21; m[1][1] = f22; m[1][2] = f23; m[1][3] = f24;
	m[2][0] = f31; m[2][1] = f32; m[2][2] = f33; m[2][3] = f34;
};

///////////////////////////////////////////////////////////////////////////////
//  -- assignment operators

INLINE zMAT3& zMAT3::operator *= ( const zMAT3 & _m )
{
	m[0][0] *= _m._11; m[0][1] *= _m._12; m[0][2] *= _m._13; m[0][3] *= _m._14;
	m[1][0] *= _m._21; m[1][1] *= _m._22; m[1][2] *= _m._23; m[1][3] *= _m._24;
	m[2][0] *= _m._31; m[2][1] *= _m._32; m[2][2] *= _m._33; m[2][3] *= _m._34;
	return *this;
};

INLINE zMAT3& zMAT3::operator /= ( const zMAT3 & _m )
{
	m[0][0] *= (1.0f / _m._11); m[0][1] *= (1.0f / _m._12); m[0][2] *= (1.0f / _m._13); m[0][3] *= (1.0f / _m._14);
	m[1][0] *= (1.0f / _m._21); m[1][1] *= (1.0f / _m._22); m[1][2] *= (1.0f / _m._23); m[1][3] *= (1.0f / _m._24);
	m[2][0] *= (1.0f / _m._31); m[2][1] *= (1.0f / _m._32); m[2][2] *= (1.0f / _m._33); m[2][3] *= (1.0f / _m._34);
	return *this;
};


INLINE zMAT3& zMAT3::operator += ( const zMAT3 & _m )
{
	m[0][0] += _m._11; m[0][1] += _m._12; m[0][2] += _m._13; m[0][3] += _m._14;
	m[1][0] += _m._21; m[1][1] += _m._22; m[1][2] += _m._23; m[1][3] += _m._24;
	m[2][0] += _m._31; m[2][1] += _m._32; m[2][2] += _m._33; m[2][3] += _m._34;
	return *this;
};

INLINE zMAT3& zMAT3::operator -= ( const zMAT3 & _m )
{
	m[0][0] -= _m._11; m[0][1] -= _m._12; m[0][2] -= _m._13; m[0][3] -= _m._14;
	m[1][0] -= _m._21; m[1][1] -= _m._22; m[1][2] -= _m._23; m[1][3] -= _m._24;
	m[2][0] -= _m._31; m[2][1] -= _m._32; m[2][2] -= _m._33; m[2][3] -= _m._34;
	return *this;
};

INLINE zMAT3& zMAT3::operator *= ( float f )
{
	m[0][0] *= f; m[0][1] *= f; m[0][2] *= f; m[0][3] *= f;
	m[1][0] *= f; m[1][1] *= f; m[1][2] *= f; m[1][3] *= f;
	m[2][0] *= f; m[2][1] *= f; m[2][2] *= f; m[2][3] *= f;
	return *this;
};

INLINE zMAT3& zMAT3::operator /= ( float f )
{
	float inv = 1.0f / f;
	m[0][0] /= inv; m[0][1] /= inv; m[0][2] /= inv; m[0][3] /= inv;
	m[1][0] /= inv; m[1][1] /= inv; m[1][2] /= inv; m[1][3] /= inv;
	m[2][0] /= inv; m[2][1] /= inv; m[2][2] /= inv; m[2][3] /= inv;
	return *this;
};

///////////////////////////////////////////////////////////////////////////////
// -- unary operators

INLINE zMAT3 zMAT3::operator + () const
{
	return *this;
};

INLINE zMAT3 zMAT3::operator - () const
{
	return zMAT3(
		-m[0][0], -m[0][1], -m[0][2],- m[0][3],
		-m[1][0], -m[1][1], -m[1][2], -m[1][3],
		-m[2][0], -m[2][1], -m[2][2], -m[2][3]
	);
};

///////////////////////////////////////////////////////////////////////////////
// -- binary operators

INLINE zMAT3 zMAT3::operator * ( const zMAT3 & _m )
{
	return zMAT3(
		m[0][0] * _m._11, m[0][1] * _m._12, m[0][2] * _m._13, m[0][3] * _m._14,
		m[1][0] * _m._21, m[1][1] * _m._22, m[1][2] * _m._23, m[1][3] * _m._24,
		m[2][0] * _m._31, m[2][1] * _m._32, m[2][2] * _m._33, m[2][3] * _m._34
		);
};

INLINE zMAT3 zMAT3::operator / ( const zMAT3 & _m )
{
	return zMAT3(
		m[0][0] * (1.0f /_m._11), m[0][1] * (1.0f /_m._12), m[0][2] * (1.0f /_m._13), m[0][3] * (1.0f /_m._14),
		m[1][0] * (1.0f /_m._21), m[1][1] * (1.0f /_m._22), m[1][2] * (1.0f /_m._23), m[1][3] * (1.0f /_m._24),
		m[2][0] * (1.0f /_m._31), m[2][1] * (1.0f /_m._32), m[2][2] * (1.0f /_m._33), m[2][3] * (1.0f /_m._34)
		);
};

INLINE zMAT3 zMAT3::operator + ( const zMAT3 & _m )
{
	return zMAT3(
		m[0][0] + _m._11, m[0][1] + _m._12, m[0][2] + _m._13, m[0][3] + _m._14,
		m[1][0] + _m._21, m[1][1] + _m._22, m[1][2] + _m._23, m[1][3] + _m._24,
		m[2][0] + _m._31, m[2][1] + _m._32, m[2][2] + _m._33, m[2][3] + _m._34
		);
};

INLINE zMAT3 zMAT3::operator - ( const zMAT3 & _m )
{
	return zMAT3(
		m[0][0] + _m._11, m[0][1] + _m._12, m[0][2] + _m._13, m[0][3] + _m._14,
		m[1][0] + _m._21, m[1][1] + _m._22, m[1][2] + _m._23, m[1][3] + _m._24,
		m[2][0] + _m._31, m[2][1] + _m._32, m[2][2] + _m._33, m[2][3] + _m._34
		);
};

INLINE zMAT3 zMAT3::operator * ( float f )
{
	return zMAT3(
		m[0][0] * f, m[0][1] * f, m[0][2] * f, m[0][3] * f,
		m[1][0] * f, m[1][1] * f, m[1][2] * f, m[1][3] * f,
		m[2][0] * f, m[2][1] * f, m[2][2] * f, m[2][3] * f
		);
};

INLINE zMAT3 zMAT3::operator / ( float f )
{
	float inv = 1.0f / f;
	return zMAT3(
		m[0][0] * inv, m[0][1] * inv, m[0][2] * inv, m[0][3] * inv,
		m[1][0] * inv, m[1][1] * inv, m[1][2] * inv, m[1][3] * inv,
		m[2][0] * inv, m[2][1] * inv, m[2][2] * inv, m[2][3] * inv
		);
};

///////////////////////////////////////////////////////////////////////////////
// -- logical operators

INLINE bool zMAT3::operator == ( const zMAT3 & _m ) const
{
	return  (
		m[0][0] == _m._11 && m[0][1] == _m._12 && m[0][2] == _m._13 && m[0][3] == _m._14 &&
		m[1][0] == _m._21 && m[1][1] == _m._22 && m[1][2] == _m._23 && m[1][3] == _m._24 &&
		m[2][0] == _m._31 && m[2][1] == _m._32 && m[2][2] == _m._33 && m[2][3] == _m._34
		);
};

INLINE bool zMAT3::operator != ( const zMAT3 & _m ) const
{
	return  (
		m[0][0] != _m._11 || m[0][1] != _m._12 || m[0][2] != _m._13 || m[0][3] != _m._14 ||
		m[1][0] != _m._21 || m[1][1] != _m._22 || m[1][2] != _m._23 || m[1][3] != _m._24 ||
		m[2][0] != _m._31 || m[2][1] != _m._32 || m[2][2] != _m._33 || m[2][3] != _m._34
		);
};

///////////////////////////////////////////////////////////////////////////////
//
// 4x4 MATRICE
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// -- constructors

INLINE zMAT4::zMAT4( const zMAT4 & _m )
{
	memcpy_s(m, sizeof(zMAT4), &_m, 1); // this is more performant
};

INLINE zMAT4::zMAT4(float f11, float f12, float f13, float f14,
	  float f21, float f22, float f23, float f24,
	  float f31, float f32, float f33, float f34,
	  float f41, float f42, float f43, float f44)
{
	m[0][0] = f11; m[0][1] = f12; m[0][2] = f13; m[0][3] = f14;
	m[1][0] = f21; m[1][1] = f22; m[1][2] = f23; m[1][3] = f24;
	m[2][0] = f31; m[2][1] = f32; m[2][2] = f33; m[2][3] = f34;
	m[3][0] = f41; m[3][1] = f42; m[3][2] = f43; m[3][3] = f44;
};

///////////////////////////////////////////////////////////////////////////////
//  -- assignment operators

INLINE zMAT4& zMAT4::operator *= ( const zMAT4 & _m )
{
	m[0][0] *= _m._11; m[0][1] *= _m._12; m[0][2] *= _m._13; m[0][3] *= _m._14;
	m[1][0] *= _m._21; m[1][1] *= _m._22; m[1][2] *= _m._23; m[1][3] *= _m._24;
	m[2][0] *= _m._31; m[2][1] *= _m._32; m[2][2] *= _m._33; m[2][3] *= _m._34;
	m[3][0] *= _m._41; m[3][1] *= _m._42; m[3][2] *= _m._43; m[3][3] *= _m._44;
	return *this;
};

INLINE zMAT4& zMAT4::operator /= ( const zMAT4 & _m )
{
	m[0][0] *= (1.0f / _m._11); m[0][1] *= (1.0f / _m._12); m[0][2] *= (1.0f / _m._13); m[0][3] *= (1.0f / _m._14);
	m[1][0] *= (1.0f / _m._21); m[1][1] *= (1.0f / _m._22); m[1][2] *= (1.0f / _m._23); m[1][3] *= (1.0f / _m._24);
	m[2][0] *= (1.0f / _m._31); m[2][1] *= (1.0f / _m._32); m[2][2] *= (1.0f / _m._33); m[2][3] *= (1.0f / _m._34);
	m[3][0] *= (1.0f / _m._41); m[3][1] *= (1.0f / _m._42); m[3][2] *= (1.0f / _m._43); m[3][3] *= (1.0f / _m._44);
	return *this;
};


INLINE zMAT4& zMAT4::operator += ( const zMAT4 & _m )
{
	m[0][0] += _m._11; m[0][1] += _m._12; m[0][2] += _m._13; m[0][3] += _m._14;
	m[1][0] += _m._21; m[1][1] += _m._22; m[1][2] += _m._23; m[1][3] += _m._24;
	m[2][0] += _m._31; m[2][1] += _m._32; m[2][2] += _m._33; m[2][3] += _m._34;
	m[3][0] += _m._41; m[3][1] += _m._42; m[3][2] += _m._43; m[3][3] += _m._44;
	return *this;
};

INLINE zMAT4& zMAT4::operator -= ( const zMAT4 & _m )
{
	m[0][0] -= _m._11; m[0][1] -= _m._12; m[0][2] -= _m._13; m[0][3] -= _m._14;
	m[1][0] -= _m._21; m[1][1] -= _m._22; m[1][2] -= _m._23; m[1][3] -= _m._24;
	m[2][0] -= _m._31; m[2][1] -= _m._32; m[2][2] -= _m._33; m[2][3] -= _m._34;
	m[3][0] -= _m._41; m[3][1] -= _m._42; m[3][2] -= _m._43; m[3][3] -= _m._44;
	return *this;
};

INLINE zMAT4& zMAT4::operator *= ( float f )
{
	m[0][0] *= f; m[0][1] *= f; m[0][2] *= f; m[0][3] *= f;
	m[1][0] *= f; m[1][1] *= f; m[1][2] *= f; m[1][3] *= f;
	m[2][0] *= f; m[2][1] *= f; m[2][2] *= f; m[2][3] *= f;
	m[3][0] *= f; m[3][1] *= f; m[3][2] *= f; m[3][3] *= f;
	return *this;
};

INLINE zMAT4& zMAT4::operator /= ( float f )
{
	float inv = 1.0f / f;
	m[0][0] /= inv; m[0][1] /= inv; m[0][2] /= inv; m[0][3] /= inv;
	m[1][0] /= inv; m[1][1] /= inv; m[1][2] /= inv; m[1][3] /= inv;
	m[2][0] /= inv; m[2][1] /= inv; m[2][2] /= inv; m[2][3] /= inv;
	m[3][0] /= inv; m[3][1] /= inv; m[3][2] /= inv; m[3][3] /= inv;
	return *this;
};

///////////////////////////////////////////////////////////////////////////////
// -- unary operators

INLINE zMAT4 zMAT4::operator + () const
{
	return *this;
};

INLINE zMAT4 zMAT4::operator - () const
{
	return zMAT4(
		-m[0][0], -m[0][1], -m[0][2],- m[0][3],
		-m[1][0], -m[1][1], -m[1][2], -m[1][3],
		-m[2][0], -m[2][1], -m[2][2], -m[2][3],
		-m[3][0], -m[3][1], -m[3][2], -m[0][3]
	);
};

///////////////////////////////////////////////////////////////////////////////
// -- binary operators

INLINE zMAT4 zMAT4::operator * ( const zMAT4 & _m )
{
	return zMAT4(
		m[0][0] * _m._11, m[0][1] * _m._12, m[0][2] * _m._13, m[0][3] * _m._14,
		m[1][0] * _m._21, m[1][1] * _m._22, m[1][2] * _m._23, m[1][3] * _m._24,
		m[2][0] * _m._31, m[2][1] * _m._32, m[2][2] * _m._33, m[2][3] * _m._34,
		m[3][0] * _m._41, m[3][1] * _m._42, m[3][2] * _m._43, m[3][3] * _m._44
		);
};

INLINE zMAT4 zMAT4::operator / ( const zMAT4 & _m )
{
	return zMAT4(
		m[0][0] * (1.0f /_m._11), m[0][1] * (1.0f /_m._12), m[0][2] * (1.0f /_m._13), m[0][3] * (1.0f /_m._14),
		m[1][0] * (1.0f /_m._21), m[1][1] * (1.0f /_m._22), m[1][2] * (1.0f /_m._23), m[1][3] * (1.0f /_m._24),
		m[2][0] * (1.0f /_m._31), m[2][1] * (1.0f /_m._32), m[2][2] * (1.0f /_m._33), m[2][3] * (1.0f /_m._34),
		m[3][0] * (1.0f /_m._41), m[3][1] * (1.0f /_m._42), m[3][2] * (1.0f /_m._43), m[3][3] * (1.0f /_m._44)
		);
};

INLINE zMAT4 zMAT4::operator + ( const zMAT4 & _m )
{
	return zMAT4(
		m[0][0] + _m._11, m[0][1] + _m._12, m[0][2] + _m._13, m[0][3] + _m._14,
		m[1][0] + _m._21, m[1][1] + _m._22, m[1][2] + _m._23, m[1][3] + _m._24,
		m[2][0] + _m._31, m[2][1] + _m._32, m[2][2] + _m._33, m[2][3] + _m._34,
		m[3][0] + _m._41, m[3][1] + _m._42, m[3][2] + _m._43, m[3][3] + _m._44
		);
};

INLINE zMAT4 zMAT4::operator - ( const zMAT4 & _m )
{
	return zMAT4(
		m[0][0] + _m._11, m[0][1] + _m._12, m[0][2] + _m._13, m[0][3] + _m._14,
		m[1][0] + _m._21, m[1][1] + _m._22, m[1][2] + _m._23, m[1][3] + _m._24,
		m[2][0] + _m._31, m[2][1] + _m._32, m[2][2] + _m._33, m[2][3] + _m._34,
		m[3][0] + _m._41, m[3][1] + _m._42, m[3][2] + _m._43, m[3][3] + _m._44
		);
};

INLINE zMAT4 zMAT4::operator * ( float f )
{
	return zMAT4(
		m[0][0] * f, m[0][1] * f, m[0][2] * f, m[0][3] * f,
		m[1][0] * f, m[1][1] * f, m[1][2] * f, m[1][3] * f,
		m[2][0] * f, m[2][1] * f, m[2][2] * f, m[2][3] * f,
		m[3][0] * f, m[3][1] * f, m[3][2] * f, m[3][3] * f
		);
};

INLINE zMAT4 zMAT4::operator / ( float f )
{
	float inv = 1.0f / f;
	return zMAT4(
		m[0][0] * inv, m[0][1] * inv, m[0][2] * inv, m[0][3] * inv,
		m[1][0] * inv, m[1][1] * inv, m[1][2] * inv, m[1][3] * inv,
		m[2][0] * inv, m[2][1] * inv, m[2][2] * inv, m[2][3] * inv,
		m[3][0] * inv, m[3][1] * inv, m[3][2] * inv, m[3][3] * inv
		);
};

///////////////////////////////////////////////////////////////////////////////
// -- logical operators

INLINE bool zMAT4::operator == ( const zMAT4 & _m ) const
{
	return  (
		m[0][0] == _m._11 && m[0][1] == _m._12 && m[0][2] == _m._13 && m[0][3] == _m._14 &&
		m[1][0] == _m._21 && m[1][1] == _m._22 && m[1][2] == _m._23 && m[1][3] == _m._24 &&
		m[2][0] == _m._31 && m[2][1] == _m._32 && m[2][2] == _m._33 && m[2][3] == _m._34 &&
		m[3][0] == _m._41 && m[3][1] == _m._42 && m[3][2] == _m._43 && m[3][3] == _m._44
		);
};

INLINE bool zMAT4::operator != ( const zMAT4 & _m ) const
{
	return  (
		m[0][0] != _m._11 || m[0][1] != _m._12 || m[0][2] != _m._13 || m[0][3] != _m._14 ||
		m[1][0] != _m._21 || m[1][1] != _m._22 || m[1][2] != _m._23 || m[1][3] != _m._24 ||
		m[2][0] != _m._31 || m[2][1] != _m._32 || m[2][2] != _m._33 || m[2][3] != _m._34 ||
		m[3][0] != _m._41 || m[3][1] != _m._42 || m[3][2] != _m._43 || m[3][3] != _m._44
		);
};

///////////////////////////////////////////////////////////////////////////////
//
// zCOLOR
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// -- constructors

INLINE zCOLOR::zCOLOR() 
{ 
	this->Clear(); 
};

INLINE zCOLOR::zCOLOR(zBYTE _r, zBYTE _g, zBYTE _b) 
{ 
	b = _b; 
	g = _g; 
	r = _r; 
	a = 255; 
}; 

INLINE zCOLOR::zCOLOR(zBYTE _r, zBYTE _g, zBYTE _b, zBYTE _a) 
{ 
	b = _b; 
	g = _g; 
	r = _r; 
	a = _a; 
};

INLINE zCOLOR::zCOLOR(zDWORD _color)
{
	color = _color;	
}; 

///////////////////////////////////////////////////////////////////////////////
// -- functions
INLINE void zCOLOR::Clear() 
{ 
	b = 0; 
	g = 0; 
	r = 0; 
	a = 255; 
};

INLINE void zCOLOR::SetRGBA(BYTE _r, BYTE _g, BYTE _b, BYTE _a) 
{ 
	r = _r; 
	g = _g; 
	b = _b; 
	a = _a; 
};

INLINE void zCOLOR::SetBGRA(BYTE _b, BYTE _g, BYTE _r, BYTE _a) 
{ 
	r = _r; 
	g = _g; 
	b = _b; 
	a = _a; 
};

INLINE void zCOLOR::SetR(BYTE _c) { r = _c; };
INLINE void zCOLOR::SetG(BYTE _c) { g = _c; };
INLINE void zCOLOR::SetB(BYTE _c) { b = _c; };
INLINE void zCOLOR::SetA(BYTE _c) { a = _c; };

INLINE BYTE zCOLOR::GetR(void) { return r; };
INLINE BYTE zCOLOR::GetG(void) { return g; };
INLINE BYTE zCOLOR::GetB(void) { return b; };
INLINE BYTE zCOLOR::GetA(void) { return a; };

#endif  //__ZTYPES_INL_INCLUDED__