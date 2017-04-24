//
//  imagecore.h
//  ImageCore
//
//  Created by Luke Alonso on 1/24/14.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#if _WIN32
#define _USE_MATH_DEFINES
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <time.h>
#include <cstdint>

#ifdef __APPLE__
#define memalign(a, s) malloc(s)
#else
#include <malloc.h>
#endif

#define IMAGECORE_EXPORT __attribute__ ((visibility ("default")))

typedef void (*ImageCoreAssertionHandler)(int code, const char* message, const char* file, int line);

IMAGECORE_EXPORT void ImageCoreAssert(int code, const char* message, const char* file, int line);
IMAGECORE_EXPORT void RegisterImageCoreAssertionHandler(ImageCoreAssertionHandler handler);

#define ASSERT(x) { if( !(x) ) { ImageCoreAssert(IMAGECORE_ASSERTION_FAILED,  #x, __FILE__, __LINE__); } }
// Never disable this assertion macro, most of the security checks in the program are wrapped with this.
#define SECURE_ASSERT(x) { if( !(x) ) { ImageCoreAssert(IMAGECORE_SECURITY_ASSERTION, #x, __FILE__, __LINE__); } }

#define IMAGECORE_UNUSED(x) (void)(x)

#if PRINT_TIMINGS
double ImageCoreGetTimeMs();
#define START_CLOCK(s) double s##start = ImageCoreGetTimeMs();
#if ANDROID
#define END_CLOCK(s) { __android_log_print(ANDROID_LOG_INFO, "libtwittermedia", "%s: %.2fms\n", #s, ImageCoreGetTimeMs() - s##start); }
#else
#define END_CLOCK(s) { printf("%s: %.2fms\n", #s, ImageCoreGetTimeMs() - s##start); }
#endif
#else
#define START_CLOCK(s) { }
#define END_CLOCK(s) { }
#endif

// Error codes

#define IMAGECORE_SUCCESS               0
#define IMAGECORE_UNKNOWN_ERROR         1
#define IMAGECORE_OUT_OF_MEMORY	        2
#define IMAGECORE_INVALID_USAGE         3
#define IMAGECORE_INVALID_FORMAT        4
#define IMAGECORE_READ_ERROR            5
#define IMAGECORE_WRITE_ERROR           6
#define IMAGECORE_INVALID_IMAGE_SIZE    7
#define IMAGECORE_INVALID_OUTPUT_SIZE   8
#define IMAGECORE_INTEGER_OVERFLOW      9
#define IMAGECORE_SECURITY_ASSERTION    10
#define IMAGECORE_INVALID_OPERATION     11
#define IMAGECORE_ASSERTION_FAILED      12
#define IMAGECORE_INVALID_COLOR         13
