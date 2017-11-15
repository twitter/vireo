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

#if  __SSE4_1__ || __ARM_NEON__

#include "filters.h"
#include "imagecore/imagecore.h"
#include "imagecore/utils/securemath.h"
#include "imagecore/utils/mathutils.h"
#include "platform_support.h"

#include "intrinsics.h"

namespace imagecore {

template<>
void Filters<ComponentSIMD<4>>::reduceHalf(const uint8_t* inputBuffer, uint8_t* outputBuffer, unsigned int width, unsigned int height, unsigned int pitch, unsigned int outputPitch, unsigned int outputCapacity)
{
	if(FiltersConfig::m_ScalarMode) {
		Filters<ComponentScalar<4>>::reduceHalf(inputBuffer, outputBuffer, width, height, pitch, outputPitch, outputCapacity);
		return;
	}
#if IMAGECORE_DETECT_SSE
	if( !checkForCPUSupport(kCPUFeature_SSE4_1)) {
		Filters<ComponentScalar<4>>::reduceHalf(inputBuffer, outputBuffer, width, height, pitch, outputPitch, outputCapacity);
		return;
	}
#endif
	unsigned int outputWidth = width >> 1;
	unsigned int outputHeight = height >> 1;
	SECURE_ASSERT(SafeUMul(outputWidth, 4U) <= outputPitch);
	SECURE_ASSERT(SafeUMul(outputHeight, outputPitch) <= outputCapacity);
	unsigned int inputPitch = pitch;
	uint8_t* outputSample = outputBuffer;
	const uint8_t* inputSample = inputBuffer;
	unsigned int rowLength = SafeUMul(outputWidth, 4U);
	vSInt32 zero = v128_setzero();
	uint8_t* imageEnd = outputSample + (outputPitch * outputHeight);
	do {
		const uint8_t* rowInputSample = inputSample;
		uint8_t* rowOutputSample = outputSample;
		uint8_t* rowEnd = rowOutputSample + rowLength;
		do {
			vUInt16 top_16_a;
			vUInt16 top_16_b;
			v128_swizzleAndUnpack(top_16_a, top_16_b, v128_load_unaligned((const vSInt32*)rowInputSample), zero);
			vUInt16 bottom_16_a;
			vUInt16 bottom_16_b;
			v128_swizzleAndUnpack(bottom_16_a, bottom_16_b, v128_load_unaligned((const vSInt32*)(rowInputSample + inputPitch)), zero);
			vUInt16 final_16 = v128_add_int16(v128_add_int16(top_16_a, bottom_16_a), v128_add_int16(top_16_b, bottom_16_b));
			final_16 = v128_shift_right_unsigned_int16<2>(final_16);
			vUInt8x8 packed_8 = v128_pack_unsigned_saturate_int16(final_16, zero, zero);
			v64_store((vSInt32*)rowOutputSample, packed_8);
			rowInputSample += 16;
			rowOutputSample += 8;
		} while (rowOutputSample < rowEnd);
		outputSample += outputPitch;
		inputSample += inputPitch * 2;
	} while (outputSample < imageEnd);
}

static void reduceHalfx8_inner(const uint8_t* inputBuffer, uint8_t* outputBuffer, unsigned int width, unsigned int height, unsigned int pitch, unsigned int outputPitch, unsigned int outputCapacity, uint32_t rowLength, vUInt8x8 unpackMask0, vUInt8x8 unpackMask1)
{
	vUInt8x8 packMask = v64_set_int8_packed(ZMASK, ZMASK, ZMASK, ZMASK, 7, 5, 3, 1);
	unsigned int outputWidth = width >> 1;
	unsigned int outputHeight = height >> 1;
	SECURE_ASSERT(outputWidth <= outputPitch);
	SECURE_ASSERT(SafeUMul(outputHeight, outputPitch) <= outputCapacity);
	unsigned int inputPitch = pitch;
	uint8_t* outputSample = outputBuffer;
	const uint8_t* inputSample = inputBuffer;
	vSInt32 zero = v128_setzero();
	uint8_t* imageEnd = outputSample + (outputPitch * outputHeight);
	do {
		const uint8_t* rowInputSample = inputSample;
		uint8_t* rowOutputSample = outputSample;
		const uint8_t* rowEnd = inputSample + rowLength;
		do {
			vUInt8x8 row_a;
			vUInt8x8 row_b;
			vUInt8x8 row_c;
			vUInt8x8 row_d;
			vUInt8x8 topRow = v64_load(zero, (const vSInt32*)rowInputSample);
			row_a = v64_shuffle_int8(topRow, unpackMask0);
			row_b = v64_shuffle_int8(topRow, unpackMask1);
			vUInt8x8 botRow = v64_load(zero, (const vSInt32*)(rowInputSample + inputPitch));
			row_c = v64_shuffle_int8(botRow, unpackMask0);
			row_d = v64_shuffle_int8(botRow, unpackMask1);
			vUInt8x8 final_16 = v64_add_int16(v64_add_int16(row_a, row_b), v64_add_int16(row_c, row_d));
			final_16 = v64_shift_right_unsigned_int16<2>(final_16);
			vUInt8x8 packed_8 = v64_pack_unsigned_saturate_int16(final_16, zero, packMask);
			*(uint32_t*)rowOutputSample = v64_convert_to_int32(packed_8);
			rowInputSample += 8;
			rowOutputSample += 4;
		} while (rowInputSample < rowEnd);
		outputSample += outputPitch;
		inputSample += inputPitch * 2;
	} while (outputSample < imageEnd);
}

static void reduceHalfx1x8(const uint8_t* inputBuffer, uint8_t* outputBuffer, unsigned int width, unsigned int height, unsigned int pitch, unsigned int outputPitch, unsigned int outputCapacity)
{
	unsigned int rowLength = width & (~0x7); // align on 8 byte boundary
	if(rowLength > 0) {
		vUInt8x8 unpackMask0 = v64_set_int8_packed(ZMASK, 6, ZMASK, 4, ZMASK, 2, ZMASK, 0);
		vUInt8x8 unpackMask1 = v64_set_int8_packed(ZMASK, 7, ZMASK, 5, ZMASK, 3, ZMASK, 1);
		reduceHalfx8_inner(inputBuffer, outputBuffer, width, height, pitch, outputPitch, outputCapacity, rowLength, unpackMask0, unpackMask1);
	}
	// for width of 4 pixels or less fallback to c++ implementation
	Filters<ComponentScalar<1>>::reduceHalf(&inputBuffer[rowLength], &outputBuffer[rowLength / 2], width - rowLength, height, pitch, outputPitch, outputCapacity);
}

static void reduceHalfx2x8(const uint8_t* inputBuffer, uint8_t* outputBuffer, unsigned int width, unsigned int height, unsigned int pitch, unsigned int outputPitch, unsigned int outputCapacity)
{
	unsigned int rowLength = (width * 2) & (~0x7); // align on 8 byte boundary
	if(rowLength > 0) {
		vUInt8x8 unpackMask0 = v64_set_int8_packed(ZMASK, 5, ZMASK, 4, ZMASK, 1, ZMASK, 0);
		vUInt8x8 unpackMask1 = v64_set_int8_packed(ZMASK, 7, ZMASK, 6, ZMASK, 3, ZMASK, 2);
		reduceHalfx8_inner(inputBuffer, outputBuffer, width, height, pitch, outputPitch, outputCapacity, rowLength, unpackMask0, unpackMask1);
	}
	// for width of 4 pixels or less fallback to c++ implementation
	Filters<ComponentScalar<2>>::reduceHalf(&inputBuffer[rowLength], &outputBuffer[rowLength / 2], width - rowLength / 2, height, pitch, outputPitch, outputCapacity);
}

static void reduceHalfx16_inner(const uint8_t* inputBuffer, uint8_t* outputBuffer, unsigned int width, unsigned int height, unsigned int pitch, unsigned int outputPitch, unsigned int outputCapacity, uint32_t rowLength, vMask128 unpackMask0, vMask128 unpackMask1)
{
	unsigned int outputWidth = width >> 1;
	unsigned int outputHeight = height >> 1;
	SECURE_ASSERT(outputWidth <= outputPitch);
	SECURE_ASSERT(SafeUMul(outputHeight, outputPitch) <= outputCapacity);
	unsigned int inputPitch = pitch;
	uint8_t* outputSample = outputBuffer;
	const uint8_t* inputSample = inputBuffer;
	vSInt32 zero = v128_setzero();
	uint8_t* imageEnd = outputSample + (outputPitch * outputHeight);
	do {
		const uint8_t* rowInputSample = inputSample;
		uint8_t* rowOutputSample = outputSample;
		const uint8_t* rowEnd = inputSample + rowLength;
		do {
			vUInt16 row_a;
			vUInt16 row_b;
			vUInt16 row_c;
			vUInt16 row_d;
			vUInt8 topRow = v128_load_unaligned((const vSInt32*)rowInputSample);
			row_a = v128_shuffle_int8(topRow, unpackMask0);
			row_b = v128_shuffle_int8(topRow, unpackMask1);
			vUInt8 botRow = v128_load_unaligned((const vSInt32*)(rowInputSample + inputPitch));
			row_c = v128_shuffle_int8(botRow, unpackMask0);
			row_d = v128_shuffle_int8(botRow, unpackMask1);
			vUInt16 final_16 = v128_add_int16(v128_add_int16(row_a, row_b), v128_add_int16(row_c, row_d));
			final_16 = v128_shift_right_unsigned_int16<2>(final_16);
			vUInt8x8 packed_8 = v128_pack_unsigned_saturate_int16(final_16, zero, zero);
			v64_store((vSInt32*)rowOutputSample, packed_8);
			rowInputSample += 16;
			rowOutputSample += 8;
		} while (rowInputSample < rowEnd);
		outputSample += outputPitch;
		inputSample += inputPitch * 2;
	} while (outputSample < imageEnd);
}

static void reduceHalfx1x16(const uint8_t* inputBuffer, uint8_t* outputBuffer, unsigned int width, unsigned int height, unsigned int pitch, unsigned int outputPitch, unsigned int outputCapacity)
{
	unsigned int rowLength = width & (~0xF); // align on 16 byte boundary
	if(rowLength > 0) {
		vMask128 unpackMask0 = v128_set_mask(V64_MASK_HI(ZMASK, 6, ZMASK, 4, ZMASK, 2, ZMASK, 0), V64_MASK_LO(ZMASK, 6, ZMASK, 4, ZMASK, 2, ZMASK, 0));
		vMask128 unpackMask1 = v128_set_mask(V64_MASK_HI(ZMASK, 7, ZMASK, 5, ZMASK, 3, ZMASK, 1), V64_MASK_LO(ZMASK, 7, ZMASK, 5, ZMASK, 3, ZMASK, 1));
		reduceHalfx16_inner(inputBuffer, outputBuffer, width, height, pitch, outputPitch, outputCapacity, rowLength, unpackMask0, unpackMask1);
	}
	// 8 source pixels at a time
	reduceHalfx1x8(&inputBuffer[rowLength], &outputBuffer[rowLength / 2], width - rowLength, height, pitch, outputPitch, outputCapacity);
}

static void reduceHalfx2x16(const uint8_t* inputBuffer, uint8_t* outputBuffer, unsigned int width, unsigned int height, unsigned int pitch, unsigned int outputPitch, unsigned int outputCapacity)
{
	unsigned int rowLength = (width * 2) & (~0xF); // align on 16 byte boundary
	if(rowLength > 0) {
		vMask128 unpackMask0 = v128_set_mask(V64_MASK_HI(ZMASK, 5, ZMASK, 4, ZMASK, 1, ZMASK, 0), V64_MASK_LO(ZMASK, 5, ZMASK, 4, ZMASK, 1, ZMASK, 0));
		vMask128 unpackMask1 = v128_set_mask(V64_MASK_HI(ZMASK, 7, ZMASK, 6, ZMASK, 3, ZMASK, 2), V64_MASK_LO(ZMASK, 7, ZMASK, 6, ZMASK, 3, ZMASK, 2));
		reduceHalfx16_inner(inputBuffer, outputBuffer, width, height, pitch, outputPitch, outputCapacity, rowLength, unpackMask0, unpackMask1);
	}
	// 8 source pixels at a time
	reduceHalfx2x8(&inputBuffer[rowLength], &outputBuffer[rowLength / 2], width - rowLength / 2, height, pitch, outputPitch, outputCapacity);
}

template<>
void Filters<ComponentSIMD<1>>::reduceHalf(const uint8_t* inputBuffer, uint8_t* outputBuffer, unsigned int width, unsigned int height, unsigned int pitch, unsigned int outputPitch, unsigned int outputCapacity)
{
	if(FiltersConfig::m_ScalarMode) {
		Filters<ComponentScalar<1>>::reduceHalf(inputBuffer, outputBuffer, width, height, pitch, outputPitch, outputCapacity);
		return;
	}
#if IMAGECORE_DETECT_SSE
	if( !checkForCPUSupport(kCPUFeature_SSE4_1)) {
		Filters<ComponentScalar<1>>::reduceHalf(inputBuffer, outputBuffer, width, height, pitch, outputPitch, outputCapacity);
		return;
	}
#endif
	reduceHalfx1x16(inputBuffer, outputBuffer, width, height, pitch, outputPitch, outputCapacity);
}

template<>
void Filters<ComponentSIMD<2>>::reduceHalf(const uint8_t* inputBuffer, uint8_t* outputBuffer, unsigned int width, unsigned int height, unsigned int pitch, unsigned int outputPitch, unsigned int outputCapacity)
{
	if(FiltersConfig::m_ScalarMode) {
		Filters<ComponentScalar<2>>::reduceHalf(inputBuffer, outputBuffer, width, height, pitch, outputPitch, outputCapacity);
		return;
	}
#if IMAGECORE_DETECT_SSE
	if( !checkForCPUSupport(kCPUFeature_SSE4_1)) {
		Filters<ComponentScalar<2>>::reduceHalf(inputBuffer, outputBuffer, width, height, pitch, outputPitch, outputCapacity);
		return;
	}
#endif
	reduceHalfx2x16(inputBuffer, outputBuffer, width, height, pitch, outputPitch, outputCapacity);
}

static void transpose1x16(const uint8_t* __restrict input_buffer, uint8_t* __restrict output_buffer, unsigned int width, unsigned int height, unsigned int input_pitch, unsigned int output_pitch, unsigned int output_capacity)
{
	const uint32_t h_blocks = width / 16;
	const uint32_t v_blocks = height / 4;
	if((h_blocks > 0) && (v_blocks > 0)) {
		const uint32_t outputPitch4 = output_pitch / 4;
		for(uint32_t v_index = 0; v_index < v_blocks; v_index++ ) {
			for(uint32_t h_index = 0; h_index < h_blocks; h_index++) {
				const uint8_t* srcBlock = input_buffer + v_index * 4 * input_pitch + h_index * 16;
				uint8_t* dstBlock = output_buffer + h_index * 16 * output_pitch + v_index * 4;
				vSInt8 srcRow0 = v128_load_unaligned((const vSInt32 *)(srcBlock + input_pitch * 0)); // a0 b0 c0 d0  e0 f0 g0 h0  i0 j0 k0 l0  m0 n0 o0 p0
				vSInt8 srcRow1 = v128_load_unaligned((const vSInt32 *)(srcBlock + input_pitch * 1)); // a1 b1 c1 d1  e1 f1 g1 h1  i1 j1 k1 l1  m1 n1 o1 p1
				vSInt8 srcRow2 = v128_load_unaligned((const vSInt32 *)(srcBlock + input_pitch * 2)); // a2 b2 c2 d2  e2 f2 g2 h2  i2 j2 k2 l2  m2 n2 o2 p2
				vSInt8 srcRow3 = v128_load_unaligned((const vSInt32 *)(srcBlock + input_pitch * 3)); // a3 b3 c3 d3  e3 f3 g3 h3  i3 j3 k3 l3  m3 n3 o3 p3

				vSInt64 dstRow0;
				vSInt64 dstRow1;
				vSInt64 dstRow2;
				vSInt64 dstRow3;
				vec_transpose_int8(srcRow0, srcRow1, srcRow2, srcRow3, dstRow0, dstRow1, dstRow2, dstRow3); // a0 a1 a2 a3  e0 e1 e2 e3  i0 i1 i2 i3  m0 m1 m2 m3
																											// b0 b1 b2 b3  f0 f1 f2 f3  j0 j1 j2 j3  n0 n1 n2 n3
																											// c0 c1 c2 c3  g0 g1 g2 g3  k0 k1 k2 k3  o0 01 02 03
																											// d0 d1 d2 d3  h0 h1 h2 h3  l0 l1 l2 l3  p0 p1 p2 p3
				// now need to store by 4x4 blocks at a time
				uint32_t* blockStart = (uint32_t*)dstBlock;
				*blockStart = v128_convert_to_int32(dstRow0);         // a0 a1 a2 a3
				blockStart += outputPitch4;
				*blockStart = v128_convert_to_int32(dstRow1);         // b0 b1 b2 b3
				blockStart += outputPitch4;
				*blockStart = v128_convert_to_int32(dstRow2);         // c0 c1 c2 c3
				blockStart += outputPitch4;
				*blockStart = v128_convert_to_int32(dstRow3);         // d0 d1 d2 d3
				blockStart += outputPitch4;

				*blockStart = v128_convert_lane_to_int32<2>(dstRow0); // e0 e1 e2 e3
				blockStart += outputPitch4;
				*blockStart = v128_convert_lane_to_int32<2>(dstRow1); // f0 f1 f2 f3
				blockStart += outputPitch4;
				*blockStart = v128_convert_lane_to_int32<2>(dstRow2); // g0 g1 g2 g3
				blockStart += outputPitch4;
				*blockStart = v128_convert_lane_to_int32<2>(dstRow3); // h0 h1 h2 h3
				blockStart += outputPitch4;

				*blockStart = v128_convert_lane_to_int32<1>(dstRow0); // i0 i1 i2 i3
				blockStart += outputPitch4;
				*blockStart = v128_convert_lane_to_int32<1>(dstRow1); // j0 j1 j2 j3
				blockStart += outputPitch4;
				*blockStart = v128_convert_lane_to_int32<1>(dstRow2); // k0 k1 k2 k3
				blockStart += outputPitch4;
				*blockStart = v128_convert_lane_to_int32<1>(dstRow3); // l0 l1 l2 l3
				blockStart += outputPitch4;

				*blockStart = v128_convert_lane_to_int32<3>(dstRow0); // m0 m1 m2 m3
				blockStart += outputPitch4;
				*blockStart = v128_convert_lane_to_int32<3>(dstRow1); // n0 n1 n2 n3
				blockStart += outputPitch4;
				*blockStart = v128_convert_lane_to_int32<3>(dstRow2); // o0 01 02 03
				blockStart += outputPitch4;
				*blockStart = v128_convert_lane_to_int32<3>(dstRow3); // p0 p1 p2 p3
				blockStart += outputPitch4;
			}
		}
		// finished the top left region, now need to do top right, bot left and bot right
		const uint32_t colsLeft = width - h_blocks * 16;
		const uint32_t rowsLeft = height - v_blocks * 4;

		const uint32_t topRightWidth = colsLeft;
		const uint32_t topRightHeight = height - rowsLeft;
		const uint8_t* topRightInput = input_buffer + width - topRightWidth;
		uint8_t* topRightOutput = output_buffer + (width - topRightWidth) * output_pitch;
		Filters<ComponentScalar<1>>::transpose(topRightInput, topRightOutput, topRightWidth, topRightHeight, input_pitch, output_pitch, output_capacity); // top right

		const uint32_t botLeftWidth = width - colsLeft;
		const uint32_t botLeftHeight = rowsLeft;
		const uint8_t* botLeftInput = input_buffer + (height - rowsLeft) * input_pitch;
		uint8_t* botLeftOutput = output_buffer + (height - rowsLeft);
		Filters<ComponentScalar<1>>::transpose(botLeftInput, botLeftOutput, botLeftWidth, botLeftHeight, input_pitch, output_pitch, output_capacity); // bot left

		const uint32_t botRightWidth = colsLeft;
		const uint32_t botRightHeight = rowsLeft;
		const uint8_t* botRightInput = input_buffer + (height - rowsLeft) * input_pitch + (width - colsLeft);
		uint8_t* botRightOutput = output_buffer + (width - colsLeft) * output_pitch + (height - rowsLeft);
		Filters<ComponentScalar<1>>::transpose(botRightInput, botRightOutput, botRightWidth, botRightHeight, input_pitch, output_pitch, output_capacity); // cols/rows left
	} else {
		Filters<ComponentScalar<1>>::transpose(input_buffer, output_buffer, width, height, input_pitch, output_pitch, output_capacity);
	}
}

template<>
void Filters<ComponentSIMD<1>>::transpose(const uint8_t* __restrict input_buffer, uint8_t* __restrict output_buffer, unsigned int width, unsigned int height, unsigned int input_pitch, unsigned int output_pitch, unsigned int output_capacity)
{
	if(FiltersConfig::m_ScalarMode) {
		Filters<ComponentScalar<1>>::transpose(input_buffer, output_buffer, width, height, input_pitch, output_pitch, output_capacity);
		return;
	}
#if IMAGECORE_DETECT_SSE
	if( !checkForCPUSupport(kCPUFeature_SSE4_1)) {
		Filters<ComponentScalar<1>>::transpose(input_buffer, output_buffer, width, height, input_pitch, output_pitch, output_capacity);
		return;
	}
#endif
	transpose1x16(input_buffer, output_buffer, width, height, input_pitch, output_pitch, output_capacity);
}


static void transpose2x16(const uint8_t* __restrict input_buffer, uint8_t* __restrict output_buffer, unsigned int width, unsigned int height, unsigned int input_pitch, unsigned int output_pitch, unsigned int output_capacity)
{
	const uint32_t h_blocks = width / 8;
	const uint32_t v_blocks = height / 4;
	if((h_blocks > 0) && (v_blocks > 0)) {
		const uint32_t outputPitch4 = output_pitch / 4;
		for(uint32_t v_index = 0; v_index < v_blocks; v_index++ ) {
			for(uint32_t h_index = 0; h_index < h_blocks; h_index++) {
				const uint8_t* srcBlock = input_buffer + v_index * 4 * input_pitch + h_index * 16;
				uint8_t* dstBlock = output_buffer + h_index * 8 * output_pitch + v_index * 8;
				vUInt16 srcRow0 = v128_load_unaligned((const vSInt32 *)(srcBlock + input_pitch * 0)); // a0 b0 c0 d0  e0 f0 g0 h0
				vUInt16 srcRow1 = v128_load_unaligned((const vSInt32 *)(srcBlock + input_pitch * 1)); // a1 b1 c1 d1  e1 f1 g1 h1
				vUInt16 srcRow2 = v128_load_unaligned((const vSInt32 *)(srcBlock + input_pitch * 2)); // a2 b2 c2 d2  e2 f2 g2 h2
				vUInt16 srcRow3 = v128_load_unaligned((const vSInt32 *)(srcBlock + input_pitch * 3)); // a3 b3 c3 d3  e3 f3 g3 h3

				vSInt64 dstRow0;
				vSInt64 dstRow1;
				vSInt64 dstRow2;
				vSInt64 dstRow3;
				vec_transpose_int16(srcRow0, srcRow1, srcRow2, srcRow3, dstRow0, dstRow1, dstRow2, dstRow3); // a0 a1 a2 a3  e0 e1 e2 e3
																											 // b0 b1 b2 b3  f0 f1 f2 f3
																											 // c0 c1 c2 c3  g0 g1 g2 g3
																											 // d0 d1 d2 d3  h0 h1 h2 h3
				// now need to store by 4x4 blocks at a time
				uint32_t* blockStart = (uint32_t*)dstBlock;
				*blockStart = v128_convert_to_int32(dstRow0);         // a0 a1
				blockStart += outputPitch4;
				*blockStart = v128_convert_to_int32(dstRow1);         // b0 b1
				blockStart += outputPitch4;
				*blockStart = v128_convert_to_int32(dstRow2);         // c0 c1
				blockStart += outputPitch4;
				*blockStart = v128_convert_to_int32(dstRow3);         // d0 d1
				blockStart += outputPitch4;

				*blockStart = v128_convert_lane_to_int32<2>(dstRow0); // e0 e1
				blockStart += outputPitch4;
				*blockStart = v128_convert_lane_to_int32<2>(dstRow1); // f0 f1
				blockStart += outputPitch4;
				*blockStart = v128_convert_lane_to_int32<2>(dstRow2); // g0 g1
				blockStart += outputPitch4;
				*blockStart = v128_convert_lane_to_int32<2>(dstRow3); // h0 h1
				blockStart += outputPitch4;

				blockStart = (uint32_t*)(dstBlock + 4);
				*blockStart = v128_convert_lane_to_int32<1>(dstRow0); // a2 a3
				blockStart += outputPitch4;
				*blockStart = v128_convert_lane_to_int32<1>(dstRow1); // b2 b3
				blockStart += outputPitch4;
				*blockStart = v128_convert_lane_to_int32<1>(dstRow2); // c2 c3
				blockStart += outputPitch4;
				*blockStart = v128_convert_lane_to_int32<1>(dstRow3); // d2 d3
				blockStart += outputPitch4;

				*blockStart = v128_convert_lane_to_int32<3>(dstRow0); // e2 e3
				blockStart += outputPitch4;
				*blockStart = v128_convert_lane_to_int32<3>(dstRow1); // f2 f3
				blockStart += outputPitch4;
				*blockStart = v128_convert_lane_to_int32<3>(dstRow2); // g2 g3
				blockStart += outputPitch4;
				*blockStart = v128_convert_lane_to_int32<3>(dstRow3); // h2 h3
				blockStart += outputPitch4;
			}
		}
		// finished the top left region, now need to do top right, bot left and bot right
		const uint32_t colsLeft = width - h_blocks * 8;
		const uint32_t rowsLeft = height - v_blocks * 4;

		const uint32_t topRightWidth = colsLeft;
		const uint32_t topRightHeight = height - rowsLeft;
		const uint8_t* topRightInput = input_buffer + 2 * (width - topRightWidth);
		uint8_t* topRightOutput = output_buffer + (width - topRightWidth) * output_pitch;
		Filters<ComponentScalar<2>>::transpose(topRightInput, topRightOutput, topRightWidth, topRightHeight, input_pitch, output_pitch, output_capacity); // top right

		const uint32_t botLeftWidth = width - colsLeft;
		const uint32_t botLeftHeight = rowsLeft;
		const uint8_t* botLeftInput = input_buffer + (height - rowsLeft) * input_pitch;
		uint8_t* botLeftOutput = output_buffer + 2 * (height - rowsLeft);
		Filters<ComponentScalar<2>>::transpose(botLeftInput, botLeftOutput, botLeftWidth, botLeftHeight, input_pitch, output_pitch, output_capacity); // bot left

		const uint32_t botRightWidth = colsLeft;
		const uint32_t botRightHeight = rowsLeft;
		const uint8_t* botRightInput = input_buffer + (height - rowsLeft) * input_pitch + 2 * (width - colsLeft);
		uint8_t* botRightOutput = output_buffer + (width - colsLeft) * output_pitch + 2 * (height - rowsLeft);
		Filters<ComponentScalar<2>>::transpose(botRightInput, botRightOutput, botRightWidth, botRightHeight, input_pitch, output_pitch, output_capacity); // cols/rows left
	} else {
		Filters<ComponentScalar<2>>::transpose(input_buffer, output_buffer, width, height, input_pitch, output_pitch, output_capacity);
	}
}

template<>
void Filters<ComponentSIMD<2>>::transpose(const uint8_t* __restrict input_buffer, uint8_t* __restrict output_buffer, unsigned int width, unsigned int height, unsigned int input_pitch, unsigned int output_pitch, unsigned int output_capacity)
{
	if(FiltersConfig::m_ScalarMode) {
		Filters<ComponentScalar<2>>::transpose(input_buffer, output_buffer, width, height, input_pitch, output_pitch, output_capacity);
		return;
	}
#if IMAGECORE_DETECT_SSE
	if( !checkForCPUSupport(kCPUFeature_SSE4_1)) {
		Filters<ComponentScalar<2>>::transpose(input_buffer, output_buffer, width, height, input_pitch, output_pitch, output_capacity);
		return;
	}
#endif
	transpose2x16(input_buffer, output_buffer, width, height, input_pitch, output_pitch, output_capacity);
}

template<uint32_t ComponentSize>
void bilinearTwoLinesx16(uint8_t* dstRow, const uint8_t* srcRow0, const uint8_t* srcRow1, uint16_t coeff0, uint16_t coeff1, uint32_t length)
{
	length = length * ComponentSize;
	const uint32_t block_16 = length / 16;
	if(block_16 > 0) {
		vUInt16 coeffRow0 = v128_set_int16(coeff0);
		vUInt16 coeffRow1 = v128_set_int16(coeff1);
		vUInt8 zero = v128_setzero();
		for(uint32_t block = 0; block < block_16; block++) {
			vUInt8 row0 = v128_load_unaligned((const vSInt32 *)srcRow0);
			vUInt8 row1 = v128_load_unaligned((const vSInt32 *)srcRow1);
			vSInt8 row0_a;
			vSInt8 row0_b;
			vSInt8 row1_a;
			vSInt8 row1_b;
			v128_unpack_int8(row0_a, row0_b, row0, zero);
			v128_unpack_int8(row1_a, row1_b, row1, zero);
			row0_a = v128_mul_int16(row0_a, coeffRow0);
			row0_b = v128_mul_int16(row0_b, coeffRow0);
			row1_a = v128_mul_int16(row1_a, coeffRow1);
			row1_b = v128_mul_int16(row1_b, coeffRow1);
			vUInt16 a_res = v128_add_int16(row0_a, row1_a);
			vUInt16 b_res = v128_add_int16(row0_b, row1_b);
			a_res = v128_shift_right_unsigned_int16<8>(a_res);
			b_res = v128_shift_right_unsigned_int16<8>(b_res);
			vUInt8x8 a_packed = v128_pack_unsigned_saturate_int16(a_res, zero, zero);
			vUInt8x8 b_packed = v128_pack_unsigned_saturate_int16(b_res, zero, zero);
			v64_store((vSInt32*)dstRow, a_packed);
			dstRow += 8;
			v64_store((vSInt32*)dstRow, b_packed);
			dstRow += 8;
			srcRow0 += 16;
			srcRow1 += 16;
		}
	}
	const uint32_t bytesProcessed = block_16 * 16;
	Filters<ComponentScalar<ComponentSize>>::bilinearTwoLines(dstRow, srcRow0, srcRow1, coeff0, coeff1, (length - bytesProcessed) / ComponentSize);
}

template<>
void Filters<ComponentSIMD<1>>::bilinearTwoLines(uint8_t* dstRow, const uint8_t* srcRow0, const uint8_t* srcRow1, uint16_t coeff0, uint16_t coeff1, uint32_t length)
{
	if(FiltersConfig::m_ScalarMode) {
		Filters<ComponentScalar<1>>::bilinearTwoLines(dstRow, srcRow0, srcRow1, coeff0, coeff1, length);
		return;
	}
#if IMAGECORE_DETECT_SSE
	if( !checkForCPUSupport(kCPUFeature_SSE4_1)) {
		Filters<ComponentScalar<1>>::bilinearTwoLines(dstRow, srcRow0, srcRow1, coeff0, coeff1, length);
		return;
	}
#endif
	bilinearTwoLinesx16<1>(dstRow, srcRow0, srcRow1, coeff0, coeff1, length);
}

template<>
void Filters<ComponentSIMD<2>>::bilinearTwoLines(uint8_t* dstRow, const uint8_t* srcRow0, const uint8_t* srcRow1, uint16_t coeff0, uint16_t coeff1, uint32_t length)
{
	if(FiltersConfig::m_ScalarMode) {
		Filters<ComponentScalar<2>>::bilinearTwoLines(dstRow, srcRow0, srcRow1, coeff0, coeff1, length);
		return;
	}
#if IMAGECORE_DETECT_SSE
	if( !checkForCPUSupport(kCPUFeature_SSE4_1)) {
		Filters<ComponentScalar<2>>::bilinearTwoLines(dstRow, srcRow0, srcRow1, coeff0, coeff1, length);
		return;
	}
#endif
	bilinearTwoLinesx16<2>(dstRow, srcRow0, srcRow1, coeff0, coeff1, length);
}

}

#endif
