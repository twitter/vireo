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

#include "imagecore/formats/reader.h"
#include "imagecore/formats/writer.h"
#include "imagecore/image/colorspace.h"

namespace imagecore {

enum EResizeMode
{
	kResizeMode_ExactCrop,
	kResizeMode_AspectFit,
	kResizeMode_AspectFill,
	kResizeMode_Stretch
};

class ResizeCropOperation
{
public:
	ResizeCropOperation();
	~ResizeCropOperation();

	void setImageReader(ImageReader* imageReader)
	{
		m_ImageReader = imageReader;
	}

	void setResizeQuality(EResizeQuality quality)
	{
		m_ResizeQuality = quality;
	}

	void setOutputSize(unsigned int width, unsigned int height)
	{
		m_OutputWidth = width;
		m_OutputHeight = height;
	}

	void setOutputMod(unsigned int mod)
	{
		m_OutputMod = mod;
	}

	void setCropGravity(ECropGravity gravity)
	{
		m_CropGravity = gravity;
	}

	void setCropRegion(ImageRegion* region)
	{
		m_CropRegion = region;
	}

	void setResizeMode(EResizeMode resizeMode)
	{
		m_ResizeMode = resizeMode;
	}

	void setOutputColorModel(EImageColorModel colorSpace)
	{
		m_OutputColorModel = colorSpace;
	}

	void setAllowUpsample(bool upsample)
	{
		m_AllowUpsample = upsample;
	}

	void setAllowDownsample(bool downsample)
	{
		m_AllowDownsample = downsample;
	}

	void setBackgroundFillColor(uint8_t r, uint8_t g, uint8_t b)
	{
		m_BackgroundFillColor = RGBA(r, g, b);
	}

	void estimateOutputSize(unsigned int imageWidth, unsigned int imageHeight, unsigned int& outputWidth, unsigned int& outputHeight);
	int performResizeCrop(Image*& resizedImage);
	int performResizeCrop(ImageRGBA*& resizedImage);

	Image* getInactiveImage()
	{
		return m_FilteredImage[m_WhichImage ^ 1];
	}

private:
	int readHeader();
	int load();
	int fillBackground();
	int resize();
	int rotateCrop();
	ImageReader* m_ImageReader;
	Image* m_FilteredImage[2];
	unsigned int m_WhichImage;
	EResizeMode m_ResizeMode;
	bool m_AllowUpsample;
	bool m_AllowDownsample;
	ImageRegion* m_CropRegion;
	ECropGravity m_CropGravity;
	EResizeQuality m_ResizeQuality;
	EImageColorModel m_OutputColorModel;
	unsigned int m_InputWidth;
	unsigned int m_InputHeight;
	unsigned int m_Orientation;
	unsigned int m_TargetWidth;
	unsigned int m_TargetHeight;
	unsigned int m_OutputWidth;
	unsigned int m_OutputHeight;
	unsigned int m_OutputMod;
	RGBA m_BackgroundFillColor;
};

}
