//
//  register.cpp
//  ImageCore
//
//  Created by Luke Alonso on 6/27/14.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

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
