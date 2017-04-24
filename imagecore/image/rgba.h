//
//  rgba.h
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
