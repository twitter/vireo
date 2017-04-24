//
//  tiff.h
//  ImageCore
//
//  Created by Luke Alonso on 6/25/14.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "imagecore/formats/reader.h"
#include "imagecore/formats/writer.h"
#include "register.h"
#include "tiffio.h"

namespace imagecore {

class ImageReaderTIFF : public ImageReader
{
public:
	DECLARE_IMAGE_READER(ImageReaderTIFF);

	virtual bool initWithStorage(Storage* source);
	virtual bool readHeader();
	virtual bool readImage(Image* destImage);
	virtual EImageFormat getFormat();
	virtual const char* getFormatName();
	virtual unsigned int getWidth();
	virtual unsigned int getHeight();
	virtual EImageColorModel getNativeColorModel();

private:
	Storage* m_Source;
	Storage* m_TempSource;
	ImageWriter::Storage* m_TempStorage;
	unsigned int m_Width;
	unsigned int m_Height;
	bool m_HasAlpha;
	TIFF* m_Tiff;
};

}
