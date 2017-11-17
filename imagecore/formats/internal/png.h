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
#include "libpng16/png.h"

namespace imagecore {

class ImageReaderPNG : public ImageReader
{
public:
	DECLARE_IMAGE_READER(ImageReaderPNG);

	virtual bool initWithStorage(Storage* source);
	virtual bool readHeader();
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
	friend class ImageWriterPNG;

	Storage* m_Source;
	unsigned int m_Width;
	unsigned int m_Height;
	unsigned int m_TotalRowsRead;
	EImageColorModel m_NativeColorModel;
	png_structp m_PNGDecompress;
	png_infop m_PNGInfo;
};

class ImageWriterPNG : public ImageWriter
{
public:
	DECLARE_IMAGE_WRITER(ImageWriterPNG);

	virtual bool writeImage(Image* sourceImage);

	virtual bool beginWrite(unsigned int width, unsigned int height, EImageColorModel colorModel);
	virtual unsigned int writeRows(Image* sourceImage, unsigned int sourceRow, unsigned int numRows);
	virtual bool endWrite();

	virtual void setSourceReader(ImageReader* hintReader);
	virtual void setWriteOptions(unsigned int options);
	virtual bool copyLossless(ImageReader* reader);
private:
	virtual bool initWithStorage(Storage* output);
	void applyCompressionSettings();

	png_structp m_PNGCompress;
	png_infop m_PNGInfo;
	ImageReader* m_SourceReader;
	unsigned int m_WriteOptions;
};

}
