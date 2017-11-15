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

// ImagePlane proxy for the simple single-plane image implementations.

#pragma once

#include "imagecore/image/image.h"
#include "imagecore/image/yuv_semiplanar.h"

namespace imagecore {

class ImageInterleaved : public Image
{
public:
	virtual uint8_t* lockRect(unsigned int width, unsigned int height, unsigned int& pitch) = 0;
	virtual uint8_t* lockRect(unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned int& pitch) = 0;
	virtual void unlockRect() = 0;
	virtual unsigned int getPitch() = 0;
	virtual const uint8_t* getBytes() = 0;
	virtual unsigned int getImageSize() = 0;
	virtual unsigned int getComponentSize() = 0;
};

template<uint32_t Channels>
class ImageSinglePlane : public ImageInterleaved
{
	typedef toPrimType<Channels> primType;

public:
	const uint8_t* getBytes()
	{
		return m_ImagePlane->getBytes();
	}

	uint8_t* lockRect(unsigned int width, unsigned int height, unsigned int& pitch)
	{
		return m_ImagePlane->lockRect(width, height, pitch);
	}

	uint8_t* lockRect(unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned int& pitch)
	{
		return m_ImagePlane->lockRect(x, y, width, height, pitch);
	}

	void unlockRect()
	{
		m_ImagePlane->unlockRect();
	}

	void setDimensions(unsigned int width, unsigned int height)
	{
		return m_ImagePlane->setDimensions(width, height);
	}

	void setDimensions(unsigned int width, unsigned int height, unsigned int padding, unsigned int alignment)
	{
		return m_ImagePlane->setDimensions(width, height, padding, alignment);
	}

	void setPadding(unsigned int padding)
	{
		m_ImagePlane->setPadding(padding);
	}

	bool resize(Image* dest, EResizeQuality quality)
	{
		ASSERT(dest->getColorModel() == this->getColorModel());
		return m_ImagePlane->resize(((ImageSinglePlane<Channels>*)dest)->getPlane(), quality);
	}

	void reduceHalf(Image* dest)
	{
		ASSERT(dest->getColorModel() == this->getColorModel());
		m_ImagePlane->reduceHalf(((ImageSinglePlane<Channels>*)dest)->getPlane());
	}

	bool downsampleFilter(Image* dest, const FilterKernelAdaptive* filterKernelX, const FilterKernelAdaptive* filterKernelY)
	{
		ASSERT(dest->getColorModel() == this->getColorModel());
		return m_ImagePlane->downsampleFilter(((ImageSinglePlane<Channels>*)dest)->getPlane(), filterKernelX, filterKernelY, false);
	}

	bool crop(const ImageRegion& boundingBox)
	{
		return m_ImagePlane->crop(boundingBox);
	}

	void rotate(Image* dest, EImageOrientation direction)
	{
		ASSERT(dest->getColorModel() == this->getColorModel());
		m_ImagePlane->rotate(((ImageSinglePlane<Channels>*)dest)->getPlane(), direction);
	}

	void fillPadding()
	{
		m_ImagePlane->fillPadding();
	}

	void copyRect(Image* dest, unsigned int sourceX, unsigned int sourceY, unsigned int destX, unsigned int destY, unsigned int width, unsigned int height)
	{
		ASSERT(dest->getColorModel() == this->getColorModel());
		m_ImagePlane->copyRect(((ImageSinglePlane<Channels>*)dest)->getPlane(), sourceX, sourceY, destX, destY, width, height);
	}

	unsigned int getWidth() const
	{
		return m_ImagePlane->getWidth();
	}

	unsigned int getHeight() const
	{
		return m_ImagePlane->getHeight();
	}

	unsigned int getPitch()
	{
		return m_ImagePlane->getPitch();
	}

	unsigned int getPadding()
	{
		return m_ImagePlane->getPadding();
	}

	unsigned int getCapacity()
	{
		return m_ImagePlane->getCapacity();
	}

	unsigned int getImageSize()
	{
		return m_ImagePlane->getImageSize();
	}

	ImagePlane<Channels>* getPlane()
	{
		return m_ImagePlane;
	}

	unsigned int getComponentSize()
	{
		return Channels;
	}

	virtual ImageRGBA* asRGBA()
	{
		return NULL;
	}

	virtual ImageGrayscale* asGrayscale()
	{
		return NULL;
	}

	ImageYUV* asYUV()
	{
		return NULL;
	}

	ImageYUVSemiplanar* asYUVSemiplanar()
	{
		return NULL;
	}

	ImageInterleaved* asInterleaved()
	{
		return this;
	}

	virtual ~ImageSinglePlane()
	{
		if( m_OwnsPlane ) {
		delete m_ImagePlane;
		}
		m_ImagePlane = NULL;
	}

protected:
	ImageSinglePlane(ImagePlane<Channels>* imagePlane)
	:	m_ImagePlane(imagePlane)
	,	m_OwnsPlane(true)
	{
	}
	ImagePlane<Channels>* m_ImagePlane;
	bool m_OwnsPlane;
};

}
