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

#include "imagecore/utils/mathutils.h"
#include "imagecore/image/yuv_semiplanar.h"
#include "imagecore/image/rgba.h"
#include "imagecore/formats/writer.h"
#include "imagecore/image/internal/conversions.h"

namespace imagecore {

static unsigned int computeSize(unsigned int in)
{
	return (in + 1) / 2;
}

ImageYUVSemiplanar* ImageYUVSemiplanar::create(ImagePlane8* planeY, ImagePlane16* planeUV)
{
	if( planeY != NULL && planeUV != NULL) {
		ImageYUVSemiplanar* image = new ImageYUVSemiplanar(planeY, planeUV);
		if( image != NULL ) {
			return image;
		}
	}
	return NULL;
}

ImageYUVSemiplanar* ImageYUVSemiplanar::create(unsigned int width, unsigned int height)
{
	return ImageYUVSemiplanar::create(width, height, 16, 16);
}

ImageYUVSemiplanar* ImageYUVSemiplanar::create(unsigned int width, unsigned int height, unsigned int padding, unsigned int alignment)
{
	ImagePlane8* planeY = ImagePlane8::create(width, height, padding, alignment);
	ImagePlane16* planeUV = ImagePlane16::create(computeSize(width), computeSize(height), padding, alignment);
	if( planeY != NULL && planeUV != NULL) {
		ImageYUVSemiplanar* image = new ImageYUVSemiplanar(planeY, planeUV);
		if( image != NULL ) {
			return image;
		}
	}
	delete planeY;
	delete planeUV;
	return NULL;
}

ImageYUVSemiplanar* ImageYUVSemiplanar::create(ImagePlane8* planeY, ImagePlane16* planeUV, ImageRGBA* inImage)
{
	uint32_t width = inImage->getWidth();
	uint32_t height = inImage->getHeight();
	uint32_t outputPitchY;
	uint8_t* dstY = planeY->lockRect(planeY->getWidth(), planeY->getHeight(), outputPitchY);
	uint32_t outputPitchUV;
	uint8_t* dstUV = planeUV->lockRect(planeUV->getWidth(), planeUV->getHeight(), outputPitchUV);
	uint32_t inputPitch;

	const uint8_t* srcRGBA = inImage->lockRect(width, height, inputPitch);
	Conversions<false>::rgba_to_yuv420(dstY, dstUV, srcRGBA, width, height, inputPitch, outputPitchY, outputPitchUV);

	ImageYUVSemiplanar* image = new ImageYUVSemiplanar(planeY, planeUV);
	if( image != NULL ) {
		return image;
	}
	return NULL;
}

ImageYUVSemiplanar* ImageYUVSemiplanar::create(ImageRGBA* inImage, unsigned int padding, unsigned int alignment)
{
	uint32_t width = inImage->getWidth();
	uint32_t height = inImage->getHeight();
	ImagePlane8* planeY = ImagePlane8::create(width, height, padding, alignment);
	ImagePlane16* planeUV = ImagePlane16::create(computeSize(width), computeSize(height), padding, alignment);
	if( planeY != NULL && planeUV != NULL) {
		ImageYUVSemiplanar* image = new ImageYUVSemiplanar(planeY, planeUV);
		if( image != NULL ) {
			uint32_t outputPitchY;
			uint8_t* dstY = planeY->lockRect(planeY->getWidth(), planeY->getHeight(), outputPitchY);
			uint32_t outputPitchUV;
			uint8_t* dstUV = planeUV->lockRect(planeUV->getWidth(), planeUV->getHeight(), outputPitchUV);
			uint32_t inputPitch;
			const uint8_t* srcRGBA = inImage->lockRect(width, height, inputPitch);
			Conversions<false>::rgba_to_yuv420(dstY, dstUV, srcRGBA, width, height, inputPitch, outputPitchY, outputPitchUV);
			return image;
		}
	}
	delete planeY;
	delete planeUV;
	return NULL;
}

ImageYUVSemiplanar::ImageYUVSemiplanar(ImagePlane8* planeY, ImagePlane16* planeUV)
:	m_PlaneY(planeY)
,	m_PlaneUV(planeUV)
,	m_Range(kYUVRange_Unknown)
,	m_OwnsPlanes(true)
{
}

ImageYUVSemiplanar::~ImageYUVSemiplanar()
{
	if (m_OwnsPlanes) {
		delete m_PlaneY;
		delete m_PlaneUV;
	}
	m_PlaneY = NULL;
	m_PlaneUV = NULL;
}

void ImageYUVSemiplanar::setDimensions(unsigned int width, unsigned int height)
{
	m_PlaneY->setDimensions(width, height);
	m_PlaneUV->setDimensions(computeSize(width), computeSize(height));
}

void ImageYUVSemiplanar::setDimensions(unsigned int width, unsigned int height, unsigned int padding, unsigned int alignment)
{
	m_PlaneY->setDimensions(width, height, padding, alignment);
	m_PlaneUV->setDimensions(computeSize(width), computeSize(height), padding, alignment);
}

void ImageYUVSemiplanar::setPadding(unsigned int padding)
{
	m_PlaneY->setPadding(padding);
	m_PlaneUV->setPadding(padding);
}

bool ImageYUVSemiplanar::resize(Image* dest, EResizeQuality quality)
{
	ImageYUVSemiplanar* destYUVSemiplanar = dest->asYUVSemiplanar();
	if( destYUVSemiplanar ) {
		if( m_PlaneY->resize(destYUVSemiplanar->getPlaneY(), quality) ) {
			if( m_PlaneUV->resize(destYUVSemiplanar->getPlaneUV(), quality) ) {
				return true;
			}
		}
	}
	return false;
}

void ImageYUVSemiplanar::reduceHalf(Image* dest)
{
	ImageYUVSemiplanar* destYUVSemiplanar = dest->asYUVSemiplanar();
	if( destYUVSemiplanar ) {
		m_PlaneY->reduceHalf(destYUVSemiplanar->getPlaneY());
		if( (m_PlaneUV->getWidth() & 1) == 1 ) {
			destYUVSemiplanar->getPlaneUV()->setDimensions(computeSize(dest->getWidth()), computeSize(dest->getHeight()));
			m_PlaneUV->resize(destYUVSemiplanar->getPlaneUV(), kResizeQuality_High);
		} else {
			m_PlaneUV->reduceHalf(destYUVSemiplanar->getPlaneUV());
		}
	}
}

bool ImageYUVSemiplanar::crop(const ImageRegion& boundingBox)
{
	if( boundingBox.right() <= getWidth() && boundingBox.bottom() <= getHeight() ) {
		ImageRegion boxY = boundingBox;
		// Since the U/V planes are half the size, we can only crop on even pixel boundaries.
		if( (boxY.left() & 1) == 1 ) {
			boxY.left(boxY.left() - 1);
		}
		if( (boxY.top() & 1) == 1 ) {
			boxY.top(boxY.top() - 1);
		}
		ImageRegion boxUV = boxY;
		boxUV.left(boxUV.left() / 2);
		boxUV.top(boxUV.top() / 2);
		boxUV.width(computeSize(boxUV.width()));
		boxUV.height(computeSize(boxUV.height()));
		m_PlaneY->crop(boxY);
		m_PlaneUV->crop(boxUV);
		return true;
	}
	return false;
}

void ImageYUVSemiplanar::rotate(Image* dest, EImageOrientation direction)
{
	ImageYUVSemiplanar* destYUVSemiplanar = dest->asYUVSemiplanar();
	if( destYUVSemiplanar ) {
		m_PlaneY->rotate(destYUVSemiplanar->getPlaneY(), direction);
		m_PlaneUV->rotate(destYUVSemiplanar->getPlaneUV(), direction);
	}
}

void ImageYUVSemiplanar::fillPadding()
{
	m_PlaneY->fillPadding();
	m_PlaneUV->fillPadding();
}

void ImageYUVSemiplanar::copyRect(Image* dest, unsigned int sourceX, unsigned int sourceY, unsigned int destX, unsigned int destY, unsigned int width, unsigned int height)
{
	ASSERT(0);
}

void ImageYUVSemiplanar::clearRect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	ASSERT(0);
}

void ImageYUVSemiplanar::copy(Image* dest)
{
	ImageYUVSemiplanar* destYUVSemiplanar = dest->asYUVSemiplanar();
	if( destYUVSemiplanar ) {
		m_PlaneY->copy(destYUVSemiplanar->getPlaneY());
		m_PlaneUV->copy(destYUVSemiplanar->getPlaneUV());
	}
}

ImageYUVSemiplanar* ImageYUVSemiplanar::move()
{
	ImageYUVSemiplanar* image = new ImageYUVSemiplanar(m_PlaneY, m_PlaneUV);
	m_OwnsPlanes = false;
	return image;
}

void ImageYUVSemiplanar::applyLookupTable(ImageYUVSemiplanar* destImage, uint8_t* tableY, uint8_t* tableUV)
{
	destImage->setDimensions(m_PlaneY->getWidth(), m_PlaneY->getHeight());
	unsigned int widthY = m_PlaneY->getWidth();
	unsigned int heightY = m_PlaneY->getHeight();
	unsigned int pitchY = m_PlaneY->getPitch();
	unsigned int destPitchY = m_PlaneY->getPitch();
	const uint8_t* bufferY = getPlaneY()->getBytes();
	uint8_t* destBufferY = destImage->getPlaneY()->lockRect(widthY, heightY, destPitchY);

	if( destPitchY == pitchY ) {
		for( unsigned int y = 0; y < heightY; ++y ) {
			for( unsigned int x = 0; x < widthY; ++x ) {
				unsigned int offset = (y * pitchY) + x;
				destBufferY[offset] = tableY[bufferY[offset]];
			}
		}
	} else {
		for( unsigned int y = 0; y < heightY; ++y ) {
			for( unsigned int x = 0; x < widthY; ++x ) {
				destBufferY[(y * destPitchY) + x] = tableY[bufferY[(y * pitchY) + x]];
			}
		}
	}

	unsigned int widthUV = m_PlaneUV->getWidth();
	unsigned int heightUV = m_PlaneUV->getHeight();
	unsigned int pitchUV = m_PlaneUV->getPitch();
	unsigned int destPitchUV = 0;
	const uint8_t* bufferUV = getPlaneUV()->getBytes();
	uint8_t* destBufferUV = destImage->getPlaneUV()->lockRect(widthUV, heightUV, destPitchUV);

	if( destPitchY == pitchY ) {
		for( unsigned int y = 0; y < heightUV; ++y ) {
			for( unsigned int x = 0; x < widthUV; ++x ) {
				unsigned int offset = (y * pitchUV) + x;
				destBufferUV[offset] = tableUV[bufferUV[offset]];
			}
		}
	} else {
		for( unsigned int y = 0; y < heightUV; ++y ) {
			for( unsigned int x = 0; x < widthUV; ++x ) {
				unsigned int inOffset = (y * pitchUV) + x;
				unsigned int outOffset = (y * destPitchUV) + x;
				destBufferUV[outOffset] = tableUV[bufferUV[inOffset]];
					}
		}
	}
}

void ImageYUVSemiplanar::compressRange(ImageYUVSemiplanar* destImage)
{
	if( m_Range == kYUVRange_Compressed ) {
		this->copy(destImage);
		destImage->setRange(kYUVRange_Compressed);
		return;
	}
	static bool didInitTable = false;
	static uint8_t invTableY[255];
	static uint8_t invTableUV[255];
	if( !didInitTable ) {
		for( int i = 0; i < 256; ++i ) {
			invTableY[i] = clamp(0, 255, floor(0.5f + lerp(16.0f, 235.0f, i / 255.0f)));
			invTableUV[i] = clamp(0, 255, floor(0.5f + lerp(16.0f, 240.0f, i / 255.0f)));
		}
		didInitTable = true;
	}
	applyLookupTable(destImage, invTableY, invTableUV);
	destImage->setRange(kYUVRange_Compressed);
}

void ImageYUVSemiplanar::expandRange(ImageYUVSemiplanar *destImage)
{
	if( m_Range == kYUVRange_Full ) {
		this->copy(destImage);
		destImage->setRange(kYUVRange_Full);
		return;
	}
	static bool didInitTable = false;
	static uint8_t tableY[255];
	static uint8_t tableUV[255];
	if( !didInitTable ) {
		for( int i = 0; i < 256; ++i ) {
			tableY[i] = clamp(0, 255, floor(0.5f + step(16.0f, 235.0f, i) * 255.0f));
			tableUV[i] = clamp(0, 255, floor(0.5f + step(16.0f, 240.0f, i) * 255.0f));
		}
		didInitTable = true;
	}
	applyLookupTable(destImage, tableY, tableUV);
	destImage->setRange(kYUVRange_Full);
}

EYUVRange ImageYUVSemiplanar::getRange()
{
	return m_Range;
}

void ImageYUVSemiplanar::setRange(EYUVRange range)
{
	m_Range = range;
}

unsigned int ImageYUVSemiplanar::getWidth() const
{
	return m_PlaneY->getWidth();
}

unsigned int ImageYUVSemiplanar::getHeight() const
{
	return m_PlaneY->getHeight();
}

unsigned int ImageYUVSemiplanar::getPadding()
{
	return m_PlaneY->getPadding();
}

EImageColorModel ImageYUVSemiplanar::getColorModel() const
{
	return kColorModel_YUV_420;
}

ImageRGBA* ImageYUVSemiplanar::asRGBA()
{
	return NULL;
}

ImageGrayscale* ImageYUVSemiplanar::asGrayscale()
{
	return NULL;
}

ImageInterleaved* ImageYUVSemiplanar::asInterleaved()
{
	return NULL;
}

ImageYUV* ImageYUVSemiplanar::asYUV()
{
	return NULL;
}

ImageYUVSemiplanar* ImageYUVSemiplanar::asYUVSemiplanar()
{
	return this;
}

}
