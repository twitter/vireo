//
//  mathutils.h
//  ImageTool
//
//  Created by Luke Alonso on 10/24/12.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "imagecore/imagecore.h"

inline float lerp(float a, float b, float v)
{
	return a + (b - a) * v;
}

inline float step(float a, float b, float v)
{
	return (v - a) / (b - a);
}

inline int max(int x, int y)
{
	return x > y ? x : y;
}

inline int min(int x, int y)
{
	return x < y ? x : y;
}

inline unsigned int max(unsigned int x, unsigned int y)
{
	return x > y ? x : y;
}

inline unsigned int min(unsigned int x, unsigned int y)
{
	return x < y ? x : y;
}

inline uint64_t max(uint64_t x, uint64_t y)
{
	return x > y ? x : y;
}

inline uint64_t min(uint64_t x, uint64_t y)
{
	return x < y ? x : y;
}

inline int clamp(int minv, int maxv, int a)
{
	return max(minv, min(a, maxv));
}

inline float saturate(float f)
{
	return fmaxf(0, fminf(1, f));
}

inline float clampf(float v, float min, float max)
{
	return fmaxf(min, fminf(v, max));
}

inline float npowf(float x, float e)
{
	return x >= 0.0f ? powf(x, e) : -powf(-x, e);
}

inline unsigned char luminance(unsigned char r, unsigned char b, unsigned char g)
{
	return (max(r, max(g, b)));
}

inline unsigned int align(unsigned int v, unsigned int a)
{
	if(a > 0) {
		unsigned int rem = v % a;
		if( rem != 0 ) {
			return v + (a - rem);
		}
	}
	return v;
}

inline void swap(unsigned int& a, unsigned int& b)
{
	unsigned int temp = a;
	a = b;
	b = temp;
}

inline unsigned int div2_round(unsigned int a)
{
	return (a + 1) / 2;
}
