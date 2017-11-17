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

#include <arm_neon.h>

#define vUInt8 uint8x16_t
#define vUInt8x8 uint8x8_t
#define vUInt16 uint16x8_t
#define vUInt32 uint32x4_t
#define vSInt8 int8x16_t
#define vSInt16 int16x8_t
#define vSInt32 int32x4_t
#define vSInt64 int64x2_t
#define vMask128 uint64x1x2_t

#define V64_MASK_LO(e7, e6, e5, e4, e3, e2, e1, e0) (uint64_t)(e0) \
| ((uint64_t)(e1) << 8) \
| ((uint64_t)(e2) << 16) \
| ((uint64_t)(e3) << 24) \
| ((uint64_t)(e4) << 32) \
| ((uint64_t)(e5) << 40) \
| ((uint64_t)(e6) << 48) \
| ((uint64_t)(e7) << 56)

#define V64_MASK_HI V64_MASK_LO

// note, this doesnt do a full transpose, the 2 middle 32 bit elements still need to be swapped
#define vec_transpose_int8(r0, r1, r2, r3, c0, c1, c2, c3) \
{ \
vSInt8 u0, u1, u2, u3; \
v128_unpack_int8(u0, u1, r0, r1); \
v128_unpack_int8(u2, u3, r2, r3); \
vSInt16 t0, t1, t2, t3; \
v128_unpack_int16(t0, t1, u0, u2); \
v128_unpack_int16(t2, t3, u1, u3); \
vSInt32 s0, s1, s2, s3; \
v128_unpack_int32(s0, s1, t0, t2); \
v128_unpack_int32(s2, s3, t1, t3); \
v128_unpack_int64(c0, c1, s0, s2); \
v128_unpack_int64(c2, c3, s1, s3); \
}

#define vec_transpose_int16(r0, r1, r2, r3, c0, c1, c2, c3) \
{ \
vSInt16 t0, t1, t2, t3; \
v128_unpack_int16(t0, t1, r0, r1); \
v128_unpack_int16(t2, t3, r2, r3); \
vSInt32 s0, s1, s2, s3; \
v128_unpack_int32(s0, s1, t0, t2); \
v128_unpack_int32(s2, s3, t1, t3); \
v128_unpack_int64(c0, c1, s0, s2); \
v128_unpack_int64(c2, c3, s1, s3); \
}

// set
inline vSInt32 v128_setzero()
{
	return vdupq_n_s32(0);
}

inline vUInt8x8 v64_setzero()
{
	return vdup_n_s32(0);
}

inline vMask128 v128_set_mask(uint64_t high, uint64_t low)
{
	vMask128 res;
	res.val[0] = vset_lane_u64(low, res.val[0], 0);
	res.val[1] = vset_lane_u64(high, res.val[1], 0);
	return res;
}

inline vUInt8x8 v64_set_int8_packed(char e7, char e6, char e5, char e4, char e3, char e2, char e1, char e0)
{
	vUInt8x8 res = v64_setzero();
	res = vset_lane_u8(e0, res, 0);
	res = vset_lane_u8(e1, res, 1);
	res = vset_lane_u8(e2, res, 2);
	res = vset_lane_u8(e3, res, 3);
	res = vset_lane_u8(e4, res, 4);
	res = vset_lane_u8(e5, res, 5);
	res = vset_lane_u8(e6, res, 6);
	res = vset_lane_u8(e7, res, 7);
	return res;
}

inline vSInt16 v128_set_int16(uint16_t a)
{
	return vdupq_n_s16(a);
}

inline vUInt8 v128_set_int8_packed(char e15, char e14, char e13, char e12, char e11, char e10, char e9, char e8, char e7, char e6, char e5, char e4, char e3, char e2, char e1, char e0)
{
	vUInt8 res = v128_setzero();
	res = vsetq_lane_u8(e0, res, 0);
	res = vsetq_lane_u8(e1, res, 1);
	res = vsetq_lane_u8(e2, res, 2);
	res = vsetq_lane_u8(e3, res, 3);
	res = vsetq_lane_u8(e4, res, 4);
	res = vsetq_lane_u8(e5, res, 5);
	res = vsetq_lane_u8(e6, res, 6);
	res = vsetq_lane_u8(e7, res, 7);
	res = vsetq_lane_u8(e8, res, 8);
	res = vsetq_lane_u8(e9, res, 9);
	res = vsetq_lane_u8(e10, res, 10);
	res = vsetq_lane_u8(e11, res, 11);
	res = vsetq_lane_u8(e12, res, 12);
	res = vsetq_lane_u8(e13, res, 13);
	res = vsetq_lane_u8(e14, res, 14);
	res = vsetq_lane_u8(e15, res, 15);
	return res;
}

// load
inline vSInt32 v128_load_unaligned(const vSInt32* mem_addr)
{
	return vld1q_u8((uint8_t*)mem_addr);
}

inline vUInt8x8 v64_load(vSInt32, const vSInt32* mem_addr)
{
	return vld1_u8((uint8_t const *)mem_addr);
}

// store
inline void v64_store(vSInt32* mem_addr, vUInt8x8 a)
{
	vst1_u8((uint8_t*)mem_addr, a);
}

// conversions
inline int32_t v128_convert_to_int32(vUInt8 a)
{
	return vgetq_lane_u32(a, 0);
}

template<int lane>
inline int32_t v128_convert_lane_to_int32(vUInt8 a)
{
	return vgetq_lane_u32(a, lane);
}

inline int64_t v128_convert_to_int64(vUInt8x8 a)
{
	return vget_lane_s64((int64x1_t)a, 0);
}

inline int32_t v64_convert_to_int32(vUInt8x8 a)
{
	return vget_lane_s32((int32x2_t)a, 0);
}

// math
inline vUInt16 v128_add_int16(vUInt16 a, vUInt16 b)
{
	return vaddq_u16(a, b);
}

inline vSInt16 v128_mul_int16(vSInt16 a, vSInt16 b)
{
	return vmulq_s16(a, b);
}

inline vUInt8x8 v64_add_int16(vUInt8x8 a, vUInt8x8 b)
{
	return vadd_u16(a, b);
}

// unpack
inline void v128_unpack_int8(vSInt8& a, vSInt8& b, vUInt8 c, vUInt8 d)
{
	int8x16x2_t unpacked = vzipq_s8(c, d);
	a = unpacked.val[0];
	b = unpacked.val[1];
}


inline void v128_unpack_int16(vSInt16& a, vSInt16& b, vUInt8 c, vUInt8 d)
{
	int16x8x2_t unpacked = vzipq_s16(c, d);
	a = unpacked.val[0];
	b = unpacked.val[1];
}

inline void v128_unpack_int32(vSInt32& a, vSInt32& b, vSInt32 c, vSInt32 d)
{
	int32x4x2_t unpacked = vzipq_s32(c, d);
	a = unpacked.val[0];
	b = unpacked.val[1];
}

inline void v128_unpack_int64(vSInt64& a, vSInt64& b, vSInt64 c, vSInt64 d)
{
	a = vcombine_s64(vget_low_s64(c), vget_low_s64(d));
	b = vcombine_s64(vget_high_s64(c), vget_high_s64(d));
}

// pack
inline vUInt8x8 v128_pack_unsigned_saturate_int16(vUInt16 a, vUInt16, vUInt16)
{
	return vmovn_u16(a);
}

inline vUInt8x8 v64_pack_unsigned_saturate_int16(vUInt8x8 a, vUInt16, vUInt8x8 mask)
{
	return vtbl1_u8(a, mask);
}

// shift
template<int imm>
inline vUInt16 v128_shift_right_unsigned_int16(vUInt16 a)
{
	return vshrq_n_u16(a, imm);
}

template<int imm>
inline vUInt8x8 v64_shift_right_unsigned_int16(vUInt8x8 a)
{
	return vshr_n_u16(a, imm);
}

// shuffles
// not the same functionality as SSE shuffle_epi8, can only index 64 bit registers, so all indices are 0..7
inline vUInt8 v128_shuffle_int8(vUInt8 a, vMask128 b)
{
	vUInt8 res;
	res = vcombine_u8(vtbl1_u8(vget_low_u8(a), (uint8x8_t)b.val[0]), vtbl1_u8(vget_high_u8(a), (uint8x8_t)b.val[1]));
	return res;
}

inline vUInt8x8 v64_shuffle_int8(vUInt8x8 a, vUInt8x8 b)
{
	return vtbl1_u8(a, b);
}

// special case for compatibility with sse
inline void v128_swizzleAndUnpack(vUInt16& a, vUInt16& b, vUInt8 c, vSInt32)
{
	uint32x2x2_t trans = vtrn_u32(vget_low_u8(c), vget_high_u8(c));
	a = vmovl_u8(trans.val[0]);
	b = vmovl_u8(trans.val[1]);
}

inline vUInt8x8 v128_merge(vSInt16 a, vSInt16 b)
{
	vUInt8x8 high = vget_high_u8(a);
	vUInt8x8 low = vget_low_u8(a);
	high = vrev16_u8(high);
	return vadd_u8(high, low);
}
















