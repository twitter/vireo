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

#include "imagecore/image/image.h"

namespace imagecore {

enum EYUVRange
{
	kYUVRange_Full,
	kYUVRange_Compressed,
	kYUVRange_Unknown
};

class ImageYUV : public Image
{
public:
	static ImageYUV* create(ImagePlane8* planeY, ImagePlane8* planeU, ImagePlane8* planeV);
	static ImageYUV* create(unsigned int width, unsigned int height);
	static ImageYUV* create(unsigned int width, unsigned int height, unsigned int padding, unsigned int alignment);

	virtual ~ImageYUV();

	virtual void setDimensions(unsigned int width, unsigned int height);
	virtual void setDimensions(unsigned int width, unsigned int height, unsigned int padding, unsigned int alignment);
	virtual void setPadding(unsigned int padding);

	virtual bool resize(Image* dest, EResizeQuality quality);
	virtual void reduceHalf(Image* dest);
	virtual bool crop(const ImageRegion& boundingBox);
	virtual void rotate(Image* dest, EImageOrientation direction);
	virtual void fillPadding();

	void copyRect(Image* dest, unsigned int sourceX, unsigned int sourceY, unsigned int destX, unsigned int destY, unsigned int width, unsigned int height);
	void clearRect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

	virtual ImageYUV* move();
	virtual unsigned int getWidth() const;
	virtual unsigned int getHeight() const;
	virtual unsigned int getPadding();
	virtual EImageColorModel getColorModel() const;

	// YUV
	virtual void expandRange(ImageYUV* destImage);
	virtual void compressRange(ImageYUV* destImage);
	virtual EYUVRange getRange();
	virtual void setRange(EYUVRange range);

	virtual ImageRGBA* asRGBA();
	virtual ImageGrayscale* asGrayscale();
	virtual ImageYUV* asYUV();
	virtual ImageYUVSemiplanar* asYUVSemiplanar();
	virtual ImageInterleaved* asInterleaved();

	ImagePlane8* getPlaneY()
	{
		return m_PlaneY;
	}

	ImagePlane8* getPlaneU()
	{
		return m_PlaneU;
	}

	ImagePlane8* getPlaneV()
	{
		return m_PlaneV;
	}

private:
	ImageYUV(ImagePlane8* planeY, ImagePlane8* planeU, ImagePlane8* planeV);
	void applyLookupTable(ImageYUV* destImage, uint8_t* tableY, uint8_t* tableUV);
	ImagePlane8* m_PlaneY;
	ImagePlane8* m_PlaneU;
	ImagePlane8* m_PlaneV;
	EYUVRange m_Range;
	bool m_OwnsPlanes;
};

}
