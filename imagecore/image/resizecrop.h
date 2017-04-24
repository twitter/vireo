//
//  resizecrop.h
//  ImageTool
//
//  Created by Luke Alonso on 3/12/13.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

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
