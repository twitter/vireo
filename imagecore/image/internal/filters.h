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

#include "imagecore/imagecore.h"
#include "imagecore/image/kernel.h"
#include "imagecore/image/image.h"

namespace imagecore {

template<typename Component>
class Filters
{
public:
	static void fixed4x4(const FilterKernelFixed* kernelX, const FilterKernelFixed* kernelY, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity);
	static void adaptive4x4(const FilterKernelAdaptive* kernelX, const FilterKernelAdaptive* kernelY, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int input_height, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity);
	static void adaptiveSeparable2x2(const FilterKernelAdaptive* kernelX, const FilterKernelAdaptive* kernelY, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int input_height, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity);
	static void adaptive2x2(const FilterKernelAdaptive* kernelX, const FilterKernelAdaptive* kernelY, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int input_height, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity);
	static void adaptiveSeperable(const FilterKernelAdaptive* kernel, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int input_height, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity, bool unpadded);
	static void reduceHalf(const uint8_t* input_buffer, uint8_t* outputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, unsigned int outputPitch, unsigned int outputCapacity);
	static void rotateLeft(const uint8_t* __restrict input_buffer, uint8_t* __restrict output_buffer, unsigned int width, unsigned int height, unsigned int input_pitch, unsigned int output_pitch,  unsigned int output_capacity);
	static void rotateRight(const uint8_t* __restrict input_buffer, uint8_t* __restrict output_buffer, unsigned int width, unsigned int height, unsigned int input_pitch, unsigned int output_pitch,  unsigned int output_capacity);
	static void rotateUp(const uint8_t* __restrict input_buffer, uint8_t* __restrict output_buffer, unsigned int width, unsigned int height, unsigned int input_pitch, unsigned int output_pitch,  unsigned int output_capacity);
	static void transpose(const uint8_t* __restrict input_buffer, uint8_t* __restrict output_buffer, unsigned int width, unsigned int height, unsigned int input_pitch, unsigned int output_pitch,  unsigned int output_capacity);
	static void bilinearTwoLines(uint8_t* dstRow, const uint8_t* srcRow0, const uint8_t* srcRow1, uint16_t coeff0, uint16_t coeff1, uint32_t length);
	static bool fasterUnpadded(uint32_t kernelSize);
	static bool supportsUnpadded(uint32_t kernelSize);
};

class FiltersConfig
{
public:
	static void setScalarMode(bool val);

private:
	static bool m_ScalarMode;
	template<typename Component> friend class Filters;
};

#if  __SSE4_1__ || __ARM_NEON__

// SIMD specializations.
template<> void Filters<ComponentSIMD<4>>::fixed4x4(const FilterKernelFixed* kernelX, const FilterKernelFixed* kernelY, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity);
template<> void Filters<ComponentSIMD<4>>::adaptive4x4(const FilterKernelAdaptive* kernelX, const FilterKernelAdaptive* kernelY, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int input_height, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity);
template<> void Filters<ComponentSIMD<4>>::adaptiveSeperable(const FilterKernelAdaptive* kernel, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int input_height, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity, bool unpadded);
template<> void Filters<ComponentSIMD<4>>::reduceHalf(const uint8_t* input_buffer, uint8_t* outputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, unsigned int outputPitch, unsigned int outputCapacity);
template<> bool Filters<ComponentSIMD<4>>::fasterUnpadded(uint32_t kernelSize);
template<> bool Filters<ComponentSIMD<4>>::supportsUnpadded(uint32_t kernelSize);
template<> void Filters<ComponentSIMD<2>>::reduceHalf(const uint8_t* input_buffer, uint8_t* outputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, unsigned int outputPitch, unsigned int outputCapacity);
template<> void Filters<ComponentSIMD<1>>::reduceHalf(const uint8_t* input_buffer, uint8_t* outputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, unsigned int outputPitch, unsigned int outputCapacity);
template<> void Filters<ComponentSIMD<1>>::transpose(const uint8_t* __restrict input_buffer, uint8_t* __restrict output_buffer, unsigned int width, unsigned int height, unsigned int input_pitch, unsigned int output_pitch,  unsigned int output_capacity);
template<> void Filters<ComponentSIMD<2>>::transpose(const uint8_t* __restrict input_buffer, uint8_t* __restrict output_buffer, unsigned int width, unsigned int height, unsigned int input_pitch, unsigned int output_pitch,  unsigned int output_capacity);
template<> void Filters<ComponentSIMD<1>>::bilinearTwoLines(uint8_t* dstRow, const uint8_t* srcRow0, const uint8_t* srcRow1, uint16_t coeff0, uint16_t coeff1, uint32_t length);
template<> void Filters<ComponentSIMD<2>>::bilinearTwoLines(uint8_t* dstRow, const uint8_t* srcRow0, const uint8_t* srcRow1, uint16_t coeff0, uint16_t coeff1, uint32_t length);

#endif

#if  __SSE4_1__

template<> void Filters<ComponentSIMD<1>>::adaptiveSeperable(const FilterKernelAdaptive* kernel, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int input_height, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity, bool unpadded);
template<> bool Filters<ComponentSIMD<1>>::fasterUnpadded(uint32_t kernelSize);
template<> bool Filters<ComponentSIMD<1>>::supportsUnpadded(uint32_t kernelSize);

#endif

}
