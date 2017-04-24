//
//  gif.h
//  ImageTool
//
//  Created by Luke Alonso on 10/28/12.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "imagecore/formats/reader.h"
#include "giflib/gif_lib.h"
#include "register.h"

namespace imagecore {

class ImageReaderGIF : public ImageReader
{
public:
	class Factory : public ImageReader::Factory
	{
	public:
		virtual ImageReader* create();
		virtual bool matchesSignature(const uint8_t* sig, unsigned int sigLen);
	};

	ImageReaderGIF();
	virtual ~ImageReaderGIF();

	virtual bool initWithStorage(Storage* source);
	virtual bool readHeader();
	virtual bool readImage(Image* destImage);
	virtual EImageFormat getFormat();
	virtual const char* getFormatName();
	virtual unsigned int getWidth();
	virtual unsigned int getHeight();
	virtual EImageColorModel getNativeColorModel();

	// Animation.
	virtual unsigned int getNumFrames();
	virtual bool advanceFrame();
	virtual bool seekToFirstFrame();
	virtual unsigned int getFrameDelayMs();

	// GIF
	virtual void preDecodeFrame(unsigned int frameIndex);

	static bool matchesSignature(uint8_t* sig, unsigned int sigSize);

private:
	bool copyFrameRegion(unsigned int frameIndex, ImageRGBA* destImage, bool writeBackground);

	Storage* m_Source;
	unsigned int m_ColorType;
	unsigned int m_Width;
	unsigned int m_Height;
	unsigned int m_CurrentFrame;
	GifFileType* m_Gif;
	ImageRGBA* m_PrevFrameCopy;
	bool m_HasAlpha;
};

}
