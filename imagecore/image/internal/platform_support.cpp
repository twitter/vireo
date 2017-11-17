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

#include "platform_support.h"

#if IMAGECORE_DETECT_SSE && __SSE4_1__

#include "cpuid.h"

static unsigned int checkCPUFeatures() {
	unsigned int eax = 0;
	unsigned int ebx = 0;
	unsigned int ecx = 0;
	unsigned int edx = 0;
	unsigned int features = 0;
	__get_cpuid(1, &eax, &ebx, &ecx, &edx);
	if( (edx & (1 << 25)) != 0 ) {
		features |= kCPUFeature_SSE;
	}
	if( (edx & (1 << 26)) != 0 ) {
		features |= kCPUFeature_SSE2;
	}
	if( (ecx & (1 << 0)) != 0 ) {
		features |= kCPUFeature_SSE3;
	}
	if( (ecx & (1 << 9)) != 0 ) {
		features |= kCPUFeature_SSE3_S;
	}
	if( (ecx & (1 << 19)) != 0 ) {
		features |= kCPUFeature_SSE4_1;
	}
	if( (ecx & (1 << 20)) != 0 ) {
		features |= kCPUFeature_SSE4_2;
	}
	/*
	 // AVX currently unused.
	 if( (ecx & (1 << 28)) != 0 && (ecx & (1 << 27)) != 0 && (ecx & (1 << 26)) != 0 ) {
	 __asm__ __volatile__
	 (".byte 0x0f, 0x01, 0xd0": "=a" (eax), "=d" (edx) : "c" (0) : "cc");
	 if( (eax & 6) == 6 ) {
	 features |= kCPUFeature_AVX;
	 }
	 }
	 */
	return features;
}

static unsigned int sCPUFeatures = checkCPUFeatures();

static bool haveCPUFeature(ECPUFeature feature) {
	return (sCPUFeatures & feature) != 0;
}

bool checkForCPUSupport(ECPUFeature feature)
{
	return haveCPUFeature(feature);

}
#else
bool checkForCPUSupport(ECPUFeature feature)
{
	return true;

}
#endif



