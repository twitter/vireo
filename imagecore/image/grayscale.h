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

class ImageGrayscale : public ImageSinglePlane<1>
{
public:
	static ImageGrayscale* create(uint8_t* buffer, unsigned int capacity);
	static ImageGrayscale* create(unsigned int width, unsigned int height);
	static ImageGrayscale* create(unsigned int width, unsigned int height, unsigned int padding, unsigned int alignment);

	virtual ~ImageGrayscale() {}

	virtual void clearRect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
	virtual ImageGrayscale* move();

	ImageRGBA* asRGBA()
	{
		return NULL;
	}

	ImageGrayscale* asGrayscale()
	{
		return this;
	}

	EImageColorModel getColorModel() const
	{
		return kColorModel_Grayscale;
	}

private:
	ImageGrayscale(ImagePlane<1>* imagePlane);
};

}
