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

#include "imagecore/image/colorspace.h"
#include "imagecore/image/rgba.h"

namespace imagecore {

ImageRGBA* ImageRGBA::create(uint8_t* buffer, unsigned int capacity, bool hasAlpha)
{
	ImagePlaneRGBA* imagePlane = ImagePlaneRGBA::create(buffer, capacity);
	if( imagePlane != NULL ) {
		ImageRGBA* image = new ImageRGBA(imagePlane, hasAlpha);
		if( image != NULL ) {
			return image;
		}
		delete imagePlane;
	}
	return NULL;
}

ImageRGBA* ImageRGBA::create(unsigned int width, unsigned int height, bool hasAlpha)
{
	ImagePlaneRGBA* imagePlane = ImagePlaneRGBA::create(width, height);
	if( imagePlane != NULL ) {
		ImageRGBA* image = new ImageRGBA(imagePlane, hasAlpha);
		if( image != NULL ) {
			return image;
		}
		delete imagePlane;
	}
	return NULL;
}

ImageRGBA* ImageRGBA::create(unsigned int width, unsigned int height, unsigned int padding, unsigned int alignment, bool hasAlpha)
{
	ImagePlaneRGBA* imagePlane = ImagePlaneRGBA::create(width, height, padding, alignment);
	if( imagePlane != NULL ) {
		ImageRGBA* image = new ImageRGBA(imagePlane, hasAlpha);
		if( image != NULL ) {
			return image;
		}
		delete imagePlane;
	}
	return NULL;
}

ImageRGBA::ImageRGBA(ImagePlaneRGBA* imagePlane, bool hasAlpha)
:	ImageSinglePlane(imagePlane)
,	m_HasAlpha(hasAlpha)
{
}

void ImageRGBA::clearRect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	uint8_t rgba[4] = {r, g, b, a};
	uint32_t rgbai;
	memcpy(&rgbai, rgba, 4);
	m_ImagePlane->clearRect(x, y, w, h, rgbai);
}

bool ImageRGBA::scanAlpha()
{
	unsigned int framePitch;
	unsigned int width = m_ImagePlane->getWidth();
	unsigned int height = m_ImagePlane->getHeight();
	uint8_t* buffer = m_ImagePlane->lockRect(width, height, framePitch);
	for( unsigned int y = 0; y < height; y++ ) {
		for( unsigned int x = 0; x < width; x++ ) {
			const RGBA* c = (RGBA*)(&buffer[y * framePitch + x * 4]);
			if( c->a != 255 )
			{
				m_ImagePlane->unlockRect();
				return true;
			}
		}
	}
	m_ImagePlane->unlockRect();
	return false;
}

ImageRGBA* ImageRGBA::move()
{
	ImageRGBA* image = new ImageRGBA(m_ImagePlane, m_HasAlpha);
	m_OwnsPlane = false;
	return image;
}

}
