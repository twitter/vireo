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

#include "conversions.h"
#include "platform_support.h"
#include "imagecore/utils/mathtypes.h"

bool ConversionsConfig::m_ScalarMode = false;

void ConversionsConfig::setScalarMode(bool val)
{
	m_ScalarMode = val;
}

//1. Basic transform from 8 bit RGB to 16 bit values (Y': unsigned, U/V: signed, matrix values got rounded so that the later on desired Y'UV range of each [0..255] is reached whilst no overflow can happen):
//
// Y'   76  150  29   R
// U   -43  -84 127   G
// V   127 -106 -21   B
//
//2. Scale down (">>8") to 8 bit values with rounding ("+128") (Y': unsigned, U/V: signed):
//
//Yt' = (Y' + 128) >> 8
//Ut  = (U  + 128) >> 8
//Vt  = (V  + 128) >> 8

//3. Add an offset to the values to eliminate any negative values (all results are 8 bit unsigned):
//
//Yu' = Yt'
//Uu  = Ut + 128
//Vu  = Vt + 128

template<bool useIntrinsics>
void Conversions<useIntrinsics>::rgb_to_yuv(int16_t& y, int16_t& u, int16_t& v, uint8_t r, uint8_t g, uint8_t b)
{
	y = (yr * r + yg * g + yb * b) >> 8;
	u = ((ur * r + ug * g + ub * b) >> 8) + 128;
	v = ((vr * r + vg * g + vb * b) >> 8) + 128;
}

template<bool useIntrinsics>
void Conversions<useIntrinsics>::rgba_to_yuv420(uint8_t* dstY, uint8_t* dstUV, const uint8_t* srcRGBA, uint32_t inputWidth, uint32_t inputHeight, uint32_t inputPitch, uint32_t outputPitchY, uint32_t outputPitchUV)
{
	if(inputWidth & (~1)) {
		for(uint32_t row = 0; row < inputHeight; row += 2) {
			uint8_t* outputY0 = dstY;
			uint8_t* outputY1 = dstY + outputPitchY;
			uint8_t* outputUV = dstUV;
			const uint8_t* inputRGBA0 = srcRGBA;
			const uint8_t* inputRGBA1 = srcRGBA + inputPitch;
			for(uint32_t column = 0; column < inputWidth; column += 2) {
				int16_t r0 = inputRGBA0[0];
				int16_t g0 = inputRGBA0[1];
				int16_t b0 = inputRGBA0[2];
				inputRGBA0 += 4;
				int16_t r1 = inputRGBA0[0];
				int16_t g1 = inputRGBA0[1];
				int16_t b1 = inputRGBA0[2];
				inputRGBA0 += 4;
				int16_t r2 = inputRGBA1[0];
				int16_t g2 = inputRGBA1[1];
				int16_t b2 = inputRGBA1[2];
				inputRGBA1 += 4;
				int16_t r3 = inputRGBA1[0];
				int16_t g3 = inputRGBA1[1];
				int16_t b3 = inputRGBA1[2];
				inputRGBA1 += 4;
				int16_t Y0;
				int16_t U;
				int16_t V;
				rgb_to_yuv(Y0, U, V, r0, g0, b0);
				int16_t Y1 = (yr * r1 + yg * g1 + yb * b1) >> 8;
				U += ((ur * r1 + ug * g1 + ub * b1) >> 8) + 128;
				V += ((vr * r1 + vg * g1 + vb * b1) >> 8) + 128;
				int16_t Y2 = (yr * r2 + yg * g2 + yb * b2) >> 8;
				U += ((ur * r2 + ug * g2 + ub * b2) >> 8) + 128;
				V += ((vr * r2 + vg * g2 + vb * b2) >> 8) + 128;
				int16_t Y3 = (yr * r3 + yg * g3 + yb * b3) >> 8;
				U += ((ur * r3 + ug * g3 + ub * b3) >> 8) + 128;
				V += ((vr * r3 + vg * g3 + vb * b3) >> 8) + 128;
				*outputY0++ = (uint8_t)Y0;
				*outputY0++ = (uint8_t)Y1;
				*outputY1++ = (uint8_t)Y2;
				*outputY1++ = (uint8_t)Y3;
				*outputUV++ = (uint8_t)(U >> 2);
				*outputUV++ = (uint8_t)(V >> 2);
			}
			dstY += outputPitchY * 2;
			dstUV += outputPitchUV;
			srcRGBA += inputPitch * 2;
		}
	}
}

// forward template declarations
template class Conversions<false>;
template class Conversions<true>;

#if  __SSE4_1__ || __ARM_NEON__

#include "intrinsics.h"

static void rgba_to_yuv420x4(uint8_t* dstY, uint8_t* dstUV, const uint8_t* srcRGBA, uint32_t inputWidth, uint32_t inputHeight, uint32_t inputPitch, uint32_t outputPitchY, uint32_t outputPitchUV)
{
	uint32_t columns_processed = inputWidth & (~3);
	if(columns_processed) { // 4 pixels wide version
		vSInt32 zero = v128_setzero();
		vSInt16 coeff_ry = v128_set_int16(76);
		vSInt16 coeff_gy = v128_set_int16(150);
		vSInt16 coeff_by = v128_set_int16(29);

		vSInt16 coeff_ru = v128_set_int16(-43);
		vSInt16 coeff_gu = v128_set_int16(-84);
		vSInt16 coeff_bu = v128_set_int16(127);

		vSInt16 coeff_rv = v128_set_int16(127);
		vSInt16 coeff_gv = v128_set_int16(-106);
		vSInt16 coeff_bv = v128_set_int16(-21);

		vSInt16 uv_bias = v128_set_int16(128);

		vUInt8 mergeMask = v128_set_int8_packed(ZMASK, ZMASK, ZMASK, ZMASK, ZMASK, ZMASK, ZMASK, ZMASK, 14, 6, 12, 4, 10, 2, 8, 0);
		for(uint32_t row = 0; row < inputHeight; row += 2) {
			uint8_t* outputY0 = dstY;
			uint8_t* outputY1 = dstY + outputPitchY;
			uint8_t* outputUV = dstUV;
			const uint8_t* inputRGBA0 = srcRGBA;
			const uint8_t* inputRGBA1 = srcRGBA + inputPitch;

			for(uint32_t column = 0; column < inputWidth; column += 4) {
				vSInt8 row0 = v128_load_unaligned((const vSInt32*)inputRGBA0);
				vSInt8 row1 = v128_load_unaligned((const vSInt32*)inputRGBA1);
				inputRGBA0 += 16;
				inputRGBA1 += 16;
				vSInt8 row0_pixels_01;
				vSInt8 row0_pixels_23;
				v128_unpack_int8(row0_pixels_01, row0_pixels_23, row0, zero);
				vSInt8 row1_pixels_45;
				vSInt8 row1_pixels_67;
				v128_unpack_int8(row1_pixels_45, row1_pixels_67, row1, zero);
				vSInt64 r0r2r4r6r1r3r5r7;
				vSInt64 g0g2g4g6g1g3g5g7;
				vSInt64 b0b2b4b6b1b3b5b7;
				vSInt64 a0a2a4a6a1a3a5a7;
				IMAGECORE_UNUSED(a0a2a4a6a1a3a5a7);
				vec_transpose_int16(row0_pixels_01, row0_pixels_23, row1_pixels_45, row1_pixels_67, r0r2r4r6r1r3r5r7, g0g2g4g6g1g3g5g7, b0b2b4b6b1b3b5b7, a0a2a4a6a1a3a5a7);

				vSInt16 y0y2y4y6y1y3y5y7 = v128_mul_int16(r0r2r4r6r1r3r5r7, coeff_ry);
				y0y2y4y6y1y3y5y7 = v128_add_int16(y0y2y4y6y1y3y5y7, v128_mul_int16(g0g2g4g6g1g3g5g7, coeff_gy));
				y0y2y4y6y1y3y5y7 = v128_add_int16(y0y2y4y6y1y3y5y7, v128_mul_int16(b0b2b4b6b1b3b5b7, coeff_by));
				y0y2y4y6y1y3y5y7 = v128_shift_right_unsigned_int16<8>(y0y2y4y6y1y3y5y7);

				vSInt16 u0u2u4u6u1u3u5u7 = v128_mul_int16(r0r2r4r6r1r3r5r7, coeff_ru);
				u0u2u4u6u1u3u5u7 = v128_add_int16(u0u2u4u6u1u3u5u7, v128_mul_int16(g0g2g4g6g1g3g5g7, coeff_gu));
				u0u2u4u6u1u3u5u7 = v128_add_int16(u0u2u4u6u1u3u5u7, v128_mul_int16(b0b2b4b6b1b3b5b7, coeff_bu));
				u0u2u4u6u1u3u5u7 = v128_shift_right_unsigned_int16<8>(u0u2u4u6u1u3u5u7);
				u0u2u4u6u1u3u5u7 = v128_add_int16(u0u2u4u6u1u3u5u7, uv_bias);

				vSInt16 v0v2v4v6v1v3v5v7 = v128_mul_int16(r0r2r4r6r1r3r5r7, coeff_rv);
				v0v2v4v6v1v3v5v7 = v128_add_int16(v0v2v4v6v1v3v5v7, v128_mul_int16(g0g2g4g6g1g3g5g7, coeff_gv));
				v0v2v4v6v1v3v5v7 = v128_add_int16(v0v2v4v6v1v3v5v7, v128_mul_int16(b0b2b4b6b1b3b5b7, coeff_bv));
				v0v2v4v6v1v3v5v7 = v128_shift_right_unsigned_int16<8>(v0v2v4v6v1v3v5v7);
				v0v2v4v6v1v3v5v7 = v128_add_int16(v0v2v4v6v1v3v5v7, uv_bias);

				vUInt8x8 y0y1y2y3y4y5y6y7 = v128_merge(y0y2y4y6y1y3y5y7, mergeMask);

				type64 y;
				y.m_64 = v128_convert_to_int64(y0y1y2y3y4y5y6y7);
				*(uint32_t*)outputY0 = y.m_32[0];
				*(uint32_t*)outputY1 = y.m_32[1];
				outputY0 += 4;
				outputY1 += 4;
				vUInt8x8 u0u1u2u3u4u5u6u7 = v128_merge(u0u2u4u6u1u3u5u7, mergeMask);
				vUInt8x8 v0v1v2v3v4v5v6v7 = v128_merge(v0v2v4v6v1v3v5v7, mergeMask);
				type64 u;
				type64 v;
				u.m_64 = v128_convert_to_int64(u0u1u2u3u4u5u6u7);
				v.m_64 = v128_convert_to_int64(v0v1v2v3v4v5v6v7);
				*outputUV++ = ((u.m_8[0] + u.m_8[1] + u.m_8[4] + u.m_8[5]) >> 2);
				*outputUV++ = ((v.m_8[0] + v.m_8[1] + v.m_8[4] + v.m_8[5]) >> 2);
				*outputUV++ = ((u.m_8[2] + u.m_8[3] + u.m_8[6] + u.m_8[7]) >> 2);
				*outputUV++ = ((v.m_8[2] + v.m_8[3] + v.m_8[6] + v.m_8[7]) >> 2);
			}
			dstY += outputPitchY * 2;
			dstUV += outputPitchUV;
			srcRGBA += inputPitch * 2;
		}
	}

	uint32_t columns_remaining = inputWidth - columns_processed;
	if(columns_remaining) {
		Conversions<false>::rgba_to_yuv420(&dstY[columns_processed], &dstUV[columns_processed * 2], &srcRGBA[columns_processed * 4], columns_remaining, inputHeight, inputPitch, outputPitchY, outputPitchUV);
	}
}

template<>
void Conversions<true>::rgba_to_yuv420(uint8_t* dstY, uint8_t* dstUV, const uint8_t* srcRGBA, uint32_t inputWidth, uint32_t inputHeight, uint32_t inputPitch, uint32_t outputPitchY, uint32_t outputPitchUV)
{
	if(ConversionsConfig::m_ScalarMode) {
		Conversions<false>::rgba_to_yuv420(dstY, dstUV, srcRGBA, inputWidth, inputHeight, inputPitch, outputPitchY, outputPitchUV);
		return;
	}
#if IMAGECORE_DETECT_SSE
	if( !checkForCPUSupport(kCPUFeature_SSE4_1)) {
		Conversions<false>::rgba_to_yuv420(dstY, dstUV, srcRGBA, inputWidth, inputHeight, inputPitch, outputPitchY, outputPitchUV);
		return;
	}
#endif
	rgba_to_yuv420x4(dstY, dstUV, srcRGBA, inputWidth, inputHeight, inputPitch, outputPitchY, outputPitchUV);
}
#endif
