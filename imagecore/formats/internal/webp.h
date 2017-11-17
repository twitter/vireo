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
#include "register.h"
#include "webp/decode.h"
#include "webp/encode.h"

namespace imagecore {

class ImageReaderWebP : public ImageReader
{
public:
	DECLARE_IMAGE_READER(ImageReaderWebP);

	virtual bool initWithStorage(Storage* source);
	virtual bool readHeader();
	virtual void computeReadDimensions(unsigned int desiredWidth, unsigned int desiredHeight, unsigned int& readWidth, unsigned int& readHeight);
	virtual bool readImage(Image* destImage);
	virtual bool beginRead(unsigned int outputWidth, unsigned int outputHeight, EImageColorModel outputColorModel);
	virtual unsigned int readRows(Image* destImage, unsigned int destRow, unsigned int numRows);
	virtual bool endRead();
	virtual EImageFormat getFormat();
	virtual const char* getFormatName();
	virtual unsigned int getWidth();
	virtual unsigned int getHeight();
	virtual EImageColorModel getNativeColorModel();
	virtual bool supportsOutputColorModel(EImageColorModel colorModel);

private:
	Storage* m_Source;
	unsigned int m_Width;
	unsigned int m_Height;
	unsigned int m_TotalRowsRead;
	uint8_t* m_DecodeBuffer;
	uint64_t m_DecodeLength;
	bool m_OwnDecodeBuffer;
	EImageColorModel m_NativeColorModel;
	WebPDecoderConfig m_DecoderConfig;
	WebPBitstreamFeatures m_Features;
};

class ImageWriterWebP : public ImageWriter
{
public:
	DECLARE_IMAGE_WRITER(ImageWriterWebP);

	virtual bool writeImage(Image* sourceImage);

	virtual bool beginWrite(unsigned int width, unsigned int height, EImageColorModel colorModel);
	virtual unsigned int writeRows(Image* sourceImage, unsigned int sourceRow, unsigned int numRows);
	virtual bool endWrite();

	virtual bool applyExtraOptions(const char** optionNames, const char** optionValues, unsigned int numOptions);
	virtual void setQuality(unsigned int quality);
	virtual void setSourceReader(ImageReader* hintReader);
private:
	virtual bool initWithStorage(Storage* output);

	WebPConfig m_Config;
	Storage* m_OutputStorage;
	ImageReader* m_SourceReader;
};

}
