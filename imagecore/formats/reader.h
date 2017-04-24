//
//  reader.h
//  ImageCore
//
//  Created by Luke Alonso on 8/20/14.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "imagecore/formats/format.h"
#include "imagecore/image/image.h"

namespace imagecore {

class ImageReader
{
public:
	class Factory
	{
	public:
		virtual ImageReader* create() = 0;
		virtual bool matchesSignature(const uint8_t* sig, unsigned int sigLen) = 0;
	};

	class Storage
	{
	public:
		enum SeekMode
		{
			kSeek_Set = 0,
			kSeek_Current = 1,
			kSeek_End = 2
		};
		virtual ~Storage() { }
		virtual uint64_t read(void* destBuffer, uint64_t numBytes) = 0;
		virtual bool seek(int64_t pos, SeekMode mode) = 0;
		virtual uint64_t tell() = 0;
		virtual bool canSeek() = 0;
		virtual bool asFile(FILE*& file) = 0;
		virtual bool asBuffer(uint8_t*& buffer, uint64_t& length) = 0;
		virtual bool peekSignature(uint8_t* signature) = 0;
		friend class ImageReader;
	};

	class MemoryStorage : public Storage
	{
	public:
		MemoryStorage(void* buffer, uint64_t length, bool ownsBuffer = false);
		virtual ~MemoryStorage();

		virtual uint64_t read(void* destBuffer, uint64_t numBytes);
		virtual bool seek(int64_t pos, SeekMode mode);
		virtual uint64_t tell();
		virtual bool canSeek();
		virtual bool asFile(FILE*& file);
		virtual bool asBuffer(uint8_t*& buffer, uint64_t& length);
		virtual bool peekSignature(uint8_t* signature);
	protected:
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

	class FileStorage : public Storage
	{
	public:
		static FileStorage* open(const char* filePath);

		FileStorage(FILE* file);
		FileStorage(FILE* file, bool canSeek, bool ownsFile);

		virtual ~FileStorage();
		virtual uint64_t read(void* destBuffer, uint64_t numBytes);
		virtual bool seek(int64_t pos, SeekMode mode);
		virtual uint64_t tell();
		virtual bool canSeek();
		virtual bool asFile(FILE*& file);
		virtual bool asBuffer(uint8_t*& buffer, uint64_t& length);
		virtual bool peekSignature(uint8_t* signature);
	private:
		FILE* m_File;
		bool m_OwnsFile;
		bool m_CanSeek;
		ImageReader::MemoryMappedStorage* m_MmapStorage;
	};

	enum EReadOptions
	{
		kReadOption_ApplyColorProfile     = 0x01,
		kReadOption_DecompressQualityFast = 0x02
	};

	virtual ~ImageReader() { }
	virtual bool readHeader() = 0;

	virtual void setReadOptions(unsigned int readOptions) { }

	virtual bool readImage(Image* destImage) = 0;

	// Incremental reading, not supported by all readers;
	virtual bool beginRead(unsigned int outputWidth, unsigned int outputHeight, EImageColorModel outputColorModel);
	virtual unsigned int readRows(Image* destImage, unsigned int destRow, unsigned int numRows);
	virtual bool endRead();

	virtual void computeReadDimensions(unsigned int desiredWidth, unsigned int desiredHeight, unsigned int& readWidth, unsigned int& readHeight);

	virtual EImageFormat getFormat() = 0;
	virtual const char* getFormatName() = 0;
	virtual unsigned int getWidth() = 0;
	virtual unsigned int getHeight() = 0;
	unsigned int getOrientedWidth();
	unsigned int getOrientedHeight();
	virtual EImageOrientation getOrientation();
	virtual EImageColorModel getNativeColorModel();
	virtual bool supportsOutputColorModel(EImageColorModel colorSpace);

	// Animation, mainly for GIF.
	virtual unsigned int getNumFrames() { return 1; }
	virtual bool advanceFrame() { return false; }
	virtual bool seekToFirstFrame() { return false; }
	virtual unsigned int getFrameDelayMs() { return 0; }

	virtual uint8_t* getColorProfile(unsigned int& size) { size = 0; return NULL; }

	static ImageReader* create(Storage* source);
	static ImageReader* initReader(Storage* source, ImageReader* imageReader);

	static int registerReader(Factory* factory);

private:
	static ImageReader* createFromSignature(const uint8_t* sig);
	virtual bool initWithStorage(Storage* storage) = 0;
};

}
