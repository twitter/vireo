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

#include "register.h"

namespace imagecore {

// This is only necessary when linked as a static library, otherwise all compiled readers register automatically.

#define FORCE_LOAD_READER(c) { extern int __register##c; registered += __register##c; }
#define FORCE_LOAD_WRITER(c) { extern int __register##c; registered += __register##c; }

int registerDefaultImageReaders()
{
	int registered = 0;
#if IMAGECORE_WITH_JPEG
	FORCE_LOAD_READER(ImageReaderJPEG);
#endif
#if IMAGECORE_WITH_PNG
	FORCE_LOAD_READER(ImageReaderPNG);
#endif
#if IMAGECORE_WITH_GIF
	FORCE_LOAD_READER(ImageReaderGIF);
#endif
#if IMAGECORE_WITH_BMP
	FORCE_LOAD_READER(ImageReaderBMP);
#endif
#if IMAGECORE_WITH_TIFF
	FORCE_LOAD_READER(ImageReaderTIFF);
#endif
#if IMAGECORE_WITH_WEBP
	FORCE_LOAD_READER(ImageReaderWebP);
#endif
	return registered;
}

int registerDefaultImageWriters()
{
	int registered = 0;
#if IMAGECORE_WITH_JPEG
	FORCE_LOAD_WRITER(ImageWriterJPEG);
#endif
#if IMAGECORE_WITH_PNG
	FORCE_LOAD_WRITER(ImageWriterPNG);
#endif
	FORCE_LOAD_WRITER(ImageWriterRAW);
	return registered;
}

}
