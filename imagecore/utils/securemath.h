//
//  securemath.h
//  ImageTool
//
//  Created by Luke Alonso on 12/17/12.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress"
#pragma GCC diagnostic ignored "-Wsign-compare"

#include <stdlib.h>
#include "imagecore/imagecore.h"
#include "safe_iop/safe_iop.h"

#define ASSERT_NO_INTEGER_OVERFLOW(x) { if( !x ) { ImageCoreAssert(IMAGECORE_INTEGER_OVERFLOW, #x, __FILE__, __LINE__); exit(IMAGECORE_INTEGER_OVERFLOW); } }

inline uint32_t SafeUMul(uint32_t a, uint32_t b)
{
	uint32_t r;
	ASSERT_NO_INTEGER_OVERFLOW(safe_umul(&r, a, b));
	return r;
}

inline uint64_t SafeUMul(uint64_t a, uint64_t b)
{
	uint64_t r;
	ASSERT_NO_INTEGER_OVERFLOW(safe_umul(&r, a, b));
	return r;
}

inline uint32_t SafeUMul3(uint32_t a, uint32_t b, uint32_t c)
{
	uint32_t r;
	ASSERT_NO_INTEGER_OVERFLOW(safe_umul(&r, a, b));
	ASSERT_NO_INTEGER_OVERFLOW(safe_umul(&r, r, c));
	return r;
}

inline uint64_t SafeUMul3(uint64_t a, uint64_t b, uint64_t c)
{
	uint64_t r;
	ASSERT_NO_INTEGER_OVERFLOW(safe_umul(&r, a, b));
	ASSERT_NO_INTEGER_OVERFLOW(safe_umul(&r, r, c));
	return r;
}

inline uint32_t SafeUAdd(uint32_t a, uint32_t b)
{
	uint32_t r;
	ASSERT_NO_INTEGER_OVERFLOW(safe_uadd(&r, a, b));
	return r;
}

inline uint64_t SafeUAdd(uint64_t a, uint64_t b)
{
	uint64_t r;
	ASSERT_NO_INTEGER_OVERFLOW(safe_uadd(&r, a, b));
	return r;
}

inline uint64_t SafeUAdd(uint64_t a, int64_t b)
{
	uint64_t r;
	ASSERT_NO_INTEGER_OVERFLOW(safe_uadd(&r, a, b));
	return r;
}

inline uint32_t SafeUSub(uint32_t a, uint32_t b)
{
	uint32_t r;
	ASSERT_NO_INTEGER_OVERFLOW(safe_usub(&r, a, b));
	return r;
}

inline uint64_t SafeUSub(uint64_t a, uint64_t b)
{
	uint64_t r;
	ASSERT_NO_INTEGER_OVERFLOW(safe_usub(&r, a, b));
	return r;
}

inline uint64_t SafeUSub(uint64_t a, int64_t b)
{
	uint64_t r;
	ASSERT_NO_INTEGER_OVERFLOW(safe_usub(&r, a, b));
	return r;
}

#pragma GCC diagnostic pop
