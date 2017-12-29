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

#ifdef __APPLE__
#include <Accelerate/Accelerate.h>
#else
#define vUInt8 __m128i
#define vUInt8x8 __m128i
#define vUInt16 __m128i
#define vUInt32 __m128i
#define vUInt64 __m128i
#define vSInt8 __m128i
#define vSInt16 __m128i
#define vSInt32 __m128i
#define vSInt64 __m128i
#define vFloat __m128
#endif

#define v128 __m128
#define v128i __m128i

// for neon compatibility
#define vUInt8x8 __m128i
#define vMask128 __m128i
#define v64_load v64_load_unaligned
#define v64_shuffle_int8 v128_shuffle_int8
#define v64_add_int16 v128_add_int16
#define v64_shift_right_unsigned_int16 v128_shift_right_unsigned_int16
#define v64_pack_unsigned_saturate_int16 v128_pack_unsigned_saturate_int16
#define v64_convert_to_int32 v128_convert_to_int32

#define V64_MASK_LO(e7, e6, e5, e4, e3, e2, e1, e0) (uint64_t)(e0) \
| ((uint64_t)(e1) << 8) \
| ((uint64_t)(e2) << 16) \
| ((uint64_t)(e3) << 24) \
| ((uint64_t)(e4) << 32) \
| ((uint64_t)(e5) << 40) \
| ((uint64_t)(e6) << 48) \
| ((uint64_t)(e7) << 56)

// for neon 64 bit compatibility, assumption that high mask only indexes the high 64 bits in the source 128 bit register
#define V64_MASK_HI(e7, e6, e5, e4, e3, e2, e1, e0) (uint64_t)(e0 == 0x80 ? 0x80 : e0 + 8) \
| ((uint64_t)(e1 == 0x80 ? 0x80 : e1 + 8) << 8) \
| ((uint64_t)(e2 == 0x80 ? 0x80 : e2 + 8) << 16) \
| ((uint64_t)(e3 == 0x80 ? 0x80 : e3 + 8) << 24) \
| ((uint64_t)(e4 == 0x80 ? 0x80 : e4 + 8) << 32) \
| ((uint64_t)(e5 == 0x80 ? 0x80 : e5 + 8) << 40) \
| ((uint64_t)(e6 == 0x80 ? 0x80 : e6 + 8) << 48) \
| ((uint64_t)(e7 == 0x80 ? 0x80 : e7 + 8) << 56)


#include <immintrin.h>
#include <emmintrin.h>
#include <cpuid.h>


// note, this doesn't do a full transpose, the 2 middle 32 bit elements still need to be swapped
#define vec_transpose_int8(r0, r1, r2, r3, c0, c1, c2, c3) \
{ \
vSInt8 u0 = v128_unpacklo_int8(r0, r1); \
vSInt8 u1 = v128_unpacklo_int8(r2, r3); \
vSInt8 u2 = v128_unpackhi_int8(r0, r1); \
vSInt8 u3 = v128_unpackhi_int8(r2, r3); \
vSInt8 t0 = v128_unpacklo_int16(u0, u1); \
vSInt8 t1 = v128_unpacklo_int16(u2, u3); \
vSInt8 t2 = v128_unpackhi_int16(u0, u1); \
vSInt8 t3 = v128_unpackhi_int16(u2, u3); \
vSInt8 s0 = v128_unpacklo_int32(t0, t1); \
vSInt8 s1 = v128_unpacklo_int32(t2, t3); \
vSInt8 s2 = v128_unpackhi_int32(t0, t1); \
vSInt8 s3 = v128_unpackhi_int32(t2, t3); \
c0 = v128_unpacklo_int64(s0, s1); \
c1 = v128_unpackhi_int64(s0, s1); \
c2 = v128_unpacklo_int64(s2, s3); \
c3 = v128_unpackhi_int64(s2, s3); \
}

#define vec_transpose_int16(r0, r1, r2, r3, c0, c1, c2, c3) \
{ \
vSInt16 t0 = v128_unpacklo_int16(r0, r1); \
vSInt16 t1 = v128_unpacklo_int16(r2, r3); \
vSInt16 t2 = v128_unpackhi_int16(r0, r1); \
vSInt16 t3 = v128_unpackhi_int16(r2, r3); \
vSInt16 s0 = v128_unpacklo_int32(t0, t1); \
vSInt16 s1 = v128_unpacklo_int32(t2, t3); \
vSInt16 s2 = v128_unpackhi_int32(t0, t1); \
vSInt16 s3 = v128_unpackhi_int32(t2, t3); \
c0 = v128_unpacklo_int64(s0, s1); \
c1 = v128_unpackhi_int64(s0, s1); \
c2 = v128_unpacklo_int64(s2, s3); \
c3 = v128_unpackhi_int64(s2, s3); \
}

// set
inline v128i v128_setzero()
{
	return _mm_setzero_si128();
}

inline v128i v128_set_int32(int32_t a)
{
	return _mm_set1_epi32(a);
}

inline v128i v128_set_int16(int16_t a)
{
	return _mm_set1_epi16(a);
}

inline v128i v128_set_int8_packed(int8_t e15, int8_t e14, int8_t e13, int8_t e12, int8_t e11, int8_t e10, int8_t e9, int8_t e8, int8_t e7, int8_t e6, int8_t e5, int8_t e4, int8_t e3, int8_t e2, int8_t e1, int8_t e0)
{
	return _mm_set_epi8(e15, e14, e13, e12, e11, e10, e9, e8, e7, e6, e5, e4, e3, e2, e1, e0);
}

inline vMask128 v128_set_mask(uint64_t high, uint64_t low)
{
	vMask128 res;
	res = _mm_set_epi64x((long long)high, (long long)low);
	return res;
}

inline v128i v64_set_int8_packed(int8_t e7, int8_t e6, int8_t e5, int8_t e4, int8_t e3, int8_t e2, int8_t e1, int8_t e0)
{
	return _mm_set_epi8(ZMASK, ZMASK, ZMASK, ZMASK, ZMASK, ZMASK, ZMASK, ZMASK, e7, e6, e5, e4, e3, e2, e1, e0);
}

// load
inline v128i v64_load_unaligned(vSInt32 a, const vSInt32* mem_addr)
{
	return (v128i) _mm_loadl_pi((v128)a, (__m64 const*) mem_addr);
}

inline v128i v128_load_unaligned(const vSInt32* mem_addr)
{
	return _mm_lddqu_si128((const __m128i*)mem_addr);
}

// store
inline void v64_store(vSInt32* mem_addr, v128i a)
{
	_mm_storel_epi64((__m128i*)mem_addr, a);
}

inline void v128_store_unaligned(vSInt32* mem_addr, v128i a)
{
	_mm_storeu_si128 ((__m128i*) mem_addr, a);
}

// conversions
inline int32_t v128_convert_to_int32(v128i a)
{
	return _mm_cvtsi128_si32(a);
}

template<int lane>
inline int32_t v128_convert_lane_to_int32(v128i a)
{
	v128i b;
	b = _mm_srli_si128(a, lane * 4);
	return _mm_cvtsi128_si32(b);
}

inline int64_t v128_convert_to_int64(v128i a)
{
#ifdef __x86_64__
	return _mm_cvtsi128_si64(a);
#else
	int64_t val[2];
	_mm_store_si128((v128i*)val, a);
	return val[0];
#endif
}

template<int imm8>
int v128_extract_int32(v128i a)
{
	return _mm_extract_epi32(a, imm8);
}

template<int imm8>
int v128_extract_int8(v128i a)
{
	return _mm_extract_epi8(a, imm8);
}

// math
inline v128i v128_add_int16(v128i a, v128i b)
{
	return _mm_add_epi16(a, b);
}

inline v128i v128_add_int32(v128i a, v128i b)
{
	return _mm_add_epi32(a, b);
}

inline v128i v128_mul_int16(v128i a, v128i b)
{
	return _mm_mullo_epi16(a, b);
}

inline v128i v128_mul_int32(v128i a, v128i b)
{
	return _mm_mullo_epi32(a, b);
}

// unpack
inline v128i v128_unpacklo_int8(v128i a, v128i b)
{
	return _mm_unpacklo_epi8(a, b);
}

inline v128i v128_unpackhi_int8(v128i a, v128i b)
{
	return _mm_unpackhi_epi8(a, b);
}

inline v128i v128_unpacklo_int16(v128i a, v128i b)
{
	return _mm_unpacklo_epi16(a, b);
}

inline v128i v128_unpackhi_int16(v128i a, v128i b)
{
	return _mm_unpackhi_epi16(a, b);
}

inline v128i v128_unpacklo_int32(v128i a, v128i b)
{
	return _mm_unpacklo_epi32(a, b);
}

inline v128i v128_unpackhi_int32(v128i a, v128i b)
{
	return _mm_unpackhi_epi32(a, b);
}

inline v128i v128_unpacklo_int64(v128i a, v128i b)
{
	return _mm_unpacklo_epi64(a, b);
}

inline v128i v128_unpackhi_int64(v128i a, v128i b)
{
	return _mm_unpackhi_epi64(a, b);
}

// pack
inline v128i v128_pack_unsigned_saturate_int16(v128i a, v128i b, v128i)
{
	return _mm_packus_epi16(a, b);
}

inline v128i v128_pack_unsigned_saturate_int32(v128i a, v128i b)
{
	return _mm_packus_epi32(a, b);
}

// shift
template<int imm>
inline v128i v128_shift_right_unsigned_int16(v128i a)
{
	return _mm_srli_epi16(a, imm);
}

template<int imm8>
inline v128i v128_shift_right_unsigned_vec128(v128i a)
{
	return _mm_srli_si128(a, imm8);
}

template<int imm>
inline v128i v128_shift_right_signed_int32(v128i a)
{
	return _mm_srai_epi32(a, imm);
}

// shuffles
template<int imm>
inline v128i v128_shuffle_int32(v128i a)
{
	return _mm_shuffle_epi32(a, imm);
}

inline v128i v128_shuffle_int8(v128i a, v128i b)
{
	return _mm_shuffle_epi8(a, b);
}

inline v128i v128_merge(v128i a, v128i b)
{
	return _mm_shuffle_epi8(a, b);
}

// special cases for cross platform compatibility
inline void v128_swizzleAndUnpack(vUInt16& a, vUInt16& b, vUInt8 c, vSInt32 zero)
{
	__m128i swizzled = v128_shuffle_int32<V128_SHUFFLE(3, 1, 2, 0)>(c);
	a = v128_unpacklo_int8(swizzled, zero);
	b = v128_unpackhi_int8(swizzled, zero);
}

inline void v128_unpack_int8(vSInt8& a, vSInt8& b, vUInt8 c, vUInt8 d)
{
	a = v128_unpacklo_int8(c, d);
	b = v128_unpackhi_int8(c, d);
}



