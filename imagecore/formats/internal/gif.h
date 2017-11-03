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
