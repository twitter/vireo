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
