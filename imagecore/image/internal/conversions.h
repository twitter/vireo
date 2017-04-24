//
//  conversions.h
//  ImageCore
//
//  Created by Paul Melamed on 4/13/15.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "imagecore/imagecore.h"
#include "imagecore/image/kernel.h"
#include "imagecore/image/image.h"

template<bool useIntrinsics>
class Conversions
{
	public:
		// converts a rgba image to yuv semiplanar format
		static void rgba_to_yuv420(uint8_t* dstY, uint8_t* dstUV, const uint8_t* srcRGBA, uint32_t inputWidth, uint32_t inputHeight, uint32_t inputPitch, uint32_t outputPitchY, uint32_t outputPitchUV);

		// converts a single rgb value to yuv
		static void rgb_to_yuv(int16_t& y, int16_t& u, int16_t& v, uint8_t r, uint8_t g, uint8_t b);

	private:
		static const int16_t yr = 76;
		static const int16_t yg = 150;
		static const int16_t yb = 29;
		static const int16_t ur = -43;
		static const int16_t ug = -84;
		static const int16_t ub = 127;
		static const int16_t vr = 127;
		static const int16_t vg = -106;
		static const int16_t vb = -21;
};

class ConversionsConfig
{
	public:
		static void setScalarMode(bool val);

	private:
		static bool m_ScalarMode;
		template<bool useIntrinsics> friend class Conversions;
};


#if  __SSE4_1__ || __ARM_NEON__

// SIMD specializations.
template<> void Conversions<true>::rgba_to_yuv420(uint8_t* dstY, uint8_t* dstUV, const uint8_t* srcRGBA, uint32_t inputWidth, uint32_t inputHeight, uint32_t inputPitch, uint32_t outputPitchY, uint32_t outputPitchUV);

#endif
