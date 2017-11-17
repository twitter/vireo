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
#include "imagecore/image/yuv.h"
#include "imagecore/formats/writer.h"
#include "imagecore/image/internal/conversions.h"

namespace imagecore {

static unsigned int computeSize(unsigned int in)
{
	return (in + 1) / 2;
}

ImageYUV* ImageYUV::create(ImagePlane8* planeY, ImagePlane8* planeU, ImagePlane8* planeV)
{
  if( planeY != NULL && planeU != NULL && planeV != NULL ) {
    ImageYUV* image = new ImageYUV(planeY, planeU, planeV);
    if( image != NULL ) {
      return image;
    }
  }
  return NULL;
}

ImageYUV* ImageYUV::create(unsigned int width, unsigned int height)
{
	return ImageYUV::create(width, height, 16, 16);
}

ImageYUV* ImageYUV::create(unsigned int width, unsigned int height, unsigned int padding, unsigned int alignment)
{
	ImagePlane8* planeY = ImagePlane8::create(width, height, padding, alignment);
	ImagePlane8* planeU = ImagePlane8::create(computeSize(width), computeSize(height), padding, alignment);
	ImagePlane8* planeV = ImagePlane8::create(computeSize(width), computeSize(height), padding, alignment);
	if( planeY != NULL && planeU != NULL && planeV != NULL ) {
		ImageYUV* image = new ImageYUV(planeY, planeU, planeV);
		if( image != NULL ) {
			return image;
		}
	}
	delete planeY;
	delete planeU;
	delete planeV;
	return NULL;
}

ImageYUV::ImageYUV(ImagePlaneGrayscale* planeY, ImagePlaneGrayscale* planeU, ImagePlaneGrayscale* planeV)
:	m_PlaneY(planeY)
,	m_PlaneU(planeU)
,	m_PlaneV(planeV)
,	m_Range(kYUVRange_Unknown)
,	m_OwnsPlanes(true)
{
}

ImageYUV::~ImageYUV()
{
	if( m_OwnsPlanes ) {
		delete m_PlaneY;
		delete m_PlaneU;
		delete m_PlaneV;
	}
	m_PlaneY = NULL;
	m_PlaneU = NULL;
	m_PlaneV = NULL;
}

void ImageYUV::setDimensions(unsigned int width, unsigned int height)
{
	m_PlaneY->setDimensions(width, height);
	m_PlaneU->setDimensions(computeSize(width), computeSize(height));
	m_PlaneV->setDimensions(computeSize(width), computeSize(height));
}

void ImageYUV::setDimensions(unsigned int width, unsigned int height, unsigned int padding, unsigned int alignment)
{
	m_PlaneY->setDimensions(width, height, padding, alignment);
	m_PlaneU->setDimensions(computeSize(width), computeSize(height), padding, alignment);
	m_PlaneV->setDimensions(computeSize(width), computeSize(height), padding, alignment);
}

void ImageYUV::setPadding(unsigned int padding)
{
	m_PlaneY->setPadding(padding);
	m_PlaneU->setPadding(padding);
	m_PlaneV->setPadding(padding);
}

bool ImageYUV::resize(Image* dest, EResizeQuality quality)
{
	ImageYUV* destYUV = dest->asYUV();
	if( destYUV ) {
		if( m_PlaneY->resize(destYUV->getPlaneY(), quality) ) {
			if( m_PlaneU->resize(destYUV->getPlaneU(), quality) ) {
				if( m_PlaneV->resize(destYUV->getPlaneV(), quality) ) {
					destYUV->setRange(m_Range);
					return true;
				}
			}
		}
	}
	return false;
}

void ImageYUV::reduceHalf(Image* dest)
{
	ImageYUV* destYUV = dest->asYUV();
	if( destYUV ) {
		m_PlaneY->reduceHalf(destYUV->getPlaneY());
		if( (m_PlaneU->getWidth() & 1) == 1 ) {
			destYUV->getPlaneU()->setDimensions(computeSize(dest->getWidth()), computeSize(dest->getHeight()));
			m_PlaneU->resize(destYUV->getPlaneU(), kResizeQuality_High);
		} else {
			m_PlaneU->reduceHalf(destYUV->getPlaneU());
		}
		if( (m_PlaneV->getWidth() & 1) == 1 ) {
			destYUV->getPlaneV()->setDimensions(computeSize(dest->getWidth()), computeSize(dest->getHeight()));
			m_PlaneV->resize(destYUV->getPlaneV(), kResizeQuality_High);
		} else {
			m_PlaneV->reduceHalf(destYUV->getPlaneV());
		}
	}
}

bool ImageYUV::crop(const ImageRegion& boundingBox)
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
		m_PlaneU->crop(boxUV);
		m_PlaneV->crop(boxUV);
		return true;
	}
	return false;
}

void ImageYUV::rotate(Image* dest, EImageOrientation direction)
{
	ImageYUV* destYUV = dest->asYUV();
	if( destYUV ) {
		m_PlaneY->rotate(destYUV->getPlaneY(), direction);
		m_PlaneU->rotate(destYUV->getPlaneU(), direction);
		m_PlaneV->rotate(destYUV->getPlaneV(), direction);
	}
}

void ImageYUV::fillPadding()
{
	m_PlaneY->fillPadding();
	m_PlaneU->fillPadding();
	m_PlaneV->fillPadding();
}

ImageYUV* ImageYUV::move()
{
	ImageYUV* image = new ImageYUV(m_PlaneY, m_PlaneU, m_PlaneV);
	m_OwnsPlanes = false;
	return image;
}

void ImageYUV::applyLookupTable(ImageYUV* destImage, uint8_t* tableY, uint8_t* tableUV)
{
	destImage->setDimensions(m_PlaneY->getWidth(), m_PlaneY->getHeight());
	unsigned int widthY = m_PlaneY->getWidth();
	unsigned int heightY = m_PlaneY->getHeight();
	unsigned int pitchY = m_PlaneY->getPitch();
	unsigned int destPitchY = m_PlaneY->getPitch();
	const uint8_t* bufferY = getPlaneY()->getBytes();
	uint8_t* destBufferY = destImage->getPlaneY()->lockRect(widthY, heightY, destPitchY);

	if( destPitchY == pitchY ) {
		for( unsigned int y = 0; y < heightY; y++ ) {
			for( unsigned int x = 0; x < widthY; x++ ) {
				unsigned int offset = (y * pitchY) + x;
				destBufferY[offset] = tableY[bufferY[offset]];
			}
		}
	} else {
		for( unsigned int y = 0; y < heightY; y++ ) {
			for( unsigned int x = 0; x < widthY; x++ ) {
				destBufferY[(y * destPitchY) + x] = tableY[bufferY[(y * pitchY) + x]];
			}
		}
	}

	unsigned int widthUV = m_PlaneU->getWidth();
	unsigned int heightUV = m_PlaneU->getHeight();
	unsigned int pitchUV = m_PlaneU->getPitch();
	unsigned int destPitchUV = 0;
	const uint8_t* bufferU = getPlaneU()->getBytes();
	const uint8_t* bufferV = getPlaneV()->getBytes();
	uint8_t* destBufferU = destImage->getPlaneU()->lockRect(widthUV, heightUV, destPitchUV);
	uint8_t* destBufferV = destImage->getPlaneV()->lockRect(widthUV, heightUV, destPitchUV);

	if( destPitchY == pitchY ) {
		for( unsigned int y = 0; y < heightUV; y++ ) {
			for( unsigned int x = 0; x < widthUV; x++ ) {
				unsigned int offset = (y * pitchUV) + x;
				destBufferU[offset] = tableUV[bufferU[offset]];
				destBufferV[offset] = tableUV[bufferV[offset]];
			}
		}
	} else {
		for( unsigned int y = 0; y < heightUV; y++ ) {
			for( unsigned int x = 0; x < widthUV; x++ ) {
				unsigned int inOffset = (y * pitchUV) + x;
				unsigned int outOffset = (y * destPitchUV) + x;
				destBufferU[outOffset] = tableUV[bufferU[inOffset]];
				destBufferV[outOffset] = tableUV[bufferV[inOffset]];
			}
		}
	}
}

void ImageYUV::compressRange(ImageYUV* destImage)
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
		for( int i = 0; i < 256; i++ ) {
			invTableY[i] = clamp(0, 255, floor(0.5f + lerp(16.0f, 235.0f, i / 255.0f)));
			invTableUV[i] = clamp(0, 255, floor(0.5f + lerp(16.0f, 240.0f, i / 255.0f)));
		}
		didInitTable = true;
	}
	applyLookupTable(destImage, invTableY, invTableUV);
	destImage->setRange(kYUVRange_Compressed);
}

void ImageYUV::expandRange(ImageYUV *destImage)
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
		for( int i = 0; i < 256; i++ ) {
			tableY[i] = clamp(0, 255, floor(0.5f + step(16.0f, 235.0f, i) * 255.0f));
			tableUV[i] = clamp(0, 255, floor(0.5f + step(16.0f, 240.0f, i) * 255.0f));
		}
		didInitTable = true;
	}
	applyLookupTable(destImage, tableY, tableUV);
	destImage->setRange(kYUVRange_Full);
}

EYUVRange ImageYUV::getRange()
{
	return m_Range;
}

void ImageYUV::setRange(EYUVRange range)
{
	m_Range = range;
}

unsigned int ImageYUV::getWidth() const
{
	return m_PlaneY->getWidth();
}

unsigned int ImageYUV::getHeight() const
{
	return m_PlaneY->getHeight();
}

unsigned int ImageYUV::getPadding()
{
	return m_PlaneY->getPadding();
}

EImageColorModel ImageYUV::getColorModel() const
{
	return kColorModel_YUV_420;
}

ImageRGBA* ImageYUV::asRGBA()
{
	return NULL;
}

ImageGrayscale* ImageYUV::asGrayscale()
{
	return NULL;
}

ImageInterleaved* ImageYUV::asInterleaved()
{
	return NULL;
}

ImageYUV* ImageYUV::asYUV()
{
	return this;
}

ImageYUVSemiplanar* ImageYUV::asYUVSemiplanar()
{
	return NULL;
}

void ImageYUV::copyRect(Image* dest, unsigned int sourceX, unsigned int sourceY, unsigned int destX, unsigned int destY, unsigned int width, unsigned int height)
{
	ImageYUV* destYUV = dest->asYUV();
	SECURE_ASSERT(destYUV != NULL);
	const uint32_t sourceX_Y = sourceX;
	const uint32_t sourceY_Y = sourceY;
	const uint32_t destX_Y = destX;
	const uint32_t destY_Y = destY;
	const uint32_t width_Y = width;
	const uint32_t height_Y = height;
	const uint32_t sourceX_UV = sourceX / 2;
	const uint32_t sourceY_UV = sourceY / 2;
	const uint32_t destX_UV = destX / 2;
	const uint32_t destY_UV = destY / 2;
	const uint32_t width_UV = computeSize(width);
	const uint32_t height_UV = computeSize(height);
	m_PlaneY->copyRect(destYUV->m_PlaneY, sourceX_Y, sourceY_Y, destX_Y, destY_Y, width_Y, height_Y);
	m_PlaneU->copyRect(destYUV->m_PlaneU, sourceX_UV, sourceY_UV, destX_UV, destY_UV, width_UV, height_UV);
	m_PlaneV->copyRect(destYUV->m_PlaneV, sourceX_UV, sourceY_UV, destX_UV, destY_UV, width_UV, height_UV);
}

void ImageYUV::clearRect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	// convert to YUV
	int16_t Y;
	int16_t U;
	int16_t V;
	Conversions<false>::rgb_to_yuv(Y, U, V, r, g, b);
	const uint32_t sourceX_Y = x;
	const uint32_t sourceY_Y = y;
	const uint32_t width_Y = w;
	const uint32_t height_Y = h;
	const uint32_t sourceX_UV = x / 2;
	const uint32_t sourceY_UV = y / 2;
	const uint32_t width_UV = computeSize(w);
	const uint32_t height_UV = computeSize(h);
	m_PlaneY->clearRect(sourceX_Y, sourceY_Y, width_Y, height_Y, Y);
	m_PlaneU->clearRect(sourceX_UV, sourceY_UV, width_UV, height_UV, U);
	m_PlaneV->clearRect(sourceX_UV, sourceY_UV, width_UV, height_UV, V);
}

}
