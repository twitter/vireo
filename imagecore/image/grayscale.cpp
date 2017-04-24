//
//  grayscale.cpp
//  ImageCore
//
//  Created by Luke Alonso on 8/29/14.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#include "imagecore/image/grayscale.h"
#include "imagecore/image/internal/conversions.h"

namespace imagecore {

ImageGrayscale* ImageGrayscale::create(uint8_t* buffer, unsigned int capacity)
{
	ImagePlaneGrayscale* imagePlane = ImagePlaneGrayscale::create(buffer, capacity);
	if( imagePlane != NULL ) {
		ImageGrayscale* image = new ImageGrayscale(imagePlane);
		if( image != NULL ) {
			return image;
		}
		delete imagePlane;
	}
	return NULL;
}

ImageGrayscale* ImageGrayscale::create(unsigned int width, unsigned int height)
{
	ImagePlaneGrayscale* imagePlane = ImagePlaneGrayscale::create(width, height);
	if( imagePlane != NULL ) {
		ImageGrayscale* image = new ImageGrayscale(imagePlane);
		if( image != NULL ) {
			return image;
		}
		delete imagePlane;
	}
	return NULL;
}

ImageGrayscale* ImageGrayscale::create(unsigned int width, unsigned int height, unsigned int padding, unsigned int alignment)
{
	ImagePlaneGrayscale* imagePlane = ImagePlaneGrayscale::create(width, height, padding, alignment);
	if( imagePlane != NULL ) {
		ImageGrayscale* image = new ImageGrayscale(imagePlane);
		if( image != NULL ) {
			return image;
		}
		delete imagePlane;
	}
	return NULL;
}

ImageGrayscale::ImageGrayscale(ImagePlaneGrayscale* imagePlane)
:	ImageSinglePlane(imagePlane)
{
}

ImageGrayscale* ImageGrayscale::move()
{
	ImageGrayscale* image = new ImageGrayscale(m_ImagePlane);
	m_OwnsPlane = false;
	return image;
}

void ImageGrayscale::clearRect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	int16_t Y;
	int16_t U;
	int16_t V;
	Conversions<false>::rgb_to_yuv(Y, U, V, r, g, b);
	m_ImagePlane->clearRect(x, y, w, h, Y);
}

}
