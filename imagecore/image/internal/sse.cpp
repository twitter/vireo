//
//  sse.cpp
//  ImageCore
//
//  Created by Luke Alonso on 8/21/14.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#include "filters.h"
#include "imagecore/imagecore.h"
#include "imagecore/utils/securemath.h"
#include "imagecore/utils/mathutils.h"
#include "platform_support.h"

#if __SSE4_1__

#include "intrinsics.h"

#define vec_transpose_epi32(r0, r1, r2, r3) \
{ \
vSInt32 t0 = v128_unpacklo_int32(r0, r1); \
vSInt32 t1 = v128_unpacklo_int32(r2, r3); \
vSInt32 t2 = v128_unpackhi_int32(r0, r1); \
vSInt32 t3 = v128_unpackhi_int32(r2, r3); \
r0 = v128_unpacklo_int64(t0, t1); \
r1 = v128_unpackhi_int64(t0, t1); \
r2 = v128_unpacklo_int64(t2, t3); \
r3 = v128_unpackhi_int64(t2, t3); \
}

namespace imagecore {

// Adaptive-width filter, both axes, 4x4 samples.
// 16.16 Fixed point SSE version.
template<>
void Filters<ComponentSIMD<4>>::adaptive4x4(const FilterKernelAdaptive* kernelX, const FilterKernelAdaptive* kernelY, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity)
{
#if IMAGECORE_DETECT_SSE
    if( !checkForCPUSupport(kCPUFeature_SSE4_1)) {
        return Filters<ComponentScalar<4>>::adaptive4x4(kernelX, kernelY, inputBuffer, inputWidth, inputHeight, inputPitch, outputBuffer, outputWidth, outputHeight, outputPitch, outputCapacity);
    }
#endif
	SECURE_ASSERT(SafeUMul(outputWidth, 4U) <= outputPitch);
	SECURE_ASSERT(SafeUMul(outputHeight, outputPitch) <= outputCapacity);

	__m128i zero = v128_setzero();
	__m128i half = v128_set_int32(kHalf22);

	__restrict int32_t* kernelTableX = kernelX->getTableFixedPoint4();
	__restrict int32_t* kernelTableY = kernelY->getTableFixedPoint4();

	for( unsigned int y = 0; y < outputHeight; y++ ) {
		int startY = kernelY->computeSampleStart(y);
		for( unsigned int x = 0; x < outputWidth; x++ ) {
			int startX = kernelX->computeSampleStart(x);
			int sampleOffset = ((startY) * (int)inputPitch) + (startX) * 4;
			const uint8_t* sample = inputBuffer + sampleOffset;

			vSInt32 final = zero;

			unsigned int filterIndexX = x * 16;
			unsigned int filter_index_y = y * 16;
			vSInt32 coeffs_x_0 = *(vSInt32*)(kernelTableX + filterIndexX + 0);
			vSInt32 coeffs_x_1 = *(vSInt32*)(kernelTableX + filterIndexX + 4);
			vSInt32 coeffs_x_2 = *(vSInt32*)(kernelTableX + filterIndexX + 8);
			vSInt32 coeffs_x_3 = *(vSInt32*)(kernelTableX + filterIndexX + 12);

			vSInt32 coeffs_y_0 = *(vSInt32*)(kernelTableY + filter_index_y + 0);
			{
				vUInt8 row_8 = v128_load_unaligned((const vSInt32*)sample);
				vUInt16 row_16_a = v128_unpacklo_int8(row_8, zero);
				vUInt16 row_16_b = v128_unpackhi_int8(row_8, zero);
				vSInt32 row_32_a = v128_unpacklo_int16(row_16_a, zero);
				vSInt32 row_32_b = v128_unpackhi_int16(row_16_a, zero);
				vSInt32 row_32_c = v128_unpacklo_int16(row_16_b, zero);
				vSInt32 row_32_d = v128_unpackhi_int16(row_16_b, zero);
				vSInt32 mul_a = v128_mul_int32(row_32_a, coeffs_x_0);
				vSInt32 mul_b = v128_mul_int32(row_32_b, coeffs_x_1);
				vSInt32 mul_c = v128_mul_int32(row_32_c, coeffs_x_2);
				vSInt32 mul_d = v128_mul_int32(row_32_d, coeffs_x_3);
				vSInt32 row = v128_shift_right_signed_int32<10>(v128_add_int32(mul_a, v128_add_int32(mul_b, v128_add_int32(mul_c, mul_d))));
				final = v128_add_int32(final, v128_mul_int32(row, coeffs_y_0));
				sample += inputPitch;
			}

			vSInt32 coeffs_y_1 = *(vSInt32*)(kernelTableY + filter_index_y + 4);
			{
				vUInt8 row_8 = v128_load_unaligned((const vSInt32*)sample);
				vUInt16 row_16_a = v128_unpacklo_int8(row_8, zero);
				vUInt16 row_16_b = v128_unpackhi_int8(row_8, zero);
				vSInt32 row_32_a = v128_unpacklo_int16(row_16_a, zero);
				vSInt32 row_32_b = v128_unpackhi_int16(row_16_a, zero);
				vSInt32 row_32_c = v128_unpacklo_int16(row_16_b, zero);
				vSInt32 row_32_d = v128_unpackhi_int16(row_16_b, zero);
				vSInt32 mul_a = v128_mul_int32(row_32_a, coeffs_x_0);
				vSInt32 mul_b = v128_mul_int32(row_32_b, coeffs_x_1);
				vSInt32 mul_c = v128_mul_int32(row_32_c, coeffs_x_2);
				vSInt32 mul_d = v128_mul_int32(row_32_d, coeffs_x_3);
				vSInt32 row = v128_shift_right_signed_int32<10>(v128_add_int32(mul_a, v128_add_int32(mul_b, v128_add_int32(mul_c, mul_d))));
				final = v128_add_int32(final, v128_mul_int32(row, coeffs_y_1));
				sample += inputPitch;
			}

			vSInt32 coeffs_y_2 = *(vSInt32*)(kernelTableY + filter_index_y + 8);
			{
				vUInt8 row_8 = v128_load_unaligned((const vSInt32*)sample);
				vUInt16 row_16_a = v128_unpacklo_int8(row_8, zero);
				vUInt16 row_16_b = v128_unpackhi_int8(row_8, zero);
				vSInt32 row_32_a = v128_unpacklo_int16(row_16_a, zero);
				vSInt32 row_32_b = v128_unpackhi_int16(row_16_a, zero);
				vSInt32 row_32_c = v128_unpacklo_int16(row_16_b, zero);
				vSInt32 row_32_d = v128_unpackhi_int16(row_16_b, zero);
				vSInt32 mul_a = v128_mul_int32(row_32_a, coeffs_x_0);
				vSInt32 mul_b = v128_mul_int32(row_32_b, coeffs_x_1);
				vSInt32 mul_c = v128_mul_int32(row_32_c, coeffs_x_2);
				vSInt32 mul_d = v128_mul_int32(row_32_d, coeffs_x_3);
				vSInt32 row = v128_shift_right_signed_int32<10>(v128_add_int32(mul_a, v128_add_int32(mul_b, v128_add_int32(mul_c, mul_d))));
				final = v128_add_int32(final, v128_mul_int32(row, coeffs_y_2));
				sample += inputPitch;
			}

			vSInt32 coeffs_y_3 = *(vSInt32*)(kernelTableY + filter_index_y + 12);
			{
				vUInt8 row_8 = v128_load_unaligned((const vSInt32*)sample);
				vUInt16 row_16_a = v128_unpacklo_int8(row_8, zero);
				vUInt16 row_16_b = v128_unpackhi_int8(row_8, zero);
				vSInt32 row_32_a = v128_unpacklo_int16(row_16_a, zero);
				vSInt32 row_32_b = v128_unpackhi_int16(row_16_a, zero);
				vSInt32 row_32_c = v128_unpacklo_int16(row_16_b, zero);
				vSInt32 row_32_d = v128_unpackhi_int16(row_16_b, zero);
				vSInt32 mul_a = v128_mul_int32(row_32_a, coeffs_x_0);
				vSInt32 mul_b = v128_mul_int32(row_32_b, coeffs_x_1);
				vSInt32 mul_c = v128_mul_int32(row_32_c, coeffs_x_2);
				vSInt32 mul_d = v128_mul_int32(row_32_d, coeffs_x_3);
				vSInt32 row = v128_shift_right_signed_int32<10>(v128_add_int32(mul_a, v128_add_int32(mul_b, v128_add_int32(mul_c, mul_d))));
				final = v128_add_int32(final, v128_mul_int32(row, coeffs_y_3));
			}
			final = v128_add_int32(final, half);
			final = v128_shift_right_signed_int32<22>(final);
			vSInt16 packed_16 = v128_pack_unsigned_saturate_int32(final, zero);
			vSInt8 packed_8 = v128_pack_unsigned_saturate_int16(packed_16, zero, zero);
			unsigned int oi = (y * outputPitch) + x * 4;
			int a = v128_convert_to_int32(packed_8);
			*(int*)(outputBuffer + oi) = a;
		}
	}
}


// Adaptive-width filter, single axis, any number of samples.
// 16.16 Fixed point SSE version.
static void adaptiveSeperableAny(const FilterKernelAdaptive* kernel, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity)
{
	SECURE_ASSERT(SafeUMul(outputWidth, 4U) <= outputPitch);
	SECURE_ASSERT(SafeUMul(outputHeight, outputPitch) <= outputCapacity);

	__restrict int32_t* kernelTable = kernel->getTableFixedPoint4();
	unsigned int kernelWidth = kernel->getKernelSize();

	__m128i zero = v128_setzero();
	__m128i half = v128_set_int32(kHalf16);

	for( unsigned int y = 0; y < outputHeight; y++ ) {
		for( unsigned int x = 0; x < outputWidth; x++ ) {
			int startX = kernel->computeSampleStart(x);
			int sampleOffset = (y * (int)inputPitch) + (startX) * 4;
			const uint8_t* sample = inputBuffer + sampleOffset;

			vSInt32 result = zero;

			for( unsigned int section = 0; section < kernelWidth; section += 4 ) {
				unsigned int filterIndexX = x * kernelWidth * 4 + section * 4;
				vSInt32 coeffs_x_0 = *(vSInt32*)(kernelTable + filterIndexX + 0);
				vSInt32 coeffs_x_1 = *(vSInt32*)(kernelTable + filterIndexX + 4);
				vSInt32 coeffs_x_2 = *(vSInt32*)(kernelTable + filterIndexX + 8);
				vSInt32 coeffs_x_3 = *(vSInt32*)(kernelTable + filterIndexX + 12);
				vUInt8 row_8_a = v128_load_unaligned((const vSInt32*)(sample + section * 4));
				vUInt16 row_16_a = v128_unpacklo_int8(row_8_a, zero);
				vUInt16 row_16_b = v128_unpackhi_int8(row_8_a, zero);
				vSInt32 row_32_a = v128_unpacklo_int16(row_16_a, zero);
				vSInt32 row_32_b = v128_unpackhi_int16(row_16_a, zero);
				vSInt32 row_32_c = v128_unpacklo_int16(row_16_b, zero);
				vSInt32 row_32_d = v128_unpackhi_int16(row_16_b, zero);
				vSInt32 mul_a = v128_mul_int32(row_32_a, coeffs_x_0);
				vSInt32 mul_b = v128_mul_int32(row_32_b, coeffs_x_1);
				vSInt32 mul_c = v128_mul_int32(row_32_c, coeffs_x_2);
				vSInt32 mul_d = v128_mul_int32(row_32_d, coeffs_x_3);
				result = v128_add_int32(result, v128_add_int32(mul_a, v128_add_int32(mul_b, v128_add_int32(mul_c, mul_d))));
			}
			result = v128_add_int32(result, half);
			result = v128_shift_right_signed_int32<16>(result);

			vSInt16 packed_16 = v128_pack_unsigned_saturate_int32(result, zero);
			vSInt8 packed_8 = v128_pack_unsigned_saturate_int16(packed_16, zero, zero);
			unsigned int oi = (x * outputPitch) + y * 4;
			int a = v128_convert_to_int32(packed_8);
			*(int*)(outputBuffer + oi) = a;
		}
	}
}

// Adaptive-width filter, single axis, 8 samples.
// 16.16 Fixed point SSE version.
static void adaptiveSeperable8(const FilterKernelAdaptive* kernel, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity)
{
	SECURE_ASSERT(SafeUMul(outputHeight, 4U) <= outputPitch);
	SECURE_ASSERT(SafeUMul(outputWidth, outputPitch) <= outputCapacity);

	__restrict int32_t* kernelTable = kernel->getTableFixedPoint4();

	__m128i zero = v128_setzero();
	__m128i half = v128_set_int32(kHalf16);

	for( unsigned int y = 0; y < outputHeight; y++ ) {
		for( unsigned int x = 0; x < outputWidth; x++ ) {

			int startX = kernel->computeSampleStart(x);

			int sampleOffset = (y * (int)inputPitch) + (startX) * 4;
			const uint8_t* sample = inputBuffer + sampleOffset;

			unsigned int filterIndexX = x * 32;

			vSInt32 coeffs_x_0 = *(vSInt32*)(kernelTable + filterIndexX + 0);
			vSInt32 coeffs_x_1 = *(vSInt32*)(kernelTable + filterIndexX + 4);
			vSInt32 coeffs_x_2 = *(vSInt32*)(kernelTable + filterIndexX + 8);
			vSInt32 coeffs_x_3 = *(vSInt32*)(kernelTable + filterIndexX + 12);
			vSInt32 coeffs_x_4 = *(vSInt32*)(kernelTable + filterIndexX + 16);
			vSInt32 coeffs_x_5 = *(vSInt32*)(kernelTable + filterIndexX + 20);
			vSInt32 coeffs_x_6 = *(vSInt32*)(kernelTable + filterIndexX + 24);
			vSInt32 coeffs_x_7 = *(vSInt32*)(kernelTable + filterIndexX + 28);

			vSInt32 result = zero;

			vUInt8 row_8_a = v128_load_unaligned((const vSInt32*)sample);
			vUInt16 row_16_a = v128_unpacklo_int8(row_8_a, zero);
			vUInt16 row_16_b = v128_unpackhi_int8(row_8_a, zero);
			vSInt32 row_32_a = v128_unpacklo_int16(row_16_a, zero);
			vSInt32 row_32_b = v128_unpackhi_int16(row_16_a, zero);
			vSInt32 row_32_c = v128_unpacklo_int16(row_16_b, zero);
			vSInt32 row_32_d = v128_unpackhi_int16(row_16_b, zero);
			vSInt32 mul_a = v128_mul_int32(row_32_a, coeffs_x_0);
			vSInt32 mul_b = v128_mul_int32(row_32_b, coeffs_x_1);
			vSInt32 mul_c = v128_mul_int32(row_32_c, coeffs_x_2);
			vSInt32 mul_d = v128_mul_int32(row_32_d, coeffs_x_3);

			result = v128_add_int32(mul_a, v128_add_int32(mul_b, v128_add_int32(mul_c, mul_d)));

			vUInt8 row_8_b = v128_load_unaligned((const vSInt32*)(sample + 16));
			vUInt16 row_16_c = v128_unpacklo_int8(row_8_b, zero);
			vUInt16 row_16_d = v128_unpackhi_int8(row_8_b, zero);
			vSInt32 row_32_e = v128_unpacklo_int16(row_16_c, zero);
			vSInt32 row_32_f = v128_unpackhi_int16(row_16_c, zero);
			vSInt32 row_32_g = v128_unpacklo_int16(row_16_d, zero);
			vSInt32 row_32_h = v128_unpackhi_int16(row_16_d, zero);
			vSInt32 mul_e = v128_mul_int32(row_32_e, coeffs_x_4);
			vSInt32 mul_f = v128_mul_int32(row_32_f, coeffs_x_5);
			vSInt32 mul_g = v128_mul_int32(row_32_g, coeffs_x_6);
			vSInt32 mul_h = v128_mul_int32(row_32_h, coeffs_x_7);

			result = v128_add_int32(result, v128_add_int32(mul_e, v128_add_int32(mul_f, v128_add_int32(mul_g, mul_h))));
			result = v128_add_int32(result, half);
			result = v128_shift_right_signed_int32<16>(result);

			vSInt16 packed_16 = v128_pack_unsigned_saturate_int32(result, zero);
			vSInt8 packed_8 = v128_pack_unsigned_saturate_int16(packed_16, zero, zero);
			unsigned int oi = (x * outputPitch) + y * 4;
			int a = v128_convert_to_int32(packed_8);
			*(int*)(outputBuffer + oi) = a;
		}
	}
}

#define SEPERABLE12_ASM_OPTIMIZATION 1

// Samples loading, unpacking and factoring in.
#define ADD_SAMPLE(x) inSample = v128_set_int32(*(samples32[x])); \
inSampleUnpacked = v128_shuffle_int8(inSample, unpackMask); \
sum = v128_add_int32(sum, v128_mul_int32(inSampleUnpacked, coeffs_x_##x));

#define SAMPLE_0 ADD_SAMPLE(0)
#define SAMPLE_1 SAMPLE_0 \
ADD_SAMPLE(1)
#define SAMPLE_2 SAMPLE_1 \
ADD_SAMPLE(2)
#define SAMPLE_3 SAMPLE_2 \
ADD_SAMPLE(3)
#define SAMPLE_4 SAMPLE_3 \
ADD_SAMPLE(4)
#define SAMPLE_5 SAMPLE_4 \
ADD_SAMPLE(5)
#define SAMPLE_6 SAMPLE_5 \
ADD_SAMPLE(6)
#define SAMPLE_7 SAMPLE_6 \
ADD_SAMPLE(7)
#define SAMPLE_8 SAMPLE_7 \
ADD_SAMPLE(8)
#define SAMPLE_9 SAMPLE_8 \
ADD_SAMPLE(9)
#define SAMPLE_10 SAMPLE_9 \
ADD_SAMPLE(10)
#define SAMPLE_11 SAMPLE_10 \
ADD_SAMPLE(11)

#define SAMPLE(x) SAMPLE_##x

// Filter coefficient loading
#define LOAD_COEFF(x) vSInt32 coeffs_x_##x = *(vSInt32*)(kernelTableSample + (x * 4));

#define COEFF_0 LOAD_COEFF(0)
#define COEFF_1 COEFF_0 \
LOAD_COEFF(1)
#define COEFF_2 COEFF_1 \
LOAD_COEFF(2)
#define COEFF_3 COEFF_2 \
LOAD_COEFF(3)
#define COEFF_4 COEFF_3 \
LOAD_COEFF(4)
#define COEFF_5 COEFF_4 \
LOAD_COEFF(5)
#define COEFF_6 COEFF_5 \
LOAD_COEFF(6)
#define COEFF_7 COEFF_6 \
LOAD_COEFF(7)
#define COEFF_8 COEFF_7 \
LOAD_COEFF(8)
#define COEFF_9 COEFF_8 \
LOAD_COEFF(9)
#define COEFF_10 COEFF_9 \
LOAD_COEFF(10)
#define COEFF_11 COEFF_10 \
LOAD_COEFF(11)

#define COEFF(x) COEFF_##x

#define ADAPTIVE_SEPERABLE_INIT SECURE_ASSERT(SafeUMul(outputHeight, 4U) <= outputPitch); \
SECURE_ASSERT(SafeUMul(outputWidth, outputPitch) <= outputCapacity); \
vSInt32 unpackMask = v128_set_int8_packed(0x80, 0x80, 0x80, 0x03, 0x80, 0x80, 0x80, 0x02, 0x80, 0x80, 0x80, 0x01, 0x80, 0x80, 0x80, 0x00); \
vSInt32 zero = v128_setzero(); \
vSInt32 half = v128_set_int32(kHalf16); \
__restrict int32_t* kernelTable = kernel->getTableFixedPoint4();

#define ADAPTIVE_SEPERABLE_XLOOP_START for( unsigned int x = 0; x < outputWidth; x++ ) { \
	int startX = kernel->computeSampleStart(x); \
	uint8_t* outputSample = outputBuffer + (x * outputPitch); \
	const int32_t* samples32[12]; \
	int32_t startIndex = startX < 0 ? 0 : startX; \
	const uint8_t* sample = inputBuffer + startIndex * 4; \
	for( int kernelIndex = 0; kernelIndex < 12; kernelIndex++ ) { \
		int sampleIndex; \
		if(startX < 0) { \
			sampleIndex = kernelIndex + startX < 0 ? 0 : kernelIndex + startX; \
			} else { \
			sampleIndex = kernelIndex + startIndex < inputWidth ? kernelIndex : kernelIndex - (kernelIndex + startIndex - inputWidth + 1); \
			} \
		sampleIndex = min(sampleIndex, (int32_t)inputWidth - 1); \
		samples32[kernelIndex] = (const int32_t*)&sample[sampleIndex * 4]; \
} \
const int32_t* kernelTableSample = kernelTable + x * 48;

#define ADAPTIVE_SEPERABLE_XLOOP_END }

#define ADAPTIVE_SEPERABLE_YLOOP_START for( unsigned int y = 0; y < outputHeight; y++ ) { \
	vSInt32 inSample; \
	vSInt32 inSampleUnpacked; \
	vSInt32 sum = zero;

#define ADAPTIVE_SEPERABLE_YLOOP_END sum = v128_add_int32(sum, half); \
	sum = v128_shift_right_signed_int32<16>(sum); \
	vSInt16 packed_16 = v128_pack_unsigned_saturate_int32(sum, zero); \
	vSInt8 packed_8 = v128_pack_unsigned_saturate_int16(packed_16, zero, zero); \
	int32_t a = v128_convert_to_int32(packed_8); \
	*((int32_t*)outputSample) = a; \
	outputSample += 4; \
	for( int kernelIndex = 0; kernelIndex < 12; kernelIndex++ ) { \
		samples32[kernelIndex] += (inputPitch / 4); \
	} \
}

#define ADAPTIVE_SEPERABLE_FUNC_CODE(x) ADAPTIVE_SEPERABLE_INIT \
ADAPTIVE_SEPERABLE_XLOOP_START \
COEFF(x) \
ADAPTIVE_SEPERABLE_YLOOP_START \
SAMPLE(x) \
ADAPTIVE_SEPERABLE_YLOOP_END \
ADAPTIVE_SEPERABLE_XLOOP_END

static void adaptiveSeperable12_maxSamples12(const FilterKernelAdaptive* kernel, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity)
{
	// The seperable version writes transposed images.
	ADAPTIVE_SEPERABLE_FUNC_CODE(11)
}

static void adaptiveSeperable12_maxSamples11(const FilterKernelAdaptive* kernel, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity)
{
	// The seperable version writes transposed images.
	ADAPTIVE_SEPERABLE_FUNC_CODE(10)
}

static void adaptiveSeperable12_maxSamples10(const FilterKernelAdaptive* kernel, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity)
{
	// The seperable version writes transposed images.
	ADAPTIVE_SEPERABLE_FUNC_CODE(9)
}

static void adaptiveSeperable12_maxSamples9(const FilterKernelAdaptive* kernel, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity)
{
	// The seperable version writes transposed images.
	ADAPTIVE_SEPERABLE_FUNC_CODE(8)
}

static void adaptiveSeperable12_maxSamples8(const FilterKernelAdaptive* kernel, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity)
{
	// The seperable version writes transposed images.
	ADAPTIVE_SEPERABLE_FUNC_CODE(7)
}

static void adaptiveSeperable12_maxSamples7(const FilterKernelAdaptive* kernel, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity)
{
	// The seperable version writes transposed images.
	ADAPTIVE_SEPERABLE_FUNC_CODE(6)
}

static void adaptiveSeperable12_maxSamples6(const FilterKernelAdaptive* kernel, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity)
{
	// The seperable version writes transposed images.
	ADAPTIVE_SEPERABLE_FUNC_CODE(5)
}

static void adaptiveSeperable12_maxSamples5(const FilterKernelAdaptive* kernel, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity)
{
	// The seperable version writes transposed images.
	ADAPTIVE_SEPERABLE_FUNC_CODE(4)
}

static void adaptiveSeperable12_maxSamples4(const FilterKernelAdaptive* kernel, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity)
{
	// The seperable version writes transposed images.
	ADAPTIVE_SEPERABLE_FUNC_CODE(3)
}

static void adaptiveSeperable12_maxSamples3(const FilterKernelAdaptive* kernel, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity)
{
	// The seperable version writes transposed images.
	ADAPTIVE_SEPERABLE_FUNC_CODE(2)
}

static void adaptiveSeperable12_maxSamples2(const FilterKernelAdaptive* kernel, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity)
{
	// The seperable version writes transposed images.
	ADAPTIVE_SEPERABLE_FUNC_CODE(1)
}

static void adaptiveSeperable12_maxSamples1(const FilterKernelAdaptive* kernel, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity)
{
	// The seperable version writes transposed images.
	ADAPTIVE_SEPERABLE_FUNC_CODE(0)
}

// Adaptive-width filter, single axis, 12 samples.
// 16.16 Fixed point SSE version
static void adaptiveSeperable12(const FilterKernelAdaptive* kernel, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity)
{
	// The seperable version writes transposed images.
	SECURE_ASSERT(SafeUMul(outputHeight, 4U) <= outputPitch);
	SECURE_ASSERT(SafeUMul(outputWidth, outputPitch) <= outputCapacity);

	__restrict int32_t* kernelTable = kernel->getTableFixedPoint4();

	__m128i half = v128_set_int32(kHalf16);

	for( unsigned int x = 0; x < outputWidth; x++ ) {
		const int32_t* kernelTableSample = kernelTable + x * 48;
		int startX = kernel->computeSampleStart(x);
		uint8_t* outputSample = outputBuffer + (x * outputPitch);
		const uint8_t* sample = inputBuffer + startX * 4;
		for( unsigned int y = 0; y < outputHeight; y++ ) {
#if (SEPERABLE12_ASM_OPTIMIZATION)
			__asm__ (
					 "pxor		%%xmm0, %%xmm0 \n"
					 "lddqu		%[sample], %%xmm1 \n"
					 "movdqa	%%xmm1, %%xmm2 \n"
					 "punpcklbw	%%xmm0, %%xmm1 \n"
					 "movdqa 	%%xmm1, %%xmm3 \n"
					 "punpcklwd	%%xmm0, %%xmm1 \n"
					 "punpckhwd	%%xmm0, %%xmm3 \n"
					 "pmulld 	0%[kernelTable], %%xmm1 \n"
					 "pmulld 	16%[kernelTable], %%xmm3 \n"
					 "paddd 	%%xmm3, %%xmm1 \n"
					 "punpckhbw	%%xmm0, %%xmm2 \n"
					 "movdqa 	%%xmm2, %%xmm3 \n"
					 "punpcklwd	%%xmm0, %%xmm2 \n"
					 "punpckhwd	%%xmm0, %%xmm3 \n"
					 "pmulld 	32%[kernelTable], %%xmm2 \n"
					 "pmulld 	48%[kernelTable], %%xmm3 \n"
					 "paddd 	%%xmm2, %%xmm1 \n"
					 "paddd 	%%xmm3, %%xmm1 \n"
					 "movdqa	%%xmm1, %%xmm4 \n"

					 "lddqu		16%[sample], %%xmm1 \n"
					 "movdqa	%%xmm1, %%xmm2 \n"
					 "punpcklbw	%%xmm0, %%xmm1 \n"
					 "movdqa 	%%xmm1, %%xmm3 \n"
					 "punpcklwd	%%xmm0, %%xmm1 \n"
					 "punpckhwd	%%xmm0, %%xmm3 \n"
					 "pmulld 	64%[kernelTable], %%xmm1 \n"
					 "pmulld 	80%[kernelTable], %%xmm3 \n"
					 "paddd 	%%xmm3, %%xmm1 \n"
					 "punpckhbw	%%xmm0, %%xmm2 \n"
					 "movdqa 	%%xmm2, %%xmm3 \n"
					 "punpcklwd	%%xmm0, %%xmm2 \n"
					 "punpckhwd	%%xmm0, %%xmm3 \n"
					 "pmulld 	96%[kernelTable], %%xmm2 \n"
					 "pmulld 	112%[kernelTable], %%xmm3 \n"
					 "paddd 	%%xmm2, %%xmm1 \n"
					 "paddd 	%%xmm3, %%xmm1 \n"
					 "paddd	 	%%xmm1, %%xmm4 \n"

					 "lddqu		32%[sample], %%xmm1 \n"
					 "movdqa	%%xmm1, %%xmm2 \n"
					 "punpcklbw	%%xmm0, %%xmm1 \n"
					 "movdqa 	%%xmm1, %%xmm3 \n"
					 "punpcklwd	%%xmm0, %%xmm1 \n"
					 "punpckhwd	%%xmm0, %%xmm3 \n"
					 "pmulld 	128%[kernelTable], %%xmm1 \n"
					 "pmulld 	144%[kernelTable], %%xmm3 \n"
					 "paddd 	%%xmm3, %%xmm1 \n"
					 "punpckhbw	%%xmm0, %%xmm2 \n"
					 "movdqa 	%%xmm2, %%xmm3 \n"
					 "punpcklwd	%%xmm0, %%xmm2 \n"
					 "punpckhwd	%%xmm0, %%xmm3 \n"
					 "pmulld 	160%[kernelTable], %%xmm2 \n"
					 "pmulld 	176%[kernelTable], %%xmm3 \n"
					 "paddd 	%%xmm2, %%xmm1 \n"
					 "paddd 	%%xmm3, %%xmm1 \n"
					 "paddd		%%xmm1, %%xmm4 \n"

					 "paddd 	%[half], %%xmm4 \n"
					 "psrad 	$16, %%xmm4 \n"
					 "packusdw 	%%xmm0, %%xmm4 \n"
					 "packuswb 	%%xmm0, %%xmm4 \n"
					 "movd 		%%xmm4, %[outputSample] \n"
					 : [outputSample] "=m" (*outputSample)
					 : [sample] "m" (*sample), [kernelTable] "m" (*kernelTableSample), [half] "x" (half)
					 : "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4"
					 );
#else
			__m128i zero = v128_setzero();

			vSInt32 result = zero;
			vSInt32 coeffs_x_0 = *(vSInt32*)(kernelTableSample + 0);
			vSInt32 coeffs_x_1 = *(vSInt32*)(kernelTableSample + 4);
			vSInt32 coeffs_x_2 = *(vSInt32*)(kernelTableSample + 8);
			vSInt32 coeffs_x_3 = *(vSInt32*)(kernelTableSample + 12);
			vSInt32 coeffs_x_4 = *(vSInt32*)(kernelTableSample + 16);
			vSInt32 coeffs_x_5 = *(vSInt32*)(kernelTableSample + 20);
			vSInt32 coeffs_x_6 = *(vSInt32*)(kernelTableSample + 24);
			vSInt32 coeffs_x_7 = *(vSInt32*)(kernelTableSample + 28);
			vSInt32 coeffs_x_8 = *(vSInt32*)(kernelTableSample + 32);
			vSInt32 coeffs_x_9 = *(vSInt32*)(kernelTableSample + 36);
			vSInt32 coeffs_x_10 = *(vSInt32*)(kernelTableSample + 40);
			vSInt32 coeffs_x_11 = *(vSInt32*)(kernelTableSample + 44);

			vUInt8 row_8_a = v128_load_unaligned((__m128i*)sample);
			vUInt16 row_16_a = v128_unpacklo_int8(row_8_a, zero);
			vUInt16 row_16_b = v128_unpackhi_int8(row_8_a, zero);
			vSInt32 row_32_a = v128_unpacklo_int16(row_16_a, zero);
			vSInt32 row_32_b = v128_unpackhi_int16(row_16_a, zero);
			vSInt32 row_32_c = v128_unpacklo_int16(row_16_b, zero);
			vSInt32 row_32_d = v128_unpackhi_int16(row_16_b, zero);
			vSInt32 mul_a = v128_mul_int32(row_32_a, coeffs_x_0);
			vSInt32 mul_b = v128_mul_int32(row_32_b, coeffs_x_1);
			vSInt32 mul_c = v128_mul_int32(row_32_c, coeffs_x_2);
			vSInt32 mul_d = v128_mul_int32(row_32_d, coeffs_x_3);

			result = v128_add_int32(mul_a, v128_add_int32(mul_b, v128_add_int32(mul_c, mul_d)));

			vUInt8 row_8_b = v128_load_unaligned((__m128i*)(sample + 16));
			vUInt16 row_16_c = v128_unpacklo_int8(row_8_b, zero);
			vUInt16 row_16_d = v128_unpackhi_int8(row_8_b, zero);
			vSInt32 row_32_e = v128_unpacklo_int16(row_16_c, zero);
			vSInt32 row_32_f = v128_unpackhi_int16(row_16_c, zero);
			vSInt32 row_32_g = v128_unpacklo_int16(row_16_d, zero);
			vSInt32 row_32_h = v128_unpackhi_int16(row_16_d, zero);
			vSInt32 mul_e = v128_mul_int32(row_32_e, coeffs_x_4);
			vSInt32 mul_f = v128_mul_int32(row_32_f, coeffs_x_5);
			vSInt32 mul_g = v128_mul_int32(row_32_g, coeffs_x_6);
			vSInt32 mul_h = v128_mul_int32(row_32_h, coeffs_x_7);

			result = v128_add_int32(result, v128_add_int32(mul_e, v128_add_int32(mul_f, v128_add_int32(mul_g, mul_h))));

			vUInt8 row_8_c = v128_load_unaligned((__m128i*)(sample + 32));
			vUInt16 row_16_e = v128_unpacklo_int8(row_8_c, zero);
			vUInt16 row_16_f = v128_unpackhi_int8(row_8_c, zero);
			vSInt32 row_32_i = v128_unpacklo_int16(row_16_e, zero);
			vSInt32 row_32_j = v128_unpackhi_int16(row_16_e, zero);
			vSInt32 row_32_k = v128_unpacklo_int16(row_16_f, zero);
			vSInt32 row_32_l = v128_unpackhi_int16(row_16_f, zero);
			vSInt32 mul_i = v128_mul_int32(row_32_i, coeffs_x_8);
			vSInt32 mul_j = v128_mul_int32(row_32_j, coeffs_x_9);
			vSInt32 mul_k = v128_mul_int32(row_32_k, coeffs_x_10);
			vSInt32 mul_l = v128_mul_int32(row_32_l, coeffs_x_11);

			result = v128_add_int32(result, v128_add_int32(mul_i, v128_add_int32(mul_j, v128_add_int32(mul_k, mul_l))));
			result = v128_add_int32(result, half);
			result = v128_shift_right_signed_int32<16>(result);

			vSInt16 packed_16 = v128_pack_unsigned_saturate_int32(result, zero);
			vSInt8 packed_8 = v128_pack_unsigned_saturate_int16(packed_16, zero);
			unsigned int oi = (x * outputPitch) + y * 4;
			int a = v128_convert_to_int32(packed_8);
			*(int*)(outputBuffer + oi) = a;
#endif
			outputSample += 4;
			sample += inputPitch;
		}
	}
}

// Adaptive-width filter, variable number of samples.
template<>
void Filters<ComponentSIMD<4>>::adaptiveSeperable(const FilterKernelAdaptive* kernel, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity, bool unpadded)
{
#if IMAGECORE_DETECT_SSE
    if( !checkForCPUSupport(kCPUFeature_SSE4_1)) {
        return Filters<ComponentScalar<4>>::adaptiveSeperable(kernel, inputBuffer, inputWidth, inputHeight, inputPitch, outputBuffer, outputWidth, outputHeight, outputPitch, outputCapacity, unpadded);
    }
#endif
	unsigned int kernelSize = kernel->getKernelSize();
	if( kernelSize == 8U ) {
		adaptiveSeperable8(kernel, inputBuffer, inputWidth, inputHeight, inputPitch, outputBuffer, outputWidth, outputHeight, outputPitch, outputCapacity);
	} else if( kernelSize == 12U ) {
		if( unpadded ) {
			switch((uint32_t)kernel->getMaxSamples()) {
				case 12: {
					adaptiveSeperable12_maxSamples12(kernel, inputBuffer, inputWidth, inputHeight, inputPitch, outputBuffer, outputWidth, outputHeight, outputPitch, outputCapacity);
					break;
				}
				case 11: {
					adaptiveSeperable12_maxSamples11(kernel, inputBuffer, inputWidth, inputHeight, inputPitch, outputBuffer, outputWidth, outputHeight, outputPitch, outputCapacity);
					break;
				}
				case 10: {
					adaptiveSeperable12_maxSamples10(kernel, inputBuffer, inputWidth, inputHeight, inputPitch, outputBuffer, outputWidth, outputHeight, outputPitch, outputCapacity);
					break;
				}
				case 9: {
					adaptiveSeperable12_maxSamples9(kernel, inputBuffer, inputWidth, inputHeight, inputPitch, outputBuffer, outputWidth, outputHeight, outputPitch, outputCapacity);
					break;
				}
				case 8: {
					adaptiveSeperable12_maxSamples8(kernel, inputBuffer, inputWidth, inputHeight, inputPitch, outputBuffer, outputWidth, outputHeight, outputPitch, outputCapacity);
					break;
				}
				case 7: {
					adaptiveSeperable12_maxSamples7(kernel, inputBuffer, inputWidth, inputHeight, inputPitch, outputBuffer, outputWidth, outputHeight, outputPitch, outputCapacity);
					break;
				}
				case 6: {
					adaptiveSeperable12_maxSamples6(kernel, inputBuffer, inputWidth, inputHeight, inputPitch, outputBuffer, outputWidth, outputHeight, outputPitch, outputCapacity);
					break;
				}
				case 5: {
					adaptiveSeperable12_maxSamples5(kernel, inputBuffer, inputWidth, inputHeight, inputPitch, outputBuffer, outputWidth, outputHeight, outputPitch, outputCapacity);
					break;
				}
				case 4: {
					adaptiveSeperable12_maxSamples4(kernel, inputBuffer, inputWidth, inputHeight, inputPitch, outputBuffer, outputWidth, outputHeight, outputPitch, outputCapacity);
					break;
				}
				case 3: {
					adaptiveSeperable12_maxSamples3(kernel, inputBuffer, inputWidth, inputHeight, inputPitch, outputBuffer, outputWidth, outputHeight, outputPitch, outputCapacity);
					break;
				}
				case 2: {
					adaptiveSeperable12_maxSamples2(kernel, inputBuffer, inputWidth, inputHeight, inputPitch, outputBuffer, outputWidth, outputHeight, outputPitch, outputCapacity);
					break;
				}
				case 1: {
					adaptiveSeperable12_maxSamples1(kernel, inputBuffer, inputWidth, inputHeight, inputPitch, outputBuffer, outputWidth, outputHeight, outputPitch, outputCapacity);
					break;
				}
			}
		} else {
			adaptiveSeperable12(kernel, inputBuffer, inputWidth, inputHeight, inputPitch, outputBuffer, outputWidth, outputHeight, outputPitch, outputCapacity);
		}
	} else {
		adaptiveSeperableAny(kernel, inputBuffer, inputWidth, inputHeight, inputPitch, outputBuffer, outputWidth, outputHeight, outputPitch, outputCapacity);
	}
}

// 4 rows at a time
void adaptiveSeperable8_12x4(const int32_t* sampleStarts, const FilterKernelAdaptive* kernel, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int input_height, unsigned int inputPitch, uint8_t* __restrict outputBuffer, int32_t sx, int32_t ex, int32_t sy, int32_t ey, unsigned int outputPitch, unsigned int outputCapacity)
{
	// The seperable version writes transposed images.
	SECURE_ASSERT(SafeUMul(ey - sy, 1U) <= outputPitch);
	SECURE_ASSERT(SafeUMul(ex - sx, outputPitch) <= outputCapacity);

	__restrict const vSInt32* kernelTable = (vSInt32*)kernel->getTableFixedPoint4();
	kernelTable += (sx * 12);

	__m128i half = v128_set_int32(kHalf16);
	__m128i zero = v128_setzero();
	vSInt32 unpackMask0 = v128_set_int8_packed(0x80, 0x80, 0x80, 0x03, 0x80, 0x80, 0x80, 0x02, 0x80, 0x80, 0x80, 0x01, 0x80, 0x80, 0x80, 0x00);
	vSInt32 unpackMask1 = v128_set_int8_packed(0x80, 0x80, 0x80, 0x07, 0x80, 0x80, 0x80, 0x06, 0x80, 0x80, 0x80, 0x05, 0x80, 0x80, 0x80, 0x04);
	vSInt32 unpackMask2 = v128_set_int8_packed(0x80, 0x80, 0x80, 0x0B, 0x80, 0x80, 0x80, 0x0A, 0x80, 0x80, 0x80, 0x09, 0x80, 0x80, 0x80, 0x08);

	for( unsigned int x = sx; x < ex; x++ ) {
		int startX = sampleStarts[x];
		uint8_t* outputSample = outputBuffer + (x * outputPitch) + sy;
		const uint8_t* sample = inputBuffer + (sy * inputPitch) + startX;
		uint8_t* outputSampleEnd = outputSample + (ey - sy);
		do {

			// Load and expand (8bit -> 32bit)
			// 4 distinct rows of 16 values each (only 12 used, since this is a 12-wide filter).

			//    0         1         2         3
			// G G G G | G G G G | G G G G | X X X X    (row 0123) dot (coeffs 0-12) = 1st output sample
			//    4         5         6         7
			// G G G G | G G G G | G G G G | X X X X    (row 4567) dot (coeffs 0-12) = 2nd output sample
			//    8         9         A         B
			// G G G G | G G G G | G G G G | X X X X    (row 89AB) dot (coeffs 0-12) = 3rd output sample
			//    C         D         E         F
			// G G G G | G G G G | G G G G | X X X X    (row CDEF) dot (coeffs 0-12) = 4th output sample

			vUInt8 row_0123 = v128_load_unaligned((const vSInt32*)(sample + inputPitch * 0));
			vSInt32 row_0 = v128_shuffle_int8(row_0123, unpackMask0);
			vSInt32 row_1 = v128_shuffle_int8(row_0123, unpackMask1);
			vSInt32 row_2 = v128_shuffle_int8(row_0123, unpackMask2);

			vUInt8 row_4567 = v128_load_unaligned((const vSInt32*)(sample + inputPitch * 1));
			vSInt32 row_4 = v128_shuffle_int8(row_4567, unpackMask0);
			vSInt32 row_5 = v128_shuffle_int8(row_4567, unpackMask1);
			vSInt32 row_6 = v128_shuffle_int8(row_4567, unpackMask2);

			vUInt8 row_89AB = v128_load_unaligned((const vSInt32*)(sample + inputPitch * 2));
			vSInt32 row_8 = v128_shuffle_int8(row_89AB, unpackMask0);
			vSInt32 row_9 = v128_shuffle_int8(row_89AB, unpackMask1);
			vSInt32 row_A = v128_shuffle_int8(row_89AB, unpackMask2);

			vUInt8 row_CDEF = v128_load_unaligned((const vSInt32*)(sample + inputPitch * 3));
			vSInt32 row_C = v128_shuffle_int8(row_CDEF, unpackMask0);
			vSInt32 row_D = v128_shuffle_int8(row_CDEF, unpackMask1);
			vSInt32 row_E = v128_shuffle_int8(row_CDEF, unpackMask2);

			vSInt32 result;

			// Transpose the rows so each multiply/add is operating on 4 of the same element, 1 from each row
			// This is more efficient than treating each row seperately and needing to do horizontal addition within the vectors.
			// The coefficients are replicated 4 times in each vector, so we have 12 vectors for 12 coefficients.
			//  i.e.
			//  (A0 A1 A2 A3) dot (X0 X1 X2 X3) = (Ar 0 0 0)
			//  (B0 B1 B2 B3) dot (X0 X1 X2 X3) = (Br 0 0 0)
			//  (C0 C1 C2 C3) dot (X0 X1 X2 X3) = (Cr 0 0 0)
			//  (D0 D1 D2 D3) dot (X0 X1 X2 X3) = (Dr 0 0 0)
			//   becomes:
			//  (A0 B0 C0 D0) * (X0 X0 X0 X0) +
			//  (A1 B1 C1 D1) * (X1 X1 X1 X1) +
			//  (A2 B2 C2 D2) * (X2 X2 X2 X2) +
			//  (A3 B3 C3 D3) * (X3 X3 X3 X3) = (Ar Br Cr Dr)

			// Fixed point fun. 8.0 * 16.16. Since the coefficients are normalized, worst case is 255*(Coeffs0+Coeffs1+...CoeffN=65536), so we won't overflow. Avoid shifting back until later.
			vec_transpose_epi32(row_0, row_4, row_8, row_C);
			result = v128_mul_int32(row_0, kernelTable[0]);
			result = v128_add_int32(result, v128_mul_int32(row_4, kernelTable[1]));
			result = v128_add_int32(result, v128_mul_int32(row_8, kernelTable[2]));
			result = v128_add_int32(result, v128_mul_int32(row_C, kernelTable[3]));

			vec_transpose_epi32(row_1, row_5, row_9, row_D);
			result = v128_add_int32(result, v128_mul_int32(row_1, kernelTable[4]));
			result = v128_add_int32(result, v128_mul_int32(row_5, kernelTable[5]));
			result = v128_add_int32(result, v128_mul_int32(row_9, kernelTable[6]));
			result = v128_add_int32(result, v128_mul_int32(row_D, kernelTable[7]));

			vec_transpose_epi32(row_2, row_6, row_A, row_E);
			result = v128_add_int32(result, v128_mul_int32(row_2, kernelTable[8]));
			result = v128_add_int32(result, v128_mul_int32(row_6, kernelTable[9]));
			result = v128_add_int32(result, v128_mul_int32(row_A, kernelTable[10]));
			result = v128_add_int32(result, v128_mul_int32(row_E, kernelTable[11]));

			result = v128_add_int32(result, half);
			// Shift back because of the multiplication we did.
			result = v128_shift_right_signed_int32<16>(result);

			// Pack back down (saturating), and store.
			vSInt16 packed_16 = v128_pack_unsigned_saturate_int32(result, zero);
			vSInt8 packed_8 = v128_pack_unsigned_saturate_int16(packed_16, zero, zero);
			*(int*)(outputSample) = v128_extract_int32<0>(packed_8);

			outputSample += 4;
			sample += inputPitch * 4;
		} while (outputSample < outputSampleEnd);
		kernelTable += 12;
	}
}

// single row at a time
void adaptiveSeperable8_12x1(const int32_t* sampleStarts, const FilterKernelAdaptive* kernel, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int input_height, unsigned int inputPitch, uint8_t* __restrict outputBuffer, int32_t sx, int32_t ex, int32_t sy, int32_t ey, unsigned int outputPitch, unsigned int outputCapacity)
{
	// The seperable version writes transposed images.
	SECURE_ASSERT(SafeUMul(ey - sy, 1U) <= outputPitch);
	SECURE_ASSERT(SafeUMul(ex - sx, outputPitch) <= outputCapacity);

	__restrict const vSInt32* kernelTable = (vSInt32*)kernel->getTableFixedPoint();
	kernelTable += (sx * 3);

	__m128i half = v128_set_int32(kHalf16);
	__m128i zero = v128_setzero();
	vSInt32 unpackMask0 = v128_set_int8_packed(0x80, 0x80, 0x80, 0x03, 0x80, 0x80, 0x80, 0x02, 0x80, 0x80, 0x80, 0x01, 0x80, 0x80, 0x80, 0x00);
	vSInt32 unpackMask1 = v128_set_int8_packed(0x80, 0x80, 0x80, 0x07, 0x80, 0x80, 0x80, 0x06, 0x80, 0x80, 0x80, 0x05, 0x80, 0x80, 0x80, 0x04);
	vSInt32 unpackMask2 = v128_set_int8_packed(0x80, 0x80, 0x80, 0x0B, 0x80, 0x80, 0x80, 0x0A, 0x80, 0x80, 0x80, 0x09, 0x80, 0x80, 0x80, 0x08);

	for( unsigned int x = sx; x < ex; x++ ) {
		int startX = sampleStarts[x];
		uint8_t* outputSample = outputBuffer + (x * outputPitch) + sy;
		const uint8_t* sample = inputBuffer + (sy * inputPitch) + startX;
		uint8_t* outputSampleEnd = outputSample + (ey - sy);
		do {
			vUInt8 samples0_15 = v128_load_unaligned((const vSInt32*)(sample));
			vSInt32 samples0_3 = v128_shuffle_int8(samples0_15, unpackMask0);
			vSInt32 samples4_7 = v128_shuffle_int8(samples0_15, unpackMask1);
			vSInt32 samples8_11 = v128_shuffle_int8(samples0_15, unpackMask2);

			vSInt32 sum = v128_mul_int32(samples0_3, kernelTable[0]);
			sum = v128_add_int32(sum, v128_mul_int32(samples4_7, kernelTable[1]));
			sum = v128_add_int32(sum, v128_mul_int32(samples8_11, kernelTable[2]));
			sum = v128_add_int32(sum, v128_shift_right_unsigned_vec128<8>(sum)); // m128[0] + m128[1], m128[2] + m128[3], ....
			sum = v128_add_int32(sum, v128_shift_right_unsigned_vec128<4>(sum)); // m128[0] + m128[1] + m128[2] + m128[3], ....
			sum = v128_add_int32(sum, half);
			sum = v128_shift_right_signed_int32<16>(sum);
			vSInt8 packed_8 = v128_pack_unsigned_saturate_int16(sum, zero, zero); // for saturation
			*outputSample  = v128_extract_int8<0>(packed_8);
			outputSample += 1;
			sample += inputPitch;
		} while (outputSample < outputSampleEnd);
		kernelTable += 3;
	}
}

#define LOAD_ROW(row, index0, index1, index2, index3) row_##row = v128_set_int8_packed(0, 0, 0, *samples8[index3], \
0, 0, 0, *samples8[index2], \
0, 0, 0, *samples8[index1], \
0, 0, 0, *samples8[index0]); \
samples8[index3] += inputPitch; \
samples8[index2] += inputPitch; \
samples8[index1] += inputPitch; \
samples8[index0] += inputPitch;

#define CONVOLVE(index0, index1, index2, index3) LOAD_ROW(0, index0, index1, index2, index3) \
LOAD_ROW(1, index0, index1, index2, index3) \
LOAD_ROW(2, index0, index1, index2, index3) \
LOAD_ROW(3, index0, index1, index2, index3) \
vec_transpose_epi32(row_0, row_1, row_2, row_3); \
result = v128_add_int32(result, v128_mul_int32(row_0, kernelTable[index0])); \
result = v128_add_int32(result, v128_mul_int32(row_1, kernelTable[index1])); \
result = v128_add_int32(result, v128_mul_int32(row_2, kernelTable[index2])); \
result = v128_add_int32(result, v128_mul_int32(row_3, kernelTable[index3]));

void adaptiveSeperableUnpadded8_12x4(const int32_t* sampleStarts, const FilterKernelAdaptive* kernel, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int input_height, unsigned int inputPitch, uint8_t* __restrict outputBuffer, int32_t sx, int32_t ex, int32_t sy, int32_t ey, unsigned int outputPitch, unsigned int outputCapacity)
{
	// The seperable version writes transposed images.
	SECURE_ASSERT(SafeUMul(ey - sy, 1U) <= outputPitch);
	SECURE_ASSERT(SafeUMul(ex - sx, outputPitch) <= outputCapacity);

	__restrict const vSInt32* kernelTable = (vSInt32*)kernel->getTableFixedPoint4();
	kernelTable += (sx * 12);
	vSInt32 half = v128_set_int32(kHalf16);
	vSInt32 zero = v128_setzero();

	for( unsigned int x = sx; x < ex; x++ ) {
		int startX = sampleStarts[x];
		uint8_t* outputSample = outputBuffer + (x * outputPitch) + sy;
		const uint8_t* samples8[12];
		int32_t startIndex = startX < 0 ? 0 : startX;
		const uint8_t* sample = inputBuffer + (sy * inputPitch) + startIndex;
		IMAGECORE_UNUSED(samples8);
		IMAGECORE_UNUSED(kernelTable);
		for( int kernelIndex = 0; kernelIndex < 12; kernelIndex++ ) {
			int sampleIndex;
			if(startX < 0) {
				sampleIndex = kernelIndex + startX < 0 ? 0 : kernelIndex + startX;
			} else {
				sampleIndex = kernelIndex + startIndex < inputWidth ? kernelIndex : kernelIndex - (kernelIndex + startIndex - inputWidth + 1);
			}
			sampleIndex = min(sampleIndex, (int32_t)inputWidth - 1);
			samples8[kernelIndex] = &sample[sampleIndex];
		}
		uint8_t* outputSampleEnd = outputSample + (ey - sy);
		do {
			vSInt32 row_0;
			vSInt32 row_1;
			vSInt32 row_2;
			vSInt32 row_3;
			vSInt32 result = half;
			CONVOLVE(0, 1, 2, 3)
			CONVOLVE(4, 5, 6, 7)
			CONVOLVE(8, 9, 10, 11)

			// Shift back because of the multiplication we did.
			result = v128_shift_right_signed_int32<16>(result);

			// Pack back down (saturating), and store.
			vSInt16 packed_16 = v128_pack_unsigned_saturate_int32(result, zero);
			vSInt8 packed_8 = v128_pack_unsigned_saturate_int16(packed_16, zero, zero);
			*(int*)(outputSample) = v128_extract_int32<0>(packed_8);
			outputSample += 4;
		} while( outputSample < outputSampleEnd );
		kernelTable += 12;
	}
}

void adaptiveSeperableUnpadded8_12x1(const int32_t* sampleStarts, const FilterKernelAdaptive* kernel, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int input_height, unsigned int inputPitch, uint8_t* __restrict outputBuffer, int32_t sx, int32_t ex, int32_t sy, int32_t ey, unsigned int outputPitch, unsigned int outputCapacity)
{
	// The seperable version writes transposed images.
	SECURE_ASSERT(SafeUMul(ey - sy, 1U) <= outputPitch);
	SECURE_ASSERT(SafeUMul(ex - sx, outputPitch) <= outputCapacity);

	__restrict const vSInt32* kernelTable = (vSInt32*)kernel->getTableFixedPoint();
	kernelTable += (sx * 3);
	vSInt32 half = v128_set_int32(kHalf16);
	vSInt32 zero = v128_setzero();

	for( unsigned int x = sx; x < ex; x++ ) {
		int startX = sampleStarts[x];
		uint8_t* outputSample = outputBuffer + (x * outputPitch) + sy;
		const uint8_t* samples8[12];
		int32_t startIndex = startX < 0 ? 0 : startX;
		const uint8_t* sample = inputBuffer + (sy * inputPitch) + startIndex;
		IMAGECORE_UNUSED(samples8);
		IMAGECORE_UNUSED(kernelTable);
		for( int kernelIndex = 0; kernelIndex < 12; kernelIndex++ ) {
			int sampleIndex;
			if(startX < 0) {
				sampleIndex = kernelIndex + startX < 0 ? 0 : kernelIndex + startX;
			} else {
				sampleIndex = kernelIndex + startIndex < inputWidth ? kernelIndex : kernelIndex - (kernelIndex + startIndex - inputWidth + 1);
			}
			sampleIndex = min(sampleIndex, (int32_t)inputWidth - 1);
			samples8[kernelIndex] = &sample[sampleIndex];
		}
		uint8_t* outputSampleEnd = outputSample + (ey - sy);
		do {
			vSInt32 samples0_3;
			vSInt32 samples4_7;
			vSInt32 samples8_11;
			samples0_3 = v128_set_int8_packed(0, 0, 0, *samples8[3],
									0, 0, 0, *samples8[2],
									0, 0, 0, *samples8[1],
									0, 0, 0, *samples8[0]);
			samples4_7 = v128_set_int8_packed(0, 0, 0, *samples8[7],
									0, 0, 0, *samples8[6],
									0, 0, 0, *samples8[5],
									0, 0, 0, *samples8[4]);
			samples8_11 = v128_set_int8_packed(0, 0, 0, *samples8[11],
									0, 0, 0, *samples8[10],
									0, 0, 0, *samples8[9],
									0, 0, 0, *samples8[8]);
			vSInt32 sum = v128_mul_int32(samples0_3, kernelTable[0]);
			sum = v128_add_int32(sum, v128_mul_int32(samples4_7, kernelTable[1]));
			sum = v128_add_int32(sum, v128_mul_int32(samples8_11, kernelTable[2]));
			sum = v128_add_int32(sum, v128_shift_right_unsigned_vec128<8>(sum));
			sum = v128_add_int32(sum, v128_shift_right_unsigned_vec128<4>(sum));
			sum = v128_add_int32(sum, half);
			sum = v128_shift_right_signed_int32<16>(sum);
			vSInt8 packed_8 = v128_pack_unsigned_saturate_int16(sum, zero, zero);
			*outputSample  = v128_extract_int8<0>(packed_8);
			outputSample += 1;
			for( int kernelIndex = 0; kernelIndex < 12; kernelIndex++ ) {
				samples8[kernelIndex] += inputPitch;
			}
		} while( outputSample < outputSampleEnd );
		kernelTable += 3;
	}
}

// uses unpadded code version around the edges of the image and the faster padded version for the internal part
void adaptiveSeperableHybrid8_12(const FilterKernelAdaptive* kernel, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity)
{
	int32_t unpaddedStartX = 0;
	int32_t unpaddedEndX = 0;
	int32_t unpaddedEndY = (outputHeight & (~3)); // optimized 4 lines at a time version
	int32_t* startX = new int32_t[outputWidth];
	// pre-calculate the start sampling points per kernel, keep track of the unpadded regions limits
	for(int32_t x = 0; x < outputWidth; x++) {
		startX[x] = kernel->computeSampleStart(x);
		if((startX[x] >= 0) && (unpaddedStartX == 0)) {
			unpaddedStartX = x;
		}
		int32_t endX = startX[x] + 12;
		if(endX < inputWidth) {
			unpaddedEndX = x;
		}
	}
	// 6 passes:
	//   1 - left edge 4 rows at a time (unpadded)
	//   2 - left edge remainder of the rows (unpadded)
	//   3 - middle section 4 rows at a time (padded)
	//   4 - middle section remainder of the rows (padded)
	//   5 - right edge 4 rows at a time (unpadded)
	//   6 - right edge remainder of the rows (unpadded)

	// left edge
	if(unpaddedStartX > 0) {
		adaptiveSeperableUnpadded8_12x4(startX, kernel, inputBuffer, inputWidth, inputHeight, inputPitch, outputBuffer, 0, unpaddedStartX, 0, unpaddedEndY, outputPitch, outputCapacity);
		if( unpaddedEndY != outputHeight) { // leftover lines, one at a time
			adaptiveSeperableUnpadded8_12x1(startX, kernel, inputBuffer, inputWidth, inputHeight, inputPitch, outputBuffer, 0, unpaddedStartX, unpaddedEndY, outputHeight, outputPitch, outputCapacity);
		}
	}
	// middle section, doesn't need unpadded code
	if(unpaddedEndX - unpaddedStartX > 0) {
		adaptiveSeperable8_12x4(startX, kernel, inputBuffer, inputWidth, inputHeight, inputPitch, outputBuffer, unpaddedStartX, unpaddedEndX, 0, unpaddedEndY, outputPitch, outputCapacity);
		if( unpaddedEndY != outputHeight) { // leftover lines, one at a time
			adaptiveSeperable8_12x1(startX, kernel, inputBuffer, inputWidth, inputHeight, inputPitch, outputBuffer, unpaddedStartX, unpaddedEndX, unpaddedEndY, outputHeight, outputPitch, outputCapacity);
		}
	}
	// right edge
	if(outputWidth - unpaddedEndX > 0) {
		adaptiveSeperableUnpadded8_12x4(startX, kernel, inputBuffer, inputWidth, inputHeight, inputPitch, outputBuffer, unpaddedEndX, outputWidth, 0, unpaddedEndY, outputPitch, outputCapacity);
		if( unpaddedEndY != outputHeight) { // leftover lines, one at a time
			adaptiveSeperableUnpadded8_12x1(startX, kernel, inputBuffer, inputWidth, inputHeight, inputPitch, outputBuffer, unpaddedEndX, outputWidth, unpaddedEndY, outputHeight, outputPitch, outputCapacity);
		}
	}
	delete[] startX;
}

void adaptiveSeperable8_12(const FilterKernelAdaptive* kernel, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int input_height, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity)
{
	// The seperable version writes transposed images.
	SECURE_ASSERT(SafeUMul(outputHeight, 1U) <= outputPitch);
	SECURE_ASSERT(SafeUMul(outputWidth, outputPitch) <= outputCapacity);

	__restrict const vSInt32* kernelTable = (vSInt32*)kernel->getTableFixedPoint4();

	__m128i half = v128_set_int32(kHalf16);
	__m128i zero = v128_setzero();
	vSInt32 unpackMask0 = v128_set_int8_packed(0x80, 0x80, 0x80, 0x03, 0x80, 0x80, 0x80, 0x02, 0x80, 0x80, 0x80, 0x01, 0x80, 0x80, 0x80, 0x00);
	vSInt32 unpackMask1 = v128_set_int8_packed(0x80, 0x80, 0x80, 0x07, 0x80, 0x80, 0x80, 0x06, 0x80, 0x80, 0x80, 0x05, 0x80, 0x80, 0x80, 0x04);
	vSInt32 unpackMask2 = v128_set_int8_packed(0x80, 0x80, 0x80, 0x0B, 0x80, 0x80, 0x80, 0x0A, 0x80, 0x80, 0x80, 0x09, 0x80, 0x80, 0x80, 0x08);


	for( unsigned int x = 0; x < outputWidth; x++ ) {
		int startX = kernel->computeSampleStart(x);
		uint8_t* outputSample = outputBuffer + (x * outputPitch);
		const uint8_t* sample = inputBuffer + startX;
		uint8_t* outputSampleEnd = outputSample + outputHeight;
		do {

			// Load and expand (8bit -> 32bit)
			// 4 distinct rows of 16 values each (only 12 used, since this is a 12-wide filter).

			//    0         1         2         3
			// G G G G | G G G G | G G G G | X X X X    (row 0123) dot (coeffs 0-12) = 1st output sample
			//    4         5         6         7
			// G G G G | G G G G | G G G G | X X X X    (row 4567) dot (coeffs 0-12) = 2nd output sample
			//    8         9         A         B
			// G G G G | G G G G | G G G G | X X X X    (row 89AB) dot (coeffs 0-12) = 3rd output sample
			//    C         D         E         F
			// G G G G | G G G G | G G G G | X X X X    (row CDEF) dot (coeffs 0-12) = 4th output sample

			vUInt8 row_0123 = v128_load_unaligned((const vSInt32*)(sample + inputPitch * 0));
			vSInt32 row_0 = v128_shuffle_int8(row_0123, unpackMask0);
			vSInt32 row_1 = v128_shuffle_int8(row_0123, unpackMask1);
			vSInt32 row_2 = v128_shuffle_int8(row_0123, unpackMask2);

			vUInt8 row_4567 = v128_load_unaligned((const vSInt32*)(sample + inputPitch * 1));
			vSInt32 row_4 = v128_shuffle_int8(row_4567, unpackMask0);
			vSInt32 row_5 = v128_shuffle_int8(row_4567, unpackMask1);
			vSInt32 row_6 = v128_shuffle_int8(row_4567, unpackMask2);

			vUInt8 row_89AB = v128_load_unaligned((const vSInt32*)(sample + inputPitch * 2));
			vSInt32 row_8 = v128_shuffle_int8(row_89AB, unpackMask0);
			vSInt32 row_9 = v128_shuffle_int8(row_89AB, unpackMask1);
			vSInt32 row_A = v128_shuffle_int8(row_89AB, unpackMask2);

			vUInt8 row_CDEF = v128_load_unaligned((const vSInt32*)(sample + inputPitch * 3));
			vSInt32 row_C = v128_shuffle_int8(row_CDEF, unpackMask0);
			vSInt32 row_D = v128_shuffle_int8(row_CDEF, unpackMask1);
			vSInt32 row_E = v128_shuffle_int8(row_CDEF, unpackMask2);

			vSInt32 result;

			// Transpose the rows so each multiply/add is operating on 4 of the same element, 1 from each row
			// This is more efficient than treating each row seperately and needing to do horizontal addition within the vectors.
			// The coefficients are replicated 4 times in each vector, so we have 12 vectors for 12 coefficients.
			//  i.e.
			//  (A0 A1 A2 A3) dot (X0 X1 X2 X3) = (Ar 0 0 0)
			//  (B0 B1 B2 B3) dot (X0 X1 X2 X3) = (Br 0 0 0)
			//  (C0 C1 C2 C3) dot (X0 X1 X2 X3) = (Cr 0 0 0)
			//  (D0 D1 D2 D3) dot (X0 X1 X2 X3) = (Dr 0 0 0)
			//   becomes:
			//  (A0 B0 C0 D0) * (X0 X0 X0 X0) +
			//  (A1 B1 C1 D1) * (X1 X1 X1 X1) +
			//  (A2 B2 C2 D2) * (X2 X2 X2 X2) +
			//  (A3 B3 C3 D3) * (X3 X3 X3 X3) = (Ar Br Cr Dr)

			// Fixed point fun. 8.0 * 16.16. Since the coefficients are normalized, worst case is 255*(Coeffs0+Coeffs1+...CoeffN=65536), so we won't overflow. Avoid shifting back until later.
			vec_transpose_epi32(row_0, row_4, row_8, row_C);
			result = v128_mul_int32(row_0, kernelTable[0]);
			result = v128_add_int32(result, v128_mul_int32(row_4, kernelTable[1]));
			result = v128_add_int32(result, v128_mul_int32(row_8, kernelTable[2]));
			result = v128_add_int32(result, v128_mul_int32(row_C, kernelTable[3]));

			vec_transpose_epi32(row_1, row_5, row_9, row_D);
			result = v128_add_int32(result, v128_mul_int32(row_1, kernelTable[4]));
			result = v128_add_int32(result, v128_mul_int32(row_5, kernelTable[5]));
			result = v128_add_int32(result, v128_mul_int32(row_9, kernelTable[6]));
			result = v128_add_int32(result, v128_mul_int32(row_D, kernelTable[7]));

			vec_transpose_epi32(row_2, row_6, row_A, row_E);
			result = v128_add_int32(result, v128_mul_int32(row_2, kernelTable[8]));
			result = v128_add_int32(result, v128_mul_int32(row_6, kernelTable[9]));
			result = v128_add_int32(result, v128_mul_int32(row_A, kernelTable[10]));
			result = v128_add_int32(result, v128_mul_int32(row_E, kernelTable[11]));

			result = v128_add_int32(result, half);
			// Shift back because of the multiplication we did.
			result = v128_shift_right_signed_int32<16>(result);

			// Pack back down (saturating), and store.
			vSInt16 packed_16 = v128_pack_unsigned_saturate_int32(result, zero);
			vSInt8 packed_8 = v128_pack_unsigned_saturate_int16(packed_16, zero, zero);
			*(int*)(outputSample) = v128_extract_int32<0>(packed_8);

			outputSample += 4;
			sample += inputPitch * 4;
		} while (outputSample < outputSampleEnd);
		kernelTable += 12;
	}
}

template<> void Filters<ComponentSIMD<1>>::adaptiveSeperable(const FilterKernelAdaptive* kernel, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity, bool unpadded)
{
#if IMAGECORE_DETECT_SSE
    if( !checkForCPUSupport(kCPUFeature_SSE4_1)) {
        return Filters<ComponentScalar<1>>::adaptiveSeperable(kernel, inputBuffer, inputWidth, inputHeight, inputPitch, outputBuffer, outputWidth, outputHeight, outputPitch, outputCapacity, unpadded);
    }
#endif
	unsigned int kernelSize = kernel->getKernelSize();
	if( kernelSize == 8U ) {
		// TODO
		SECURE_ASSERT(0);
	} else if( kernelSize == 12U ) {
		if(unpadded) {
			adaptiveSeperableHybrid8_12(kernel, inputBuffer, inputWidth, inputHeight, inputPitch, outputBuffer, outputWidth, outputHeight, outputPitch, outputCapacity);
		} else {
			adaptiveSeperable8_12(kernel, inputBuffer, inputWidth, inputHeight, inputPitch, outputBuffer, outputWidth, outputHeight, outputPitch, outputCapacity);
		}
	} else {
		SECURE_ASSERT(0);
	}
}

// 4 sample fixed filter
// 16.16 Fixed point SSE version
template<>
void Filters<ComponentSIMD<4>>::fixed4x4(const FilterKernelFixed *kernelX, const FilterKernelFixed *kernelY, const uint8_t *inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t *outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity)
{
#if IMAGECORE_DETECT_SSE
    if( !checkForCPUSupport(kCPUFeature_SSE4_1)) {
        return Filters<ComponentScalar<4>>::fixed4x4(kernelX, kernelY, inputBuffer, inputWidth, inputHeight, inputPitch, outputBuffer, outputWidth, outputHeight, outputPitch, outputCapacity);
    }
#endif
	SECURE_ASSERT(SafeUMul(outputWidth, 4U) <= outputPitch);
	SECURE_ASSERT(SafeUMul(outputHeight, outputPitch) <= outputCapacity);

	__restrict int32_t* kernelTableX = kernelX->getTableFixedPoint4();
	__restrict int32_t* kernelTableY = kernelY->getTableFixedPoint4();

	__m128i zero = v128_setzero();
	__m128i half = v128_set_int32(kHalf22);

	for( int y = 0; y < outputHeight; y++ ) {
		int sampleY = kernelY->computeSampleStart(y);
		for( int x = 0; x < outputWidth; x++ ) {
			int sampleX = kernelX->computeSampleStart(x);

			int sampleOffset = ((sampleY - 1) * (int)inputPitch) + (sampleX - 1) * 4;
			const uint8_t* sample = inputBuffer + sampleOffset;

			vSInt32 final = zero;

			unsigned int filterIndexX = x;
			filterIndexX *= 16;
			vSInt32 coeffs_x_0 = *(vSInt32*)(kernelTableX + filterIndexX + 0);
			vSInt32 coeffs_x_1 = *(vSInt32*)(kernelTableX + filterIndexX + 4);
			vSInt32 coeffs_x_2 = *(vSInt32*)(kernelTableX + filterIndexX + 8);
			vSInt32 coeffs_x_3 = *(vSInt32*)(kernelTableX + filterIndexX + 12);

			unsigned int filterIndexY = y;
			filterIndexY *= 16;

			vSInt32 coeffs_y_0 = *(vSInt32*)(kernelTableY + filterIndexY + 0);
			{
				vUInt8 row_8 = v128_load_unaligned((const vSInt32*)sample);
				vUInt16 row_16_a = v128_unpacklo_int8(row_8, zero);
				vUInt16 row_16_b = v128_unpackhi_int8(row_8, zero);
				vSInt32 row_32_a = v128_unpacklo_int16(row_16_a, zero);
				vSInt32 row_32_b = v128_unpackhi_int16(row_16_a, zero);
				vSInt32 row_32_c = v128_unpacklo_int16(row_16_b, zero);
				vSInt32 row_32_d = v128_unpackhi_int16(row_16_b, zero);
				vSInt32 mul_a = v128_mul_int32(row_32_a, coeffs_x_0);
				vSInt32 mul_b = v128_mul_int32(row_32_b, coeffs_x_1);
				vSInt32 mul_c = v128_mul_int32(row_32_c, coeffs_x_2);
				vSInt32 mul_d = v128_mul_int32(row_32_d, coeffs_x_3);
				vSInt32 row = v128_shift_right_signed_int32<10>(v128_add_int32(mul_a, v128_add_int32(mul_b, v128_add_int32(mul_c, mul_d))));
				final = v128_add_int32(final, v128_mul_int32(row, coeffs_y_0));
				sample += inputPitch;
			}

			vSInt32 coeffs_y_1 = *(vSInt32*)(kernelTableY + filterIndexY + 4);
			{
				vUInt8 row_8 = v128_load_unaligned((const vSInt32*)sample);
				vUInt16 row_16_a = v128_unpacklo_int8(row_8, zero);
				vUInt16 row_16_b = v128_unpackhi_int8(row_8, zero);
				vSInt32 row_32_a = v128_unpacklo_int16(row_16_a, zero);
				vSInt32 row_32_b = v128_unpackhi_int16(row_16_a, zero);
				vSInt32 row_32_c = v128_unpacklo_int16(row_16_b, zero);
				vSInt32 row_32_d = v128_unpackhi_int16(row_16_b, zero);
				vSInt32 mul_a = v128_mul_int32(row_32_a, coeffs_x_0);
				vSInt32 mul_b = v128_mul_int32(row_32_b, coeffs_x_1);
				vSInt32 mul_c = v128_mul_int32(row_32_c, coeffs_x_2);
				vSInt32 mul_d = v128_mul_int32(row_32_d, coeffs_x_3);
				vSInt32 row = v128_shift_right_signed_int32<10>(v128_add_int32(mul_a, v128_add_int32(mul_b, v128_add_int32(mul_c, mul_d))));
				final = v128_add_int32(final, v128_mul_int32(row, coeffs_y_1));
				sample += inputPitch;
			}

			vSInt32 coeffs_y_2 = *(vSInt32*)(kernelTableY + filterIndexY + 8);
			{
				vUInt8 row_8 = v128_load_unaligned((const vSInt32*)sample);
				vUInt16 row_16_a = v128_unpacklo_int8(row_8, zero);
				vUInt16 row_16_b = v128_unpackhi_int8(row_8, zero);
				vSInt32 row_32_a = v128_unpacklo_int16(row_16_a, zero);
				vSInt32 row_32_b = v128_unpackhi_int16(row_16_a, zero);
				vSInt32 row_32_c = v128_unpacklo_int16(row_16_b, zero);
				vSInt32 row_32_d = v128_unpackhi_int16(row_16_b, zero);
				vSInt32 mul_a = v128_mul_int32(row_32_a, coeffs_x_0);
				vSInt32 mul_b = v128_mul_int32(row_32_b, coeffs_x_1);
				vSInt32 mul_c = v128_mul_int32(row_32_c, coeffs_x_2);
				vSInt32 mul_d = v128_mul_int32(row_32_d, coeffs_x_3);
				vSInt32 row = v128_shift_right_signed_int32<10>(v128_add_int32(mul_a, v128_add_int32(mul_b, v128_add_int32(mul_c, mul_d))));
				final = v128_add_int32(final, v128_mul_int32(row, coeffs_y_2));
				sample += inputPitch;
			}

			vSInt32 coeffs_y_3 = *(vSInt32*)(kernelTableY + filterIndexY + 12);
			{
				vUInt8 row_8 = v128_load_unaligned((const vSInt32*)sample);
				vUInt16 row_16_a = v128_unpacklo_int8(row_8, zero);
				vUInt16 row_16_b = v128_unpackhi_int8(row_8, zero);
				vSInt32 row_32_a = v128_unpacklo_int16(row_16_a, zero);
				vSInt32 row_32_b = v128_unpackhi_int16(row_16_a, zero);
				vSInt32 row_32_c = v128_unpacklo_int16(row_16_b, zero);
				vSInt32 row_32_d = v128_unpackhi_int16(row_16_b, zero);
				vSInt32 mul_a = v128_mul_int32(row_32_a, coeffs_x_0);
				vSInt32 mul_b = v128_mul_int32(row_32_b, coeffs_x_1);
				vSInt32 mul_c = v128_mul_int32(row_32_c, coeffs_x_2);
				vSInt32 mul_d = v128_mul_int32(row_32_d, coeffs_x_3);
				vSInt32 row = v128_shift_right_signed_int32<10>(v128_add_int32(mul_a, v128_add_int32(mul_b, v128_add_int32(mul_c, mul_d))));
				final = v128_add_int32(final, v128_mul_int32(row, coeffs_y_3));
			}

			final = v128_add_int32(final, half);
			final = v128_shift_right_signed_int32<22>(final);
			vSInt16 packed_16 = v128_pack_unsigned_saturate_int32(final, zero);
			vSInt8 packed_8 = v128_pack_unsigned_saturate_int16(packed_16, zero, zero);
			unsigned int oi = (y * outputPitch) + x * 4;
			int a = v128_convert_to_int32(packed_8);
			*(int*)(outputBuffer + oi) = a;
		}
	}
}

template<>
bool Filters<ComponentSIMD<4>>::fasterUnpadded(uint32_t kernelSize)
{
	return (kernelSize == 12);
}

template<>
bool Filters<ComponentSIMD<1>>::fasterUnpadded(uint32_t kernelSize)
{
	return false;
}

template<>
bool Filters<ComponentSIMD<4>>::supportsUnpadded(uint32_t kernelSize)
{
	return true;
}

template<>
bool Filters<ComponentSIMD<1>>::supportsUnpadded(uint32_t kernelSize)
{
	return true;
}

// forward template declarations
template class Filters<ComponentScalar<1>>;
template class Filters<ComponentScalar<2>>;
template class Filters<ComponentScalar<4>>;
template class Filters<ComponentSIMD<1>>;
template class Filters<ComponentSIMD<2>>;
template class Filters<ComponentSIMD<4>>;

}

#endif
