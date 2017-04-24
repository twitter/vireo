//
//  neon.cpp
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

#if __ARM_NEON__

#include "intrinsics.h"

namespace imagecore {

void adaptive4x4_3(const FilterKernelAdaptive* kernelX, const FilterKernelAdaptive* kernelY, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity)
{
	SECURE_ASSERT(SafeUMul(outputWidth, 4U) <= outputPitch);
	SECURE_ASSERT(SafeUMul(outputHeight, outputPitch) <= outputCapacity);

	int32x4_t half = vdupq_n_s32(kHalf22);

	__restrict int32_t* kernelTableX = kernelX->getTableFixedPoint4();
	__restrict int32_t* kernelTableY = kernelY->getTableFixedPoint4();

	for( unsigned int y = 0; y < outputHeight; y++ ) {
		int startY = kernelY->computeSampleStart(y);
		for( unsigned int x = 0; x < outputWidth; x++ ) {
			int startX = kernelX->computeSampleStart(x);

			int sampleOffset = ((startY) * (int)inputPitch) + (startX) * 4;
			const uint8_t* sample = inputBuffer + sampleOffset;

			int32x4_t final;

			unsigned int filterIndexX = x * 16;
			unsigned int filter_index_y = y * 16;

			int32x4_t coeffs_x_0 = *(int32x4_t*)(kernelTableX + filterIndexX + 0);
			int32x4_t coeffs_x_1 = *(int32x4_t*)(kernelTableX + filterIndexX + 4);
			int32x4_t coeffs_x_2 = *(int32x4_t*)(kernelTableX + filterIndexX + 8);
			int32x4_t coeffs_y_0 = *(int32x4_t*)(kernelTableY + filter_index_y + 0);
			{
				uint8x16_t row_8 = vld1q_u8((uint8_t*)sample);
				int16x8_t row_16_a = vmovl_u8(vget_low_u8(row_8));
				int16x8_t row_16_b = vmovl_u8(vget_high_u8(row_8));
				int32x4_t row_32_a = vmovl_s16(vget_low_s16(row_16_a));
				int32x4_t row_32_b = vmovl_s16(vget_high_s16(row_16_a));
				int32x4_t row_32_c = vmovl_s16(vget_low_s16(row_16_b));
				int32x4_t mul_a = vmulq_s32(row_32_a, coeffs_x_0);
				int32x4_t mul_b = vmulq_s32(row_32_b, coeffs_x_1);
				int32x4_t mul_c = vmulq_s32(row_32_c, coeffs_x_2);
				int32x4_t row = vshrq_n_s32(vaddq_s32(mul_a, vaddq_s32(mul_b, mul_c)), 10);
				final = vmulq_s32(row, coeffs_y_0);
				sample += inputPitch;
			}

			int32x4_t coeffs_y_1 = *(int32x4_t*)(kernelTableY + filter_index_y + 4);
			{
				uint8x16_t row_8 = vld1q_u8((uint8_t*)sample);
				int16x8_t row_16_a = vmovl_u8(vget_low_u8(row_8));
				int16x8_t row_16_b = vmovl_u8(vget_high_u8(row_8));
				int32x4_t row_32_a = vmovl_s16(vget_low_s16(row_16_a));
				int32x4_t row_32_b = vmovl_s16(vget_high_s16(row_16_a));
				int32x4_t row_32_c = vmovl_s16(vget_low_s16(row_16_b));
				int32x4_t mul_a = vmulq_s32(row_32_a, coeffs_x_0);
				int32x4_t mul_b = vmulq_s32(row_32_b, coeffs_x_1);
				int32x4_t mul_c = vmulq_s32(row_32_c, coeffs_x_2);
				int32x4_t row = vshrq_n_s32(vaddq_s32(mul_a, vaddq_s32(mul_b, mul_c)), 10);
				final = vaddq_s32(final, vmulq_s32(row, coeffs_y_1));
				sample += inputPitch;
			}

			int32x4_t coeffs_y_2 = *(int32x4_t*)(kernelTableY + filter_index_y + 8);
			{
				uint8x16_t row_8 = vld1q_u8((uint8_t*)sample);
				int16x8_t row_16_a = vmovl_u8(vget_low_u8(row_8));
				int16x8_t row_16_b = vmovl_u8(vget_high_u8(row_8));
				int32x4_t row_32_a = vmovl_s16(vget_low_s16(row_16_a));
				int32x4_t row_32_b = vmovl_s16(vget_high_s16(row_16_a));
				int32x4_t row_32_c = vmovl_s16(vget_low_s16(row_16_b));
				int32x4_t mul_a = vmulq_s32(row_32_a, coeffs_x_0);
				int32x4_t mul_b = vmulq_s32(row_32_b, coeffs_x_1);
				int32x4_t mul_c = vmulq_s32(row_32_c, coeffs_x_2);
				int32x4_t row = vshrq_n_s32(vaddq_s32(mul_a, vaddq_s32(mul_b, mul_c)), 10);
				final = vaddq_s32(final, vmulq_s32(row, coeffs_y_2));
				sample += inputPitch;
			}

			final = vaddq_s32(final, half);
			final = vshrq_n_s32(final, 22);
			int8x8_t packed_8 = vmovn_s16(vcombine_s16(vmovn_s32(final), vdup_n_s16(0)));
			unsigned int oi = (y * outputPitch) + x * 4;
			vst1_lane_s32((int32_t*)(outputBuffer + oi), packed_8, 0);
		}
	}
}

template<>
void Filters<ComponentSIMD<4>>::adaptive4x4(const FilterKernelAdaptive* kernelX, const FilterKernelAdaptive* kernelY, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity)
{
	if (kernelX->getMaxSamples() == 3 && kernelY->getMaxSamples() == 3) {
		adaptive4x4_3(kernelX, kernelY, inputBuffer, inputWidth, inputHeight, inputPitch, outputBuffer, outputWidth, outputHeight, outputPitch, outputCapacity);
		return;
	}
	SECURE_ASSERT(SafeUMul(outputWidth, 4U) <= outputPitch);
	SECURE_ASSERT(SafeUMul(outputHeight, outputPitch) <= outputCapacity);

	int32x4_t half = vdupq_n_s32(kHalf22);

	__restrict int32_t* kernelTableX = kernelX->getTableFixedPoint4();
	__restrict int32_t* kernelTableY = kernelY->getTableFixedPoint4();

	for( unsigned int y = 0; y < outputHeight; y++ ) {
		int startY = kernelY->computeSampleStart(y);
		for( unsigned int x = 0; x < outputWidth; x++ ) {
			int startX = kernelX->computeSampleStart(x);

			int sampleOffset = ((startY) * (int)inputPitch) + (startX) * 4;
			const uint8_t* sample = inputBuffer + sampleOffset;

			int32x4_t final;

			unsigned int filterIndexX = x * 16;
			unsigned int filter_index_y = y * 16;

			int32x4_t coeffs_x_0 = *(int32x4_t*)(kernelTableX + filterIndexX + 0);
			int32x4_t coeffs_x_1 = *(int32x4_t*)(kernelTableX + filterIndexX + 4);
			int32x4_t coeffs_x_2 = *(int32x4_t*)(kernelTableX + filterIndexX + 8);
			int32x4_t coeffs_x_3 = *(int32x4_t*)(kernelTableX + filterIndexX + 12);

			int32x4_t coeffs_y_0 = *(int32x4_t*)(kernelTableY + filter_index_y + 0);
			{
				uint8x16_t row_8 = vld1q_u8((uint8_t*)sample);
				int16x8_t row_16_a = vmovl_u8(vget_low_u8(row_8));
				int16x8_t row_16_b = vmovl_u8(vget_high_u8(row_8));
				int32x4_t row_32_a = vmovl_s16(vget_low_s16(row_16_a));
				int32x4_t row_32_b = vmovl_s16(vget_high_s16(row_16_a));
				int32x4_t row_32_c = vmovl_s16(vget_low_s16(row_16_b));
				int32x4_t row_32_d = vmovl_s16(vget_high_s16(row_16_b));
				int32x4_t mul_a = vmulq_s32(row_32_a, coeffs_x_0);
				int32x4_t mul_b = vmulq_s32(row_32_b, coeffs_x_1);
				int32x4_t mul_c = vmulq_s32(row_32_c, coeffs_x_2);
				int32x4_t mul_d = vmulq_s32(row_32_d, coeffs_x_3);
				int32x4_t row = vshrq_n_s32(vaddq_s32(mul_a, vaddq_s32(mul_b, vaddq_s32(mul_c, mul_d))), 10);
				final = vmulq_s32(row, coeffs_y_0);
				sample += inputPitch;
			}

			int32x4_t coeffs_y_1 = *(int32x4_t*)(kernelTableY + filter_index_y + 4);
			{
				uint8x16_t row_8 = vld1q_u8((uint8_t*)sample);
				int16x8_t row_16_a = vmovl_u8(vget_low_u8(row_8));
				int16x8_t row_16_b = vmovl_u8(vget_high_u8(row_8));
				int32x4_t row_32_a = vmovl_s16(vget_low_s16(row_16_a));
				int32x4_t row_32_b = vmovl_s16(vget_high_s16(row_16_a));
				int32x4_t row_32_c = vmovl_s16(vget_low_s16(row_16_b));
				int32x4_t row_32_d = vmovl_s16(vget_high_s16(row_16_b));
				int32x4_t mul_a = vmulq_s32(row_32_a, coeffs_x_0);
				int32x4_t mul_b = vmulq_s32(row_32_b, coeffs_x_1);
				int32x4_t mul_c = vmulq_s32(row_32_c, coeffs_x_2);
				int32x4_t mul_d = vmulq_s32(row_32_d, coeffs_x_3);
				int32x4_t row = vshrq_n_s32(vaddq_s32(mul_a, vaddq_s32(mul_b, vaddq_s32(mul_c, mul_d))), 10);
				final = vaddq_s32(final, vmulq_s32(row, coeffs_y_1));
				sample += inputPitch;
			}

			int32x4_t coeffs_y_2 = *(int32x4_t*)(kernelTableY + filter_index_y + 8);
			{
				uint8x16_t row_8 = vld1q_u8((uint8_t*)sample);
				int16x8_t row_16_a = vmovl_u8(vget_low_u8(row_8));
				int16x8_t row_16_b = vmovl_u8(vget_high_u8(row_8));
				int32x4_t row_32_a = vmovl_s16(vget_low_s16(row_16_a));
				int32x4_t row_32_b = vmovl_s16(vget_high_s16(row_16_a));
				int32x4_t row_32_c = vmovl_s16(vget_low_s16(row_16_b));
				int32x4_t row_32_d = vmovl_s16(vget_high_s16(row_16_b));
				int32x4_t mul_a = vmulq_s32(row_32_a, coeffs_x_0);
				int32x4_t mul_b = vmulq_s32(row_32_b, coeffs_x_1);
				int32x4_t mul_c = vmulq_s32(row_32_c, coeffs_x_2);
				int32x4_t mul_d = vmulq_s32(row_32_d, coeffs_x_3);
				int32x4_t row = vshrq_n_s32(vaddq_s32(mul_a, vaddq_s32(mul_b, vaddq_s32(mul_c, mul_d))), 10);
				final = vaddq_s32(final, vmulq_s32(row, coeffs_y_2));
				sample += inputPitch;
			}

			int32x4_t coeffs_y_3 = *(int32x4_t*)(kernelTableY + filter_index_y + 12);
			{
				uint8x16_t row_8 = vld1q_u8((uint8_t*)sample);
				int16x8_t row_16_a = vmovl_u8(vget_low_u8(row_8));
				int16x8_t row_16_b = vmovl_u8(vget_high_u8(row_8));
				int32x4_t row_32_a = vmovl_s16(vget_low_s16(row_16_a));
				int32x4_t row_32_b = vmovl_s16(vget_high_s16(row_16_a));
				int32x4_t row_32_c = vmovl_s16(vget_low_s16(row_16_b));
				int32x4_t row_32_d = vmovl_s16(vget_high_s16(row_16_b));
				int32x4_t mul_a = vmulq_s32(row_32_a, coeffs_x_0);
				int32x4_t mul_b = vmulq_s32(row_32_b, coeffs_x_1);
				int32x4_t mul_c = vmulq_s32(row_32_c, coeffs_x_2);
				int32x4_t mul_d = vmulq_s32(row_32_d, coeffs_x_3);
				int32x4_t row = vshrq_n_s32(vaddq_s32(mul_a, vaddq_s32(mul_b, vaddq_s32(mul_c, mul_d))), 10);
				final = vaddq_s32(final, vmulq_s32(row, coeffs_y_3));
				sample += inputPitch;
			}

			final = vaddq_s32(final, half);
			final = vshrq_n_s32(final, 22);
			int8x8_t packed_8 = vmovn_s16(vcombine_s16(vmovn_s32(final), vdup_n_s16(0)));
			unsigned int oi = (y * outputPitch) + x * 4;
			vst1_lane_s32((int32_t*)(outputBuffer + oi), packed_8, 0);
		}
	}
}


static void adaptiveSeperableAny(const FilterKernelAdaptive* kernel, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity)
{
	SECURE_ASSERT(SafeUMul(outputWidth, 4U) <= outputPitch);
	SECURE_ASSERT(SafeUMul(outputHeight, outputPitch) <= outputCapacity);

	__restrict int32_t* kernelTable = kernel->getTableFixedPoint4();
	unsigned int kernel_width = kernel->getKernelSize();

	int32x4_t zero = vdupq_n_s32(0);
	int32x4_t half = vdupq_n_s32(kHalf16);

	for( unsigned int y = 0; y < outputHeight; y++ ) {
		for( unsigned int x = 0; x < outputWidth; x++ ) {
			int startX = kernel->computeSampleStart(x);
			int sampleOffset = (y * (int)inputPitch) + (startX) * 4;
			const uint8_t* sample = inputBuffer + sampleOffset;

			int32x4_t result = zero;

			for( unsigned int section = 0; section < kernel_width; section += 4 ) {
				unsigned int filterIndexX = x * kernel_width * 4 + section * 4;
				int32x4_t coeffs_x_0 = *(int32x4_t*)(kernelTable + filterIndexX + 0);
				int32x4_t coeffs_x_1 = *(int32x4_t*)(kernelTable + filterIndexX + 4);
				int32x4_t coeffs_x_2 = *(int32x4_t*)(kernelTable + filterIndexX + 8);
				int32x4_t coeffs_x_3 = *(int32x4_t*)(kernelTable + filterIndexX + 12);
				uint8x16_t row_8 = vld1q_u8((uint8_t*)(sample + section * 4));
				int16x8_t row_16_a = vmovl_u8(vget_low_u8(row_8));
				int16x8_t row_16_b = vmovl_u8(vget_high_u8(row_8));
				int32x4_t row_32_a = vmovl_s16(vget_low_s16(row_16_a));
				int32x4_t row_32_b = vmovl_s16(vget_high_s16(row_16_a));
				int32x4_t row_32_c = vmovl_s16(vget_low_s16(row_16_b));
				int32x4_t row_32_d = vmovl_s16(vget_high_s16(row_16_b));
				int32x4_t mul_a = vmulq_s32(row_32_a, coeffs_x_0);
				int32x4_t mul_b = vmulq_s32(row_32_b, coeffs_x_1);
				int32x4_t mul_c = vmulq_s32(row_32_c, coeffs_x_2);
				int32x4_t mul_d = vmulq_s32(row_32_d, coeffs_x_3);
				result = vaddq_s32(result, vaddq_s32(mul_a, vaddq_s32(mul_b, vaddq_s32(mul_c, mul_d))));
			}
			result = vaddq_s32(result, half);
			result = vshrq_n_s32(result, 16);
			int8x8_t packed_8 = vqmovun_s16(vcombine_s16(vmovn_s32(result), vdup_n_s16(0)));
			unsigned int oi = (x * outputPitch) + y * 4;
			vst1_lane_s32((int32_t*)(outputBuffer + oi), packed_8, 0);
		}
	}
}


static void adaptiveSeperable8(const FilterKernelAdaptive* kernel, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity)
{
	// The seperable version writes transposed images.
	SECURE_ASSERT(SafeUMul(outputHeight, 4U) <= outputPitch);
	SECURE_ASSERT(SafeUMul(outputWidth, outputPitch) <= outputCapacity);

	__restrict int32_t* kernelTable = kernel->getTableFixedPoint4();

	int32x4_t zero = vdupq_n_s32(0);
	int32x4_t half = vdupq_n_s32(kHalf16);

	for( unsigned int y = 0; y < outputHeight; y++ ) {
		for( unsigned int x = 0; x < outputWidth; x++ ) {
			int startX = kernel->computeSampleStart(x);

			int sampleOffset = (y * (int)inputPitch) + (startX) * 4;
			const uint8_t* sample = inputBuffer + sampleOffset;

			int32x4_t result = zero;
			unsigned int filterIndexX = x * 32;

			int32x4_t coeffs_x_0 = *(int32x4_t*)(kernelTable + filterIndexX + 0);
			int32x4_t coeffs_x_1 = *(int32x4_t*)(kernelTable + filterIndexX + 4);
			int32x4_t coeffs_x_2 = *(int32x4_t*)(kernelTable + filterIndexX + 8);
			int32x4_t coeffs_x_3 = *(int32x4_t*)(kernelTable + filterIndexX + 12);
			int32x4_t coeffs_x_4 = *(int32x4_t*)(kernelTable + filterIndexX + 16);
			int32x4_t coeffs_x_5 = *(int32x4_t*)(kernelTable + filterIndexX + 20);
			int32x4_t coeffs_x_6 = *(int32x4_t*)(kernelTable + filterIndexX + 24);
			int32x4_t coeffs_x_7 = *(int32x4_t*)(kernelTable + filterIndexX + 28);

			uint8x16_t row_8_a = vld1q_u8((uint8_t*)sample);
			int16x8_t row_16_a = vmovl_u8(vget_low_u8(row_8_a));
			int16x8_t row_16_b = vmovl_u8(vget_high_u8(row_8_a));
			int32x4_t row_32_a = vmovl_s16(vget_low_s16(row_16_a));
			int32x4_t row_32_b = vmovl_s16(vget_high_s16(row_16_a));
			int32x4_t row_32_c = vmovl_s16(vget_low_s16(row_16_b));
			int32x4_t row_32_d = vmovl_s16(vget_high_s16(row_16_b));
			int32x4_t mul_a = vmulq_s32(row_32_a, coeffs_x_0);
			int32x4_t mul_b = vmulq_s32(row_32_b, coeffs_x_1);
			int32x4_t mul_c = vmulq_s32(row_32_c, coeffs_x_2);
			int32x4_t mul_d = vmulq_s32(row_32_d, coeffs_x_3);

			result = vaddq_s32(result, vaddq_s32(mul_a, vaddq_s32(mul_b, vaddq_s32(mul_c, mul_d))));

			uint8x16_t row_8_b = vld1q_u8((uint8_t*)(sample + 16));
			int16x8_t row_16_c = vmovl_u8(vget_low_u8(row_8_b));
			int16x8_t row_16_d = vmovl_u8(vget_high_u8(row_8_b));
			int32x4_t row_32_e = vmovl_s16(vget_low_s16(row_16_c));
			int32x4_t row_32_f = vmovl_s16(vget_high_s16(row_16_c));
			int32x4_t row_32_g = vmovl_s16(vget_low_s16(row_16_d));
			int32x4_t row_32_h = vmovl_s16(vget_high_s16(row_16_d));
			int32x4_t mul_e = vmulq_s32(row_32_e, coeffs_x_4);
			int32x4_t mul_f = vmulq_s32(row_32_f, coeffs_x_5);
			int32x4_t mul_g = vmulq_s32(row_32_g, coeffs_x_6);
			int32x4_t mul_h = vmulq_s32(row_32_h, coeffs_x_7);

			result = vaddq_s32(result, vaddq_s32(mul_e, vaddq_s32(mul_f, vaddq_s32(mul_g, mul_h))));

			result = vaddq_s32(result, half);
			result = vshrq_n_s32(result, 16);

			int8x8_t packed_8 = vqmovun_s16(vcombine_s16(vmovn_s32(result), vdup_n_s16(0)));
			unsigned int oi = (x * outputPitch) + y * 4;
			vst1_lane_s32((int32_t*)(outputBuffer + oi), packed_8, 0);
		}
	}
}

// Adaptive-width filter, single axis, 12 samples.
// 16.16 Fixed point SSE version
static void adaptiveSeperable12(const FilterKernelAdaptive* kernel, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity)
{
	// The seperable version writes transposed images.
	SECURE_ASSERT(SafeUMul(outputHeight, 4U) <= outputPitch);
	SECURE_ASSERT(SafeUMul(outputWidth, outputPitch) <= outputCapacity);

	__restrict int32_t* kernelTable = kernel->getTableFixedPoint4();

	int32x4_t zero = vdupq_n_s32(0);
	int32x4_t half = vdupq_n_s32(kHalf16);

	for( unsigned int y = 0; y < outputHeight; y++ ) {
		for( unsigned int x = 0; x < outputWidth; x++ ) {
			int startX = kernel->computeSampleStart(x);
			int sampleOffset = (y * (int)inputPitch) + (startX) * 4;
			const uint8_t* sample = inputBuffer + sampleOffset;

			int32x4_t result = zero;
			unsigned int filterIndexX = x * 48;

			int32x4_t coeffs_x_0 = *(int32x4_t*)(kernelTable + filterIndexX + 0);
			int32x4_t coeffs_x_1 = *(int32x4_t*)(kernelTable + filterIndexX + 4);
			int32x4_t coeffs_x_2 = *(int32x4_t*)(kernelTable + filterIndexX + 8);
			int32x4_t coeffs_x_3 = *(int32x4_t*)(kernelTable + filterIndexX + 12);
			int32x4_t coeffs_x_4 = *(int32x4_t*)(kernelTable + filterIndexX + 16);
			int32x4_t coeffs_x_5 = *(int32x4_t*)(kernelTable + filterIndexX + 20);
			int32x4_t coeffs_x_6 = *(int32x4_t*)(kernelTable + filterIndexX + 24);
			int32x4_t coeffs_x_7 = *(int32x4_t*)(kernelTable + filterIndexX + 28);
			int32x4_t coeffs_x_8 = *(int32x4_t*)(kernelTable + filterIndexX + 32);
			int32x4_t coeffs_x_9 = *(int32x4_t*)(kernelTable + filterIndexX + 36);
			int32x4_t coeffs_x_10 = *(int32x4_t*)(kernelTable + filterIndexX + 40);
			int32x4_t coeffs_x_11 = *(int32x4_t*)(kernelTable + filterIndexX + 44);

			uint8x16_t row_8_a = vld1q_u8((uint8_t*)sample);
			int16x8_t row_16_a = vmovl_u8(vget_low_u8(row_8_a));
			int16x8_t row_16_b = vmovl_u8(vget_high_u8(row_8_a));
			int32x4_t row_32_a = vmovl_s16(vget_low_s16(row_16_a));
			int32x4_t row_32_b = vmovl_s16(vget_high_s16(row_16_a));
			int32x4_t row_32_c = vmovl_s16(vget_low_s16(row_16_b));
			int32x4_t row_32_d = vmovl_s16(vget_high_s16(row_16_b));
			int32x4_t mul_a = vmulq_s32(row_32_a, coeffs_x_0);
			int32x4_t mul_b = vmulq_s32(row_32_b, coeffs_x_1);
			int32x4_t mul_c = vmulq_s32(row_32_c, coeffs_x_2);
			int32x4_t mul_d = vmulq_s32(row_32_d, coeffs_x_3);

			result = vaddq_s32(result, vaddq_s32(mul_a, vaddq_s32(mul_b, vaddq_s32(mul_c, mul_d))));

			uint8x16_t row_8_b = vld1q_u8((uint8_t*)(sample + 16));
			int16x8_t row_16_c = vmovl_u8(vget_low_u8(row_8_b));
			int16x8_t row_16_d = vmovl_u8(vget_high_u8(row_8_b));
			int32x4_t row_32_e = vmovl_s16(vget_low_s16(row_16_c));
			int32x4_t row_32_f = vmovl_s16(vget_high_s16(row_16_c));
			int32x4_t row_32_g = vmovl_s16(vget_low_s16(row_16_d));
			int32x4_t row_32_h = vmovl_s16(vget_high_s16(row_16_d));
			int32x4_t mul_e = vmulq_s32(row_32_e, coeffs_x_4);
			int32x4_t mul_f = vmulq_s32(row_32_f, coeffs_x_5);
			int32x4_t mul_g = vmulq_s32(row_32_g, coeffs_x_6);
			int32x4_t mul_h = vmulq_s32(row_32_h, coeffs_x_7);

			result = vaddq_s32(result, vaddq_s32(mul_e, vaddq_s32(mul_f, vaddq_s32(mul_g, mul_h))));

			uint8x16_t row_8_c = vld1q_u8((uint8_t*)(sample + 32));
			int16x8_t row_16_e = vmovl_u8(vget_low_u8(row_8_c));
			int16x8_t row_16_f = vmovl_u8(vget_high_u8(row_8_c));
			int32x4_t row_32_i = vmovl_s16(vget_low_s16(row_16_e));
			int32x4_t row_32_j = vmovl_s16(vget_high_s16(row_16_e));
			int32x4_t row_32_k = vmovl_s16(vget_low_s16(row_16_f));
			int32x4_t row_32_l = vmovl_s16(vget_high_s16(row_16_f));
			int32x4_t mul_i = vmulq_s32(row_32_i, coeffs_x_8);
			int32x4_t mul_j = vmulq_s32(row_32_j, coeffs_x_9);
			int32x4_t mul_k = vmulq_s32(row_32_k, coeffs_x_10);
			int32x4_t mul_l = vmulq_s32(row_32_l, coeffs_x_11);

			result = vaddq_s32(result, vaddq_s32(mul_i, vaddq_s32(mul_j, vaddq_s32(mul_k, mul_l))));

			result = vaddq_s32(result, half);
			result = vshrq_n_s32(result, 16);

			int8x8_t packed_8 = vqmovun_s16(vcombine_s16(vmovn_s32(result), vdup_n_s16(0)));
			unsigned int oi = (x * outputPitch) + y * 4;
			vst1_lane_s32((int32_t*)(outputBuffer + oi), packed_8, 0);
		}
	}
}

// Adaptive-width filter, variable number of samples.
template<>
void Filters<ComponentSIMD<4>>::adaptiveSeperable(const FilterKernelAdaptive* kernel, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity, bool)
{
	unsigned int kernelSize = kernel->getKernelSize();
	if( kernelSize == 8U ) {
		adaptiveSeperable8(kernel, inputBuffer, inputWidth, inputHeight, inputPitch, outputBuffer, outputWidth, outputHeight, outputPitch, outputCapacity);
	} else if( kernelSize == 12U ) {
		adaptiveSeperable12(kernel, inputBuffer, inputWidth, inputHeight, inputPitch, outputBuffer, outputWidth, outputHeight, outputPitch, outputCapacity);
	} else {
		adaptiveSeperableAny(kernel, inputBuffer, inputWidth, inputHeight, inputPitch, outputBuffer, outputWidth, outputHeight, outputPitch, outputCapacity);
	}
}

template<>
void Filters<ComponentSIMD<4>>::fixed4x4(const FilterKernelFixed *kernelX, const FilterKernelFixed *kernelY, const uint8_t *inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t *outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity)
{
	SECURE_ASSERT(SafeUMul(outputWidth, 4U) <= outputPitch);
	SECURE_ASSERT(SafeUMul(outputHeight, outputPitch) <= outputCapacity);

	__restrict int32_t* kernelTableX = kernelX->getTableFixedPoint4();
	__restrict int32_t* kernelTableY = kernelY->getTableFixedPoint4();

	int32x4_t half = vdupq_n_s32(kHalf22);

	for( unsigned int y = 0; y < outputHeight; y++ ) {
		int sampleY = kernelY->computeSampleStart(y);
		for( unsigned int x = 0; x < outputWidth; x++ ) {
			int sampleX = kernelX->computeSampleStart(x);

			int sampleOffset = ((sampleY - 1) * (int)inputPitch) + (sampleX - 1) * 4;

			const uint8_t* sample = inputBuffer + sampleOffset;

			int32x4_t final;

			unsigned int filterIndexX = x;
			filterIndexX *= 16;
			int32x4_t coeffs_x_0 = *(int32x4_t*)(kernelTableX + filterIndexX + 0);
			int32x4_t coeffs_x_1 = *(int32x4_t*)(kernelTableX + filterIndexX + 4);
			int32x4_t coeffs_x_2 = *(int32x4_t*)(kernelTableX + filterIndexX + 8);
			int32x4_t coeffs_x_3 = *(int32x4_t*)(kernelTableX + filterIndexX + 12);

			unsigned int filter_index_y = y;
			filter_index_y *= 16;

			int32x4_t coeffs_y_0 = *(int32x4_t*)(kernelTableY + filter_index_y + 0);
			{
				uint8x16_t row_8 = vld1q_u8((uint8_t*)sample);
				int16x8_t row_16_a = vmovl_u8(vget_low_u8(row_8));
				int16x8_t row_16_b = vmovl_u8(vget_high_u8(row_8));
				int32x4_t row_32_a = vmovl_s16(vget_low_s16(row_16_a));
				int32x4_t row_32_b = vmovl_s16(vget_high_s16(row_16_a));
				int32x4_t row_32_c = vmovl_s16(vget_low_s16(row_16_b));
				int32x4_t row_32_d = vmovl_s16(vget_high_s16(row_16_b));
				int32x4_t mul_a = vmulq_s32(row_32_a, coeffs_x_0);
				int32x4_t mul_b = vmulq_s32(row_32_b, coeffs_x_1);
				int32x4_t mul_c = vmulq_s32(row_32_c, coeffs_x_2);
				int32x4_t mul_d = vmulq_s32(row_32_d, coeffs_x_3);
				int32x4_t row = vshrq_n_s32(vaddq_s32(mul_a, vaddq_s32(mul_b, vaddq_s32(mul_c, mul_d))), 10);
				final = vmulq_s32(row, coeffs_y_0);
				sample += inputPitch;
			}

			int32x4_t coeffs_y_1 = *(int32x4_t*)(kernelTableY + filter_index_y + 4);
			{
				uint8x16_t row_8 = vld1q_u8((uint8_t*)sample);
				int16x8_t row_16_a = vmovl_u8(vget_low_u8(row_8));
				int16x8_t row_16_b = vmovl_u8(vget_high_u8(row_8));
				int32x4_t row_32_a = vmovl_s16(vget_low_s16(row_16_a));
				int32x4_t row_32_b = vmovl_s16(vget_high_s16(row_16_a));
				int32x4_t row_32_c = vmovl_s16(vget_low_s16(row_16_b));
				int32x4_t row_32_d = vmovl_s16(vget_high_s16(row_16_b));
				int32x4_t mul_a = vmulq_s32(row_32_a, coeffs_x_0);
				int32x4_t mul_b = vmulq_s32(row_32_b, coeffs_x_1);
				int32x4_t mul_c = vmulq_s32(row_32_c, coeffs_x_2);
				int32x4_t mul_d = vmulq_s32(row_32_d, coeffs_x_3);
				int32x4_t row = vshrq_n_s32(vaddq_s32(mul_a, vaddq_s32(mul_b, vaddq_s32(mul_c, mul_d))), 10);
				final = vaddq_s32(final, vmulq_s32(row, coeffs_y_1));
				sample += inputPitch;
			}

			int32x4_t coeffs_y_2 = *(int32x4_t*)(kernelTableY + filter_index_y + 8);
			{
				uint8x16_t row_8 = vld1q_u8((uint8_t*)sample);
				int16x8_t row_16_a = vmovl_u8(vget_low_u8(row_8));
				int16x8_t row_16_b = vmovl_u8(vget_high_u8(row_8));
				int32x4_t row_32_a = vmovl_s16(vget_low_s16(row_16_a));
				int32x4_t row_32_b = vmovl_s16(vget_high_s16(row_16_a));
				int32x4_t row_32_c = vmovl_s16(vget_low_s16(row_16_b));
				int32x4_t row_32_d = vmovl_s16(vget_high_s16(row_16_b));
				int32x4_t mul_a = vmulq_s32(row_32_a, coeffs_x_0);
				int32x4_t mul_b = vmulq_s32(row_32_b, coeffs_x_1);
				int32x4_t mul_c = vmulq_s32(row_32_c, coeffs_x_2);
				int32x4_t mul_d = vmulq_s32(row_32_d, coeffs_x_3);
				int32x4_t row = vshrq_n_s32(vaddq_s32(mul_a, vaddq_s32(mul_b, vaddq_s32(mul_c, mul_d))), 10);
				final = vaddq_s32(final, vmulq_s32(row, coeffs_y_2));
				sample += inputPitch;
			}

			int32x4_t coeffs_y_3 = *(int32x4_t*)(kernelTableY + filter_index_y + 12);
			{
				uint8x16_t row_8 = vld1q_u8((uint8_t*)sample);
				int16x8_t row_16_a = vmovl_u8(vget_low_u8(row_8));
				int16x8_t row_16_b = vmovl_u8(vget_high_u8(row_8));
				int32x4_t row_32_a = vmovl_s16(vget_low_s16(row_16_a));
				int32x4_t row_32_b = vmovl_s16(vget_high_s16(row_16_a));
				int32x4_t row_32_c = vmovl_s16(vget_low_s16(row_16_b));
				int32x4_t row_32_d = vmovl_s16(vget_high_s16(row_16_b));
				int32x4_t mul_a = vmulq_s32(row_32_a, coeffs_x_0);
				int32x4_t mul_b = vmulq_s32(row_32_b, coeffs_x_1);
				int32x4_t mul_c = vmulq_s32(row_32_c, coeffs_x_2);
				int32x4_t mul_d = vmulq_s32(row_32_d, coeffs_x_3);
				int32x4_t row = vshrq_n_s32(vaddq_s32(mul_a, vaddq_s32(mul_b, vaddq_s32(mul_c, mul_d))), 10);
				final = vaddq_s32(final, vmulq_s32(row, coeffs_y_3));
				sample += inputPitch;
			}

			final = vaddq_s32(final, half);
			final = vshrq_n_s32(final, 22);
			int8x8_t packed_8 = vqmovun_s16(vcombine_s16(vmovn_s32(final), vdup_n_s16(0)));
			unsigned int oi = (y * outputPitch) + x * 4;
			vst1_lane_s32((int32_t*)(outputBuffer + oi), packed_8, 0);
		}
	}
}

template<>
bool Filters<ComponentSIMD<4>>::fasterUnpadded(uint32_t kernelSize)
{
	return false;
}

template<>
bool Filters<ComponentSIMD<4>>::supportsUnpadded(uint32_t kernelSize)
{
	return false;
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
