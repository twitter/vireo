//
//  writer.h
//  ImageCore
//
//  Created by Luke Alonso on 8/20/14.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "imagecore/formats/format.h"
#include "imagecore/formats/reader.h"
#include "imagecore/image/image.h"

namespace imagecore {

class ImageWriter
{
public:
	class Factory
	{
	public:
		virtual ImageWriter* create() = 0;
		virtual EImageFormat getFormat() = 0;
		virtual bool appropriateForInputFormat(EImageFormat) = 0;
		virtual bool supportsInputColorModel(EImageColorModel) = 0;
		virtual bool matchesExtension(const char* extension) = 0;
	};

	class Storage
	{
	public:
		virtual ~Storage() { }
		virtual uint64_t write(const void* sourceBuffer, uint64_t numBytes) = 0;
		virtual uint64_t writeStream(class ImageReader::Storage* stream);
		virtual bool asFile(FILE*& file) = 0;
		virtual bool asBuffer(uint8_t*& buffer, uint64_t& length) = 0;
		virtual uint64_t totalBytesWritten() = 0;
		virtual void flush() = 0;
	};

	class FileStorage : public Storage
	{
	public:
		static FileStorage* open(const char* filePath);

		FileStorage(FILE* file, bool ownsFile = false);
		~FileStorage();

		virtual uint64_t write(const void* sourceBuffer, uint64_t numBytes);
		virtual bool asFile(FILE*& file);
		virtual bool asBuffer(uint8_t*& buffer, uint64_t& length);
		virtual uint64_t totalBytesWritten();
		virtual void flush();

		bool fileError();

	private:
		FILE* m_File;
		uint64_t m_BytesWritten;
		bool m_OwnsFile;
	};

	class MemoryStorage : public Storage
	{
	public:
		MemoryStorage();
		MemoryStorage(uint64_t bufferLength);
		MemoryStorage(void* buffer, uint64_t length);

		virtual ~MemoryStorage();
		virtual uint64_t write(const void* destBuffer, uint64_t numBytes);
		virtual bool asFile(FILE*& file);
		virtual bool asBuffer(uint8_t*& buffer, uint64_t& length);
		virtual bool ownBuffer(uint8_t*& buffer, uint64_t& length);
		virtual uint64_t totalBytesWritten();
		virtual void flush();
	protected:
		bool grow(uint64_t numBytes);
		uint8_t* m_Buffer;
		uint64_t m_TotalBytes;
		uint64_t m_UsedBytes;
		bool m_OwnsBuffer;
	};

	class MemoryMappedStorage : public MemoryStorage
	{
	public:
		static MemoryMappedStorage* map(FILE* f);
		static MemoryMappedStorage* map(int fd, uint64_t length);

		virtual ~MemoryMappedStorage();
	protected:
		MemoryMappedStorage(void* buffer, uint64_t length);
	};

	enum EWriteOptions
	{
		kWriteOption_CopyMetaData               = 0x01,
		kWriteOption_CopyColorProfile           = 0x02,
		kWriteOption_WriteDefaultColorProfile   = 0x04,
		kWriteOption_QualityFast                = 0x08,
		kWriteOption_WriteExifOrientation       = 0x10,
		kWriteOption_LosslessPerfect            = 0x20,
		kWriteOption_GeoTagData                 = 0x40,
		kWriteOption_AssumeMCUPaddingFilled     = 0x80,
		kWriteOption_ForcePNGRunLengthEncoding  = 0x100,
		kWriteOption_Progressive                = 0x200
	};

	virtual ~ImageWriter() { }

	virtual void setWriteOptions(unsigned int writeOptions) { }
	virtual void setQuality(unsigned int quality) { }
	virtual void setSourceReader(ImageReader* sourceReader) { }
	virtual bool applyExtraOptions(const char** optionNames, const char** optionValues, unsigned int numOptions) { return numOptions == 0; }

	virtual bool writeImage(Image* sourceImage) = 0;

	// Incremental writing.
	virtual bool beginWrite(unsigned int width, unsigned int height, EImageColorModel colorModel) = 0;
	virtual unsigned int writeRows(Image* sourceImage, unsigned int sourceRow, unsigned int numRows) = 0;
	virtual bool endWrite() = 0;

	virtual bool copyLossless(ImageReader* reader);

	static ImageWriter* createWithFormat(EImageFormat imageFormat, ImageWriter::Storage* storage);
	static bool outputFormatSupportsColorModel(EImageFormat imageFormat, EImageColorModel colorModel);
	static EImageFormat formatFromExtension(const char* filename, EImageFormat defaultImageFormat);

	static int registerWriter(Factory* factory);

	static void debugWriteRaw(const char* file, Image* image);
	static void debugWriteRaw(const char* file, ImagePlane8* image);
	static void debugWriteRaw(const char* file, ImagePlaneRGBA* image);

	static void writeToFile(const char* file, Image* image, EImageFormat format);
	static void writeToFile(const char* file, ImagePlane8* image, EImageFormat format);
	static void writeToFile(const char* file, ImagePlaneRGBA* image, EImageFormat format);

private:
	static Factory* factoryForFormat(EImageFormat imageFormat);

	virtual bool initWithStorage(Storage* output) = 0;
};

}
