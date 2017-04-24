//
//  greyscale.h
//  ImageCore
//
//  Created by Luke Alonso on 8/29/14.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

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
