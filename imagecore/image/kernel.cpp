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

#include "imagecore/imagecore.h"
#include "imagecore/utils/mathutils.h"
#include "imagecore/utils/securemath.h"
#include "imagecore/image/kernel.h"

// From GPU Gems 3.
float mitchell_netravali(float x, float B, float C)
{
	float ax = fabs(x);
	if (ax < 1) {
		return ((12 - 9 * B - 6 * C) * ax * ax * ax + (-18 + 12 * B + 6 * C) * ax * ax + (6 - 2 * B)) / 6;
	} else if ((ax >= 1) && (ax < 2)) {
		return ((-B - 6 * C) * ax * ax * ax + (6 * B + 30 * C) * ax * ax + (-12 * B - 48 * C) * ax + (8 * B + 24 * C)) / 6;
	} else {
		return 0;
	}
}

// From Numerical Recipes in C: The Art of Scientific Computing
// Zeroth order modified bessel function of the first kind.
float bessel_i0(float x)
{
	float ax,ans;
	double y;
	if ((ax=fabs(x)) < 3.75) {
		y=x/3.75;
		y*=y;
		ans=1.0+y*(3.5156229+y*(3.0899424+y*(1.2067492
											 +y*(0.2659732+y*(0.360768e-1+y*0.45813e-2)))));
	} else {
		y=3.75/ax;
		ans=(exp(ax)/sqrt(ax))*(0.39894228+y*(0.1328592e-1
											  +y*(0.225319e-2+y*(-0.157565e-2+y*(0.916281e-2
																				 +y*(-0.2057706e-1+y*(0.2635537e-1+y*(-0.1647633e-1
																												+y*0.392377e-2))))))));
	}
	return ans;
}

const float KaiserAlpha = 7.0f;
const float KaiserInvI0Alpha = 1.0f / bessel_i0(KaiserAlpha);

float kaiser(const float x, const float w)
{
	double xSq = x * x;
	if( xSq < 1.0 ) {
		return KaiserInvI0Alpha * bessel_i0(KaiserAlpha * sqrt(1.0 - xSq));
	}
	return 0.0f;
}

float sinc(const float x)
{
	if( x == 0.0f ) {
		return 1.0f;
	}
	double xpi = x * M_PI;
	// Implementations of sinf() can vary by several ULP, enough to end up giving different
	// 16.16 fixed point kernel coefficients. Use sin() instead, which should be more consistent
	// across systems throughout the range of precision we require.
	return (float)(sin(xpi) / xpi);
}

float lanczos(const float x, const float invSize)
{
	return sinc(x) * sinc(x * invSize);
}

#define SHARPEN_COS 0
#define SHARPEN_X4 0
#define SHARPEN_MUL 1

float lanczos_sharpen(const float x, float weight)
{
#if SHARPEN_COS
	weight *= (1.4f+(cosf(1.4f*x*x+M_PI)*0.4f));
#elif SHARPEN_X4
	weight = sinc(x) * sinc(powf(x/3.0f, 4.0f));
#elif SHARPEN_MUL
	// Not continuous.
	if (weight < 0) {
		weight *= 1.30f;
	}
#endif
	return weight;
}

float lanczos_sharper(const float x, const float invSize)
{
	float weight = lanczos(x, invSize);
	return lanczos_sharpen(x, weight);
}

float lanczos_fixed(const float x, const float invSize)
{
	double px = fmax(M_PI * x, 0.000001);
	float f = sin(px) * sin(px * 0.5) / (px * px);
	return f;
}

float lanczos_fixed_sharper(const float x, const float invSize)
{
	float weight = lanczos_fixed(x, invSize);
	return lanczos_sharpen(x, weight);
}

float mitchell(const float x, const float invSize)
{
	return mitchell_netravali(x, 0.33333f, 0.33333f);
}

typedef float (*FilterFunction)(const float, const float);
FilterFunction s_FilterFunctionsAdaptive[kFilterType_MAX] = { lanczos, lanczos_sharper, mitchell, kaiser };
FilterFunction s_FilterFunctionsFixed[kFilterType_MAX] = { lanczos_fixed, lanczos_fixed_sharper, mitchell, kaiser };

FilterKernel::FilterKernel()
{
	m_InSampleOffset = 0;
	m_OutSampleOffset = 0;
	m_SampleRatio = 1.0f;
	m_Table = NULL;
	m_TableBilinear = NULL;
	m_TableFixedPoint = NULL;
	m_TableFixedPoint4 = NULL;
	m_TableSize = 0;
	m_KernelSize = 0;
	m_MaxSamples = 0;
}

void FilterKernel::generateFixedPoint(EFilterType type)
{
	unsigned int tableSize = SafeUMul(m_TableSize, m_KernelSize);
	if(type == kFilterType_Linear) {
		m_TableBilinear = new uint8_t[tableSize];
		// Convert to fixed point.
		for( unsigned int i = 0; i < tableSize; i++ ) {
			m_TableBilinear[i] = (int)(m_Table[i] * 255.0f);
		}
	} else {
		m_TableFixedPoint = new int32_t[tableSize];
		m_TableFixedPoint4 = new int32_t[SafeUMul(tableSize, 4U)];
		// Convert to fixed point.
		// This is duplicated 4 times to save a few SSE instructions.
		for( unsigned int i = 0; i < tableSize; i++ ) {
			int value = (int)(m_Table[i] * 65536.0f);
			m_TableFixedPoint[i] = value;
			m_TableFixedPoint4[i * 4 + 0] = value;
			m_TableFixedPoint4[i * 4 + 1] = value;
			m_TableFixedPoint4[i * 4 + 2] = value;
			m_TableFixedPoint4[i * 4 + 3] = value;
		}
	}
}

void FilterKernel::setSampleOffset(unsigned int inSampleOffset, unsigned int outSampleOffset)
{
	SECURE_ASSERT(m_OutSampleOffset < m_TableSize);
	m_InSampleOffset = inSampleOffset;
	m_OutSampleOffset = outSampleOffset;
}


FilterKernel::~FilterKernel()
{
	delete[] m_Table;
	delete[] m_TableBilinear;
	delete[] m_TableFixedPoint;
	delete[] m_TableFixedPoint4;
}

// Uses a dynamic number of taps per sample, based on on the filter window size and the scaling factor.
FilterKernelAdaptive::FilterKernelAdaptive(EFilterType type, unsigned int kernelSize, unsigned int inSize, unsigned int outSize)
:	FilterKernel()
{
	float ratio = (float)outSize / (float)inSize;
	float invRatio = (float)inSize / (float)outSize;
	m_SampleRatio = invRatio;
	unsigned int tableAllocSize = SafeUMul(outSize, kernelSize);
	m_Table = new float[tableAllocSize];
	m_TableSize = outSize;
	m_KernelSize = kernelSize;
	float maxWindowSize = (kernelSize * 0.5) - 0.00001f;
	float windowWidth = (fmin(maxWindowSize, kernelSize * 0.25f * invRatio));
	float invFilterScale = 1.0f / (kernelSize * 0.25f);
	m_WindowWidth = windowWidth;
	FilterFunction filterFunction = s_FilterFunctionsAdaptive[type];
	int maxSamples = 0;
	if (type == kFilterType_Linear) {
		for( unsigned int i = 0; i < outSize; i++ ) {
			float sample = (i + 0.5f) * invRatio;
			float w0 = sample - floorf(sample);
			m_Table[i * kernelSize + 0] = 1.0f - w0;
			m_Table[i * kernelSize + 1] = w0;
		}
		m_WindowWidth = 0.5f;
		maxSamples = 2;
	} else {
		for( unsigned int i = 0; i < outSize; i++ ) {
			float sample = (i + 0.5f) * invRatio - 0.00001f;
			int start = sample - windowWidth + 0.5f;
			int end = sample + windowWidth + 0.5f;
			int numSamples = end - start;
			maxSamples = max(maxSamples, numSamples);
			float sum = 0.0f;
			for( int k = 0; k < numSamples; k++ ) {
				float samplePos = ratio * ((float)(start + k) - sample + 0.5f);
				float weight = filterFunction(samplePos, invFilterScale);
				m_Table[i * kernelSize + k] = weight;
				sum += weight;
			}
			for( unsigned int k = numSamples; k < kernelSize; k++ ) {
				m_Table[i * kernelSize + k] = 0.0f;
			}
			float invSum = 1.0f / sum;
			for (int k = 0; k < numSamples; k++) {
				m_Table[i * kernelSize + k] *= invSum;
			}
		}
	}
	m_MaxSamples = maxSamples;

	generateFixedPoint(type);
}

// Always takes 4 fixed samples, regardless of the scaling factor.
FilterKernelFixed::FilterKernelFixed(EFilterType type, unsigned int inSize, unsigned int outSize)
:	FilterKernel()
{
	m_Table = new float[outSize * 4];
	m_TableSize = outSize;
	m_KernelSize = 4;
	m_WindowWidth = 4;
	FilterFunction filterFunction = s_FilterFunctionsFixed[type];
	m_SampleRatio = (float)inSize / (float)outSize;
	for( unsigned int i = 0; i < outSize; i++ ) {
		float inP = fmaxf(0.0f, ((float)(i + 0.5f) * m_SampleRatio) - 0.5f);
		float frP = inP - floorf(inP);
		float a = filterFunction(frP + 1.0f, 4);
		float b = filterFunction(frP, 4);
		float c = filterFunction(1.0f - frP, 4);
		float d = filterFunction(2.0f - frP, 4);
		float s = 1.0f / (a + b + c + d);
		m_Table[i * 4 + 0] = a * s;
		m_Table[i * 4 + 1] = b * s;
		m_Table[i * 4 + 2] = c * s;
		m_Table[i * 4 + 3] = d * s;
	}
	generateFixedPoint(type);
}
