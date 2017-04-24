//
//  bmp.h
//  ImageTool
//
//  Created by Luke Alonso on 10/28/12.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "imagecore/formats/reader.h"
#include "register.h"

#pragma pack(push, 1)

namespace imagecore {

struct BITMAP_FILEHEADER
{
	uint16_t Signature;
	uint32_t Size;
	uint32_t Reserved;
	uint32_t BitsOffset;
};

struct BITMAP_HEADER
{
	uint32_t HeaderSize;
	int32_t Width;
	int32_t Height;
	uint16_t Planes;
	uint16_t BitCount;
	uint32_t Compression;
	uint32_t SizeImage;
	int32_t PelsPerMeterX;
	int32_t PelsPerMeterY;
	uint32_t ClrUsed;
	uint32_t ClrImportant;
};

struct BITMAP_HEADER_EXTENDED
{
	BITMAP_HEADER bitmapHeader;
	uint32_t RedMask;
	uint32_t GreenMask;
	uint32_t BlueMask;
	uint32_t AlphaMask;
};

#pragma pack(pop)

class ImageReaderBMP : public ImageReader
{
public:
	DECLARE_IMAGE_READER(ImageReaderBMP);

	virtual bool initWithStorage(Storage* source);
	virtual bool readHeader();
	virtual bool readImage(Image* destImage);
	virtual EImageFormat getFormat();
	virtual const char* getFormatName();
	virtual unsigned int getWidth();
	virtual unsigned int getHeight();
	virtual EImageColorModel getNativeColorModel();

private:
	BITMAP_FILEHEADER m_BitmapFileHeader;
	BITMAP_HEADER* m_BitmapHeader;
	Storage* m_Source;
	unsigned int m_Width;
	unsigned int m_Height;
	EImageColorModel m_NativeColorModel;
};

}
