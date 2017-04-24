//
//  platform_support.cpp
//  ImageCore
//
//  Created by Paul Melamed on 4/14/15.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

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



