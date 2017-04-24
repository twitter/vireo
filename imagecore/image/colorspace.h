//
//  colorspace.h
//  ImageCore
//
//  Created by Luke Alonso on 7/18/14.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "math.h"
#include <stdio.h>

struct RGBA
{
	unsigned char r, g, b, a;

	RGBA()
	{
	}

	RGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
	:	r(r)
	,	g(g)
	,	b(b)
	,	a(a)
	{
	}

	RGBA(unsigned char r, unsigned char g, unsigned char b)
	:	RGBA(r, g, b, 255)
	{
	}

	RGBA(const RGBA& v)
	:	r(v.r)
	,	g(v.g)
	,	b(v.b)
	,	a(v.a)
	{}

	RGBA(const RGBA&& v)
	:	r(v.r)
	,	g(v.g)
	,	b(v.b)
	,	a(v.a)
	{}

	RGBA& operator=(const RGBA& v)
    {
		r = v.r;
		g = v.g;
		b = v.b;
		a = v.a;
		return *this;
	}

	bool operator==(const RGBA& v)
	{
		return (r == v.r && g == v.g && b == v.b && a == v.a);
	}
};

struct float3
{
	float x, y, z;

	float3()
	{}

	float3(float s)
	:	x(s)
	,	y(s)
	,	z(s)
	{}

	float3(float vx, float vy, float vz)
	:	x(vx)
	,	y(vy)
	,	z(vz)
	{}

	float3(const float3& v)
	:	x(v.x)
	,	y(v.y)
	,	z(v.z)
	{}

	float3(const float3&& v)
	:	x(v.x)
	,	y(v.y)
	,	z(v.z)
	{}

	float3& operator=(const float3& v)
    {
		x = v.x;
		y = v.y;
		z = v.z;
		return *this;
	}

	float3 operator+(const float3& b) const
	{
		return float3(x + b.x, y + b.y, z + b.z);
	}

	float3 operator-(const float3& b) const
	{
		return float3(x - b.x, y - b.y, z - b.z);
	}

	float3 operator*(const float3& b) const
	{
		return float3(x * b.x, y * b.y, z * b.z);
	}

	float3 operator/(const float3& b) const
	{
		return float3(x / b.x, y / b.y, z / b.z);
	}

	float dot(const float3& b) const
	{
		return x * b.x + y * b.y + z * b.z;
	}

	float3 pow(float e) const
	{
		return float3(powf(x, e), powf(y, e), powf(z, e));
	}
};

class ColorSpace
{
public:
	// sRGB
	static float3 srgbToLinear(const float3& s);
	static float3 linearToSrgb(const float3& l);

	// HSV
	static float3 srgbToHsv(const float3& s);
	static float3 hsvToSrgb(const float3& h);

	// LAB
	static float3 srgbToLab(const float3& s);
	static float3 labToSrgb(const float3& l);

	// Float
	static float3 byteToFloat(const RGBA& c);
	static RGBA floatToByte(const float3& v);
};
