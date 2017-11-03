/*
 * MIT License
 *
 * Copyright (c) 2017 Twitter
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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
