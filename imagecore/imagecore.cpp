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

static ImageCoreAssertionHandler s_AssertionHandler;

static void DefaultAssertionHandler(int code, const char* message, const char* file, int line)
{
	fprintf(stderr, "assertion failed: %s (%s:%i)", message, file, line);
#if DEBUG
	__builtin_trap();
#else
	exit(code);
#endif
}

void ImageCoreAssert(int code, const char* message, const char* file, int line)
{
	if( s_AssertionHandler == NULL ) {
		DefaultAssertionHandler(code, message, file, line);
	} else {
		s_AssertionHandler(code, message, file, line);
	}
}

void RegisterImageCoreAssertionHandler(ImageCoreAssertionHandler handler)
{
	s_AssertionHandler = handler;
}

#if __APPLE__

#include <mach/mach_time.h>

double ImageCoreGetTimeMs()
{
	static mach_timebase_info_data_t sTimebaseInfo;
	if ( sTimebaseInfo.denom == 0 ) {
		(void) mach_timebase_info(&sTimebaseInfo);
	}
	return (double)((mach_absolute_time() * sTimebaseInfo.numer / sTimebaseInfo.denom) / 1000) / 1000.0;
}

#else

double ImageCoreGetTimeMs()
{
	return (double)clock() / (double)(CLOCKS_PER_SEC / 1000);
}

#endif

