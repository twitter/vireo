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
