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
