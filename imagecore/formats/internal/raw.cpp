//
//  raw.cpp
//  ImageTool
//
//  Created by Luke Alonso on 01/15/14.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#include "raw.h"
#include "imagecore/image/interleaved.h"

namespace imagecore {

REGISTER_IMAGE_WRITER(ImageWriterRAW);

bool ImageWriterRAW::Factory::matchesExtension(const char *extension)
{
	return strcasecmp(extension, "raw") == 0 || strcasecmp(extension, "bin") == 0;
}

bool ImageWriterRAW::Factory::appropriateForInputFormat(EImageFormat format)
{
	return false;
}

bool ImageWriterRAW::Factory::supportsInputColorModel(EImageColorModel colorModel)
{
	return Image::colorModelIsInterleaved(colorModel);
}

EImageFormat ImageWriterRAW::Factory::getFormat()
{
	return kImageFormat_RAW;
}

ImageWriterRAW::ImageWriterRAW()
:	m_Storage(NULL)
{
}

ImageWriterRAW::~ImageWriterRAW()
{
}

bool ImageWriterRAW::initWithStorage(ImageWriter::Storage* output)
{
	m_Storage = output;
	return true;
}

bool ImageWriterRAW::beginWrite(unsigned int width, unsigned int height, EImageColorModel colorModel)
{
	return true;
}

unsigned int ImageWriterRAW::writeRows(Image* source, unsigned int sourceRow, unsigned int numRows)
{
	if( Image::colorModelIsInterleaved(source->getColorModel()) ) {
		ImageInterleaved* sourceImage = source->asInterleaved();
		unsigned int sourcePitch = sourceImage->getPitch();
		uint8_t* sourceBuffer = (uint8_t*)sourceImage->getBytes();
		unsigned int finalRow = sourceRow + numRows;
		unsigned int rowSize = sourceImage->getWidth() * sourceImage->getComponentSize();
		for( unsigned int y = sourceRow; y < finalRow; y++ ) {
			m_Storage->write(sourceBuffer + sourcePitch * y, rowSize);
		}
	}
	return numRows;
}

bool ImageWriterRAW::endWrite()
{
	return true;
}

bool ImageWriterRAW::writeImage(Image* sourceImage)
{
	unsigned int sourceWidth = sourceImage->getWidth();
	unsigned int sourceHeight = sourceImage->getHeight();
	if( !beginWrite(sourceWidth, sourceHeight, sourceImage->getColorModel()) ) {
		return false;
	}
	if( writeRows(sourceImage, 0, sourceHeight) != sourceHeight ) {
		return false;
	}
	if( !endWrite() ) {
		return false;
	}
	return true;
}

}
