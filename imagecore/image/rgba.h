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

#include "imagecore/image/interleaved.h"

namespace imagecore {

class ImageRGBA : public ImageSinglePlane<4>
{
public:
	static ImageRGBA* create(uint8_t* buffer, unsigned int capacity, bool hasAlpha=false);
	static ImageRGBA* create(unsigned int width, unsigned int height, bool hasAlpha=false);
	static ImageRGBA* create(unsigned int width, unsigned int height, unsigned int padding, unsigned int alignment, bool hasAlpha=false);

	virtual ~ImageRGBA() {}

	void clearRect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
	bool scanAlpha();
	virtual ImageRGBA* move();

	void setOffset(unsigned int offsetX, unsigned int offsetY)
	{
		m_ImagePlane->setOffset(offsetX, offsetY);
	}

	ImageRGBA* asRGBA()
	{
		return this;
	}

	EImageColorModel getColorModel() const
	{
		return m_HasAlpha ? kColorModel_RGBA : kColorModel_RGBX;
	}

private:
	ImageRGBA(ImagePlane<4>* imagePlane, bool hasAlpha);
	bool m_HasAlpha;
};

}
