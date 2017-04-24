//
//  filters.cpp
//  ImageTool
//
//  Created by Luke Alonso on 10/24/12.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#include "imagecore/imagecore.h"
#include "imagecore/utils/securemath.h"
#include "imagecore/utils/mathutils.h"
#include "filters.h"

#define COMPONENT_SIZE ((unsigned int)sizeof(Component))

namespace imagecore {

const int kHalf16 = (1 << 15) - 1;
const int kHalf22 = (1 << 21) - 1;


bool FiltersConfig::m_ScalarMode = false;

void FiltersConfig::setScalarMode(bool val)
{
	m_ScalarMode = val;
}

template<typename Component>
void Filters<Component>::reduceHalf(const uint8_t* inputBuffer, uint8_t* outputBuffer, unsigned int width, unsigned int height, unsigned int pitch, unsigned int outputPitch, unsigned int outputCapacity)
{
	unsigned int outputWidth = width >> 1;
	unsigned int rowLength = SafeUMul(outputWidth, COMPONENT_SIZE);
	if(rowLength > 0) {
		unsigned int outputHeight = height >> 1;
		SECURE_ASSERT(SafeUMul(outputWidth, COMPONENT_SIZE) <= outputPitch);
		SECURE_ASSERT(SafeUMul(outputHeight, outputPitch) <= outputCapacity);
		unsigned int inputPitch = pitch;
		uint8_t* outputSample = outputBuffer;
		const uint8_t* inputSample = inputBuffer;
		uint8_t* imageEnd = outputSample + (outputPitch * outputHeight);
		do {
			const uint8_t* rowInputSample = inputSample;
			uint8_t* rowOutputSample = outputSample;
			uint8_t* rowEnd = rowOutputSample + rowLength;
			do {
				const uint8_t* samp_00 = rowInputSample;
				const uint8_t* samp_01 = rowInputSample + COMPONENT_SIZE;
				const uint8_t* samp_10 = rowInputSample + inputPitch;
				const uint8_t* samp_11 = rowInputSample + (inputPitch + COMPONENT_SIZE);
				for( unsigned int i = 0; i < COMPONENT_SIZE; i++ ) {
					rowOutputSample[i] = (samp_00[i] + samp_01[i] + samp_10[i] + samp_11[i]) >> 2;
				}
				rowInputSample += COMPONENT_SIZE * 2;
				rowOutputSample += COMPONENT_SIZE;
			} while (rowOutputSample != rowEnd);
			outputSample += outputPitch;
			inputSample += inputPitch * 2;
		} while (outputSample != imageEnd);
	}
}

template<typename Component>
void Filters<Component>::adaptive4x4(const FilterKernelAdaptive* kernelX, const FilterKernelAdaptive* kernelY, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity)
{
	SECURE_ASSERT(SafeUMul(outputWidth, COMPONENT_SIZE) <= outputPitch);
	SECURE_ASSERT(SafeUMul(outputHeight, outputPitch) <= outputCapacity);

	__restrict int32_t* kernelTableX = kernelX->getTableFixedPoint();
	__restrict int32_t* kernelTableY = kernelY->getTableFixedPoint();

	for( unsigned int y = 0; y < outputHeight; y++ ) {
		int startY = kernelY->computeSampleStart(y);
		for( unsigned int x = 0; x < outputWidth; x++ ) {
			int startX = kernelX->computeSampleStart(x);

			int sampleOffset = ((startY) * (int)inputPitch) + (startX) * COMPONENT_SIZE;
			const uint8_t* sample = inputBuffer + sampleOffset;

			int final[COMPONENT_SIZE];
			for( unsigned int i = 0; i < COMPONENT_SIZE; i++ ) {
				final[i] = 0;
			}

			unsigned int filterIndexX = x * 4;
			unsigned int filterIndexY = y * 4;

			int coeffsX0 = *(kernelTableX + filterIndexX + 0);
			int coeffsX1 = *(kernelTableX + filterIndexX + 1);
			int coeffsX2 = *(kernelTableX + filterIndexX + 2);
			int coeffsX3 = *(kernelTableX + filterIndexX + 3);

			{
				int coeffsY = *(kernelTableY + filterIndexY + 0);
				for( unsigned int i = 0; i < COMPONENT_SIZE; i++ ) {
					int val = (coeffsX0 * (int)sample[0 * COMPONENT_SIZE + i] + coeffsX1 * (int)sample[1 * COMPONENT_SIZE + i] + coeffsX2 * (int)sample[2 * COMPONENT_SIZE + i] + coeffsX3 * (int)sample[3 * COMPONENT_SIZE + i]) >> 10;
					final[i] += val * coeffsY;
				}
				sample += inputPitch;
			}

			{
				int coeffsY = *(kernelTableY + filterIndexY + 1);
				for( unsigned int i = 0; i < COMPONENT_SIZE; i++ ) {
					int val = (coeffsX0 * (int)sample[0 * COMPONENT_SIZE + i] + coeffsX1 * (int)sample[1 * COMPONENT_SIZE + i] + coeffsX2 * (int)sample[2 * COMPONENT_SIZE + i] + coeffsX3 * (int)sample[3 * COMPONENT_SIZE + i]) >> 10;
					final[i] += val * coeffsY;
				}
				sample += inputPitch;
			}

			{
				int coeffsY = *(kernelTableY + filterIndexY + 2);
				for( unsigned int i = 0; i < COMPONENT_SIZE; i++ ) {
					int val = (coeffsX0 * (int)sample[0 * COMPONENT_SIZE + i] + coeffsX1 * (int)sample[1 * COMPONENT_SIZE + i] + coeffsX2 * (int)sample[2 * COMPONENT_SIZE + i] + coeffsX3 * (int)sample[3 * COMPONENT_SIZE + i]) >> 10;
					final[i] += val * coeffsY;
				}
				sample += inputPitch;
			}

			{
				int coeffsY = *(kernelTableY + filterIndexY + 3);
				for( unsigned int i = 0; i < COMPONENT_SIZE; i++ ) {
					int val = (coeffsX0 * (int)sample[0 * COMPONENT_SIZE + i] + coeffsX1 * (int)sample[1 * COMPONENT_SIZE + i] + coeffsX2 * (int)sample[2 * COMPONENT_SIZE + i] + coeffsX3 * (int)sample[3 * COMPONENT_SIZE + i]) >> 10;
					final[i] += val * coeffsY;
				}
				sample += inputPitch;
			}

			uint8_t* outSample = outputBuffer + ((y * outputPitch) + x * COMPONENT_SIZE);
			for( unsigned int i = 0; i < COMPONENT_SIZE; i++ ) {
				outSample[i] = clamp(0, 255, (final[i] + kHalf22) >> 22);
			}
		}
	}
}

// Adaptive-width filter, variable number of samples.
template<typename Component>
void Filters<Component>::adaptiveSeperable(const FilterKernelAdaptive* kernel, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity, bool)
{
	unsigned int kernelSize = kernel->getKernelSize();
	if( kernelSize == 12U ) {
		// The seperable version writes transposed images.
		SECURE_ASSERT(SafeUMul(outputHeight, COMPONENT_SIZE) <= outputPitch);
		SECURE_ASSERT(SafeUMul(outputWidth, outputPitch) <= outputCapacity);

		__restrict int32_t* kernelTableX = kernel->getTableFixedPoint();

		for( unsigned int y = 0; y < outputHeight; y++ ) {
			for( unsigned int x = 0; x < outputWidth; x++ ) {
				int startX = kernel->computeSampleStart(x);
				int sampleOffset = y * (int)inputPitch + startX * COMPONENT_SIZE;
				const uint8_t* sample = inputBuffer + sampleOffset;

				unsigned int filterIndexX = x * 12;

				int final[COMPONENT_SIZE];
				for( unsigned int i = 0; i < COMPONENT_SIZE; i++ ) {
					final[i] = 0;
				}

				for (int i = 0; i < 3; i++) {
					int coeffsX0 = *(kernelTableX + filterIndexX + 0);
					int coeffsX1 = *(kernelTableX + filterIndexX + 1);
					int coeffsX2 = *(kernelTableX + filterIndexX + 2);
					int coeffsX3 = *(kernelTableX + filterIndexX + 3);
					for( unsigned int i = 0; i < COMPONENT_SIZE; i++ ) {
						final[i] += (coeffsX0 * (int)sample[i + 0 * COMPONENT_SIZE] + coeffsX1 * (int)sample[i + 1 * COMPONENT_SIZE] + coeffsX2 * (int)sample[i + 2 * COMPONENT_SIZE] + coeffsX3 * (int)sample[i + 3 * COMPONENT_SIZE]);
					}
					filterIndexX += 4;
					sample += 4 * COMPONENT_SIZE;
				}

				uint8_t* outSample = outputBuffer + ((x * outputPitch) + y * COMPONENT_SIZE);
				for( unsigned int i = 0; i < COMPONENT_SIZE; i++ ) {
					outSample[i] = clamp(0, 255, (final[i] + kHalf16) >> 16);
				}
			}
		}
	} else {
		// Not implemented yet.
		SECURE_ASSERT(0);
	}
}

template<typename Component>
void Filters<Component>::adaptive2x2(const FilterKernelAdaptive* kernelX, const FilterKernelAdaptive* kernelY, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity)
{
	SECURE_ASSERT(SafeUMul(outputWidth, COMPONENT_SIZE) <= outputPitch);
	SECURE_ASSERT(SafeUMul(outputHeight, outputPitch) <= outputCapacity);

	__restrict int32_t* kernelTableX = kernelX->getTableFixedPoint();
	__restrict int32_t* kernelTableY = kernelY->getTableFixedPoint();

	for( unsigned int y = 0; y < outputHeight; y++ ) {
		int startY = kernelY->computeSampleStart(y);
		for( unsigned int x = 0; x < outputWidth; x++ ) {
			int startX = kernelX->computeSampleStart(x);

			int sampleOffset = ((startY) * (int)inputPitch) + (startX) * COMPONENT_SIZE;
			const uint8_t* sample = inputBuffer + sampleOffset;

			int final[COMPONENT_SIZE];
			for( unsigned int i = 0; i < COMPONENT_SIZE; i++ ) {
				final[i] = 0;
			}

			unsigned int filterIndexX = x * 2;
			unsigned int filterIndexY = y * 2;

			int coeffsX0 = *(kernelTableX + filterIndexX + 0);
			int coeffsX1 = *(kernelTableX + filterIndexX + 1);

			{
				int coeffsY = *(kernelTableY + filterIndexY + 0);
				for( unsigned int i = 0; i < COMPONENT_SIZE; i++ ) {
					int val = (coeffsX0 * (int)sample[0 * COMPONENT_SIZE + i] + coeffsX1 * (int)sample[1 * COMPONENT_SIZE + i]) >> 10;
					final[i] += val * coeffsY;
				}
				sample += inputPitch;
			}

			{
				int coeffsY = *(kernelTableY + filterIndexY + 1);
				for( unsigned int i = 0; i < COMPONENT_SIZE; i++ ) {
					int val = (coeffsX0 * (int)sample[0 * COMPONENT_SIZE + i] + coeffsX1 * (int)sample[1 * COMPONENT_SIZE + i]) >> 10;
					final[i] += val * coeffsY;
				}
				sample += inputPitch;
			}

			uint8_t* outSample = outputBuffer + ((y * outputPitch) + x * COMPONENT_SIZE);
			for( unsigned int i = 0; i < COMPONENT_SIZE; i++ ) {
				outSample[i] = clamp(0, 255, (final[i] + kHalf22) >> 22);
			}
		}
	}
}

template<typename Component>
void Filters<Component>::fixed4x4(const FilterKernelFixed *kernelX, const FilterKernelFixed *kernelY, const uint8_t *inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t *outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity)
{
	SECURE_ASSERT(SafeUMul(outputWidth, COMPONENT_SIZE) <= outputPitch);
	SECURE_ASSERT(SafeUMul(outputHeight, outputPitch) <= outputCapacity);

	int32_t* kernelTableX = kernelX->getTableFixedPoint();
	int32_t* kernelTableY = kernelY->getTableFixedPoint();

	for( unsigned int y = 0; y < outputHeight; y++ ) {
		int sampleY = kernelY->computeSampleStart(y);
		for( unsigned int x = 0; x < outputWidth; x++ ) {
			float sampleX = kernelX->computeSampleStart(x);

			int sampleOffset = ((sampleY - 1) * (int)inputPitch) + (sampleX - 1) * COMPONENT_SIZE;
			const uint8_t* sample = inputBuffer + sampleOffset;

			int final[COMPONENT_SIZE];
			for( unsigned int i = 0; i < COMPONENT_SIZE; i++ ) {
				final[i] = 0;
			}

			unsigned int filterIndexX = x * 4;
			int coeffsX0 = *(kernelTableX + filterIndexX + 0);
			int coeffsX1 = *(kernelTableX + filterIndexX + 1);
			int coeffsX2 = *(kernelTableX + filterIndexX + 2);
			int coeffsX3 = *(kernelTableX + filterIndexX + 3);

			unsigned int filterIndexY = y * 4;

			{
				int coeffsY = *(kernelTableY + filterIndexY + 0);
				for( unsigned int i = 0; i < COMPONENT_SIZE; i++ ) {
					int val = (coeffsX0 * (int)sample[0 * COMPONENT_SIZE + i] + coeffsX1 * (int)sample[1 * COMPONENT_SIZE + i] + coeffsX2 * (int)sample[2 * COMPONENT_SIZE + i] + coeffsX3 * (int)sample[3 * COMPONENT_SIZE + i]) >> 10;
					final[i] += val * coeffsY;
				}
				sample += inputPitch;
			}

			{
				int coeffsY = *(kernelTableY + filterIndexY + 1);
				for( unsigned int i = 0; i < COMPONENT_SIZE; i++ ) {
					int val = (coeffsX0 * (int)sample[0 * COMPONENT_SIZE + i] + coeffsX1 * (int)sample[1 * COMPONENT_SIZE + i] + coeffsX2 * (int)sample[2 * COMPONENT_SIZE + i] + coeffsX3 * (int)sample[3 * COMPONENT_SIZE + i]) >> 10;
					final[i] += val * coeffsY;
				}
				sample += inputPitch;
			}

			{
				int coeffsY = *(kernelTableY + filterIndexY + 2);
				for( unsigned int i = 0; i < COMPONENT_SIZE; i++ ) {
					int val = (coeffsX0 * (int)sample[0 * COMPONENT_SIZE + i] + coeffsX1 * (int)sample[1 * COMPONENT_SIZE + i] + coeffsX2 * (int)sample[2 * COMPONENT_SIZE + i] + coeffsX3 * (int)sample[3 * COMPONENT_SIZE + i]) >> 10;
					final[i] += val * coeffsY;
				}
				sample += inputPitch;
			}

			{
				int coeffsY = *(kernelTableY + filterIndexY + 3);
				for( unsigned int i = 0; i < COMPONENT_SIZE; i++ ) {
					int val = (coeffsX0 * (int)sample[0 * COMPONENT_SIZE + i] + coeffsX1 * (int)sample[1 * COMPONENT_SIZE + i] + coeffsX2 * (int)sample[2 * COMPONENT_SIZE + i] + coeffsX3 * (int)sample[3 * COMPONENT_SIZE + i]) >> 10;
					final[i] += val * coeffsY;
				}
				sample += inputPitch;
			}

			uint8_t* outSample = outputBuffer + ((y * outputPitch) + x * COMPONENT_SIZE);
			for( unsigned int i = 0; i < COMPONENT_SIZE; i++ ) {
				outSample[i] = clamp(0, 255, (final[i] + kHalf22) >> 22);
			}
		}
	}
}

template<typename Component>
void Filters<Component>::bilinearTwoLines(uint8_t* dstRow, const uint8_t* srcRow0, const uint8_t* srcRow1, uint16_t coeff0, uint16_t coeff1, uint32_t length)
{
	for(uint32_t col = 0; col < length; col++) {
		for(uint32_t channel = 0; channel < COMPONENT_SIZE; channel++) {
			uint16_t index = col * COMPONENT_SIZE + channel;
			dstRow[index] = (uint8_t)(((coeff0 * srcRow0[index] + coeff1 * srcRow1[index])) >> 8);
		}
	}
}

template<typename Component>
void Filters<Component>::adaptiveSeparable2x2(const FilterKernelAdaptive* kernelX, const FilterKernelAdaptive* kernelY, const uint8_t* __restrict inputBuffer, unsigned int inputWidth, unsigned int inputHeight, unsigned int inputPitch, uint8_t* __restrict outputBuffer, unsigned int outputWidth, unsigned int outputHeight, unsigned int outputPitch, unsigned int outputCapacity)
{
	__restrict uint8_t* kernelTableX = kernelX->getTableFixedPointBilinear();
	__restrict uint8_t* kernelTableY = kernelY->getTableFixedPointBilinear();

	ImagePlane<COMPONENT_SIZE>* horizontal = ImagePlane<COMPONENT_SIZE>::create(inputWidth, outputHeight, 0, 4);
	uint32_t horizontalPitch;
	uint8_t* horizontalData = horizontal->lockRect(inputWidth, outputHeight, horizontalPitch);

	for(uint32_t row = 0; row < outputHeight; row++) {
		int32_t startY0 = kernelY->computeSampleStart(row);
		int32_t startY1 = startY0 + 1;
		startY1 = min(startY1, (int32_t)inputHeight - 1);
		uint32_t filterIndexY = row * 2;
		uint16_t coeffsY0 = *(kernelTableY + filterIndexY + 0);
		uint16_t coeffsY1 = *(kernelTableY + filterIndexY + 1);
		const uint8_t* srcRow0 = inputBuffer + inputPitch * startY0;
		const uint8_t* srcRow1 = inputBuffer + inputPitch * startY1;
		uint8_t* dstRow0 = horizontalData + row * horizontalPitch;

		// compute horizontal convolution
		bilinearTwoLines(dstRow0, srcRow0, srcRow1, coeffsY0, coeffsY1, inputWidth);
	}

	ImagePlane<COMPONENT_SIZE>* horizontalTransposed = ImagePlane<COMPONENT_SIZE>::create(outputHeight, inputWidth, 0, 4);
	uint32_t horizontalTransposedPitch;
	uint8_t* horizontalTransposedData = horizontalTransposed->lockRect(outputHeight, inputWidth, horizontalTransposedPitch);

	horizontal->transpose(horizontalTransposed);

	for(uint32_t row = 0; row < outputWidth; row++) {
		int32_t startX0 = kernelX->computeSampleStart(row);
		int32_t startX1 = startX0 + 1;
		startX1 = min(startX1, (int32_t)inputWidth - 1);
		uint32_t filterIndexX = row * 2;
		uint16_t coeffsX0 = *(kernelTableX + filterIndexX + 0);
		uint16_t coeffsX1 = *(kernelTableX + filterIndexX + 1);
		uint8_t* srcRow0 = horizontalTransposedData + horizontalTransposedPitch * startX0;
		uint8_t* srcRow1 = horizontalTransposedData + horizontalTransposedPitch * startX1;
		uint8_t* dstRow0 = outputBuffer + row * outputPitch;

		// compute horizontal convolution
		bilinearTwoLines(dstRow0, srcRow0, srcRow1, coeffsX0, coeffsX1, outputHeight);
	}
	delete horizontal;
	delete horizontalTransposed;
}

template<typename Component>
void Filters<Component>::rotateLeft(const uint8_t* __restrict input_buffer, uint8_t* __restrict output_buffer, unsigned int width, unsigned int height, unsigned int input_pitch, unsigned int output_pitch, unsigned int output_capacity)
{
	SECURE_ASSERT(SafeUMul3(width, height, COMPONENT_SIZE) <= output_capacity);
	input_pitch /= COMPONENT_SIZE;
	output_pitch /= COMPONENT_SIZE;
	unsigned int output_width = height;
	unsigned int output_height = width;
	// Start at the top right of the input.
	Component* input_sample = (Component*)input_buffer + (width - 1);
	Component* output_sample = (Component*)output_buffer;
	int input_step = input_pitch;
	// Write to the output left -> right, but walking the image top -> bottom.
	for( unsigned int y = 0; y < output_height; y++ ) {
		for( unsigned int x = 0; x < output_width; x++ ) {
			*output_sample = *input_sample;
			input_sample += input_step;
			output_sample++;
		}
		// Skip past any extra junk, to the next row.
		output_sample += (output_pitch - output_width);
		// Back to the top of the input, but one pixel to the left.
		input_sample -= 1 + (input_pitch * height);
	};
}

template<typename Component>
void Filters<Component>::rotateRight(const uint8_t* __restrict input_buffer, uint8_t* __restrict output_buffer, unsigned int width, unsigned int height, unsigned int input_pitch, unsigned int output_pitch, unsigned int output_capacity)
{
	SECURE_ASSERT(SafeUMul3(width, height, COMPONENT_SIZE) <= output_capacity);
	input_pitch /= COMPONENT_SIZE;
	output_pitch /= COMPONENT_SIZE;
	unsigned int output_width = height;
	unsigned int output_height = width;
	// Start at the bottom left of the input.
	Component* input_sample = (Component*)input_buffer + (height - 1) * input_pitch;
	Component* output_sample = (Component*)output_buffer;
	int input_step = input_pitch;
	// Write to the output left -> right, but walking the input image bottom -> top.
	for( unsigned int y = 0; y < output_height; y++ ) {
		for( unsigned int x = 0; x < output_width; x++ ) {
			*output_sample = *input_sample;
			input_sample -= input_step;
			output_sample++;
		}
		// Skip past any extra junk, to the next row.
		output_sample += (output_pitch - output_width);
		// Back to the bottom of the input, but one pixel to the right.
		input_sample += 1 + (input_pitch * height);
	};
}

template<typename Component>
void Filters<Component>::rotateUp(const uint8_t* __restrict input_buffer, uint8_t* __restrict output_buffer, unsigned int width, unsigned int height, unsigned int input_pitch, unsigned int output_pitch, unsigned int output_capacity)
{
	SECURE_ASSERT(SafeUMul3(width, height, COMPONENT_SIZE) <= output_capacity);
	input_pitch /= COMPONENT_SIZE;
	output_pitch /= COMPONENT_SIZE;
	unsigned int output_width = width;
	unsigned int output_height = height;
	// Start at the bottom right of the input image.
	Component* input_sample = (Component*)input_buffer + (height - 1) * input_pitch + width - 1;
	Component* output_sample = (Component*)output_buffer;
	// Write to the output left -> right, but walking the image right -> left.
	for( unsigned int y = 0; y < output_height; y++ ) {
		for( unsigned int x = 0; x < output_width; x++ ) {
			*output_sample = *input_sample;
			input_sample--;
			output_sample++;
		}
		// Skip past any extra junk, to the next row.
		output_sample += (output_pitch - output_width);
		input_sample -= (input_pitch - width);
	};
}

template<typename Component>
void Filters<Component>::transpose(const uint8_t* __restrict input_buffer, uint8_t* __restrict output_buffer, unsigned int width, unsigned int height, unsigned int input_pitch, unsigned int output_pitch,  unsigned int output_capacity)
{
	if((width > 0) && (height > 0)) {
		SECURE_ASSERT(SafeUMul3(width, height, COMPONENT_SIZE) <= output_capacity);
		uint32_t normalized_output_pitch = output_pitch / COMPONENT_SIZE;
		for(uint32_t y = 0; y < height; y++) {
			typename Component::primType::asType* srcRow = (typename Component::primType::asType*)(input_buffer + y * input_pitch);
			typename Component::primType::asType* dstCol = (typename Component::primType::asType*)(output_buffer + y * COMPONENT_SIZE);
			for(uint32_t x = 0; x < width; x++) {
				*dstCol = *srcRow++;
				dstCol += normalized_output_pitch;
			}
		}
	}
}

template<typename Component>
bool Filters<Component>::fasterUnpadded(uint32_t kernelSize)
{
	return false;
}

template<typename Component>
bool Filters<Component>::supportsUnpadded(uint32_t kernelSize)
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
