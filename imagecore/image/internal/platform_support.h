//
//  platform_support.h
//  ImageCore
//
//  Created by Paul Melamed on 4/14/15.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

enum ECPUFeature
{
	kCPUFeature_SSE     = 0x01,
	kCPUFeature_SSE2    = 0x02,
	kCPUFeature_SSE3    = 0x04,
	kCPUFeature_SSE3_S  = 0x08,
	kCPUFeature_SSE4_1  = 0x10,
	kCPUFeature_SSE4_2  = 0x20,
	kCPUFeature_AVX     = 0x40
};


bool checkForCPUSupport(ECPUFeature feature);
