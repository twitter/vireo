//
//  webp.h
//  ImageCore
//
//  Created by Luke Alonso on 2/18/15.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

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
