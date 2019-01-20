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

#include "imagecore/formats/writer.h"
#include "imagecore/formats/reader.h"
#include "imagecore/utils/securemath.h"
#include "imagecore/utils/mathutils.h"
#include "internal/register.h"
#include "imagecore/image/rgba.h"
#include "imagecore/image/grayscale.h"
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>

#define MAX_FORMATS 32

namespace imagecore {

ImageWriter::Factory* s_WriterFactories[MAX_FORMATS];
unsigned int s_NumWriterFactories;
static int s_RegisteredDefaultWriters = registerDefaultImageWriters();

int ImageWriter::registerWriter(ImageWriter::Factory* factory)
{
	ASSERT(s_NumWriterFactories < MAX_FORMATS);
	s_WriterFactories[s_NumWriterFactories] = factory;
	++s_NumWriterFactories;
	return s_NumWriterFactories;
}

EImageFormat ImageWriter::formatFromExtension(const char* filename, EImageFormat defaultImageFormat)
{
	const char* lastDotStr = filename;
	const char* nextDotStr = filename;
	do {
		nextDotStr = strstr(nextDotStr + 1, ".");
		if( nextDotStr != NULL ) {
			lastDotStr = nextDotStr + 1;
		}
	} while (nextDotStr != NULL);
	for( unsigned int i = 0; i < s_NumWriterFactories; ++i ) {
		if( s_WriterFactories[i]->matchesExtension(lastDotStr) ) {
			return s_WriterFactories[i]->getFormat();
		}
	}
	return defaultImageFormat;
}

ImageWriter* ImageWriter::createWithFormat(EImageFormat imageFormat, ImageWriter::Storage* output)
{
	ImageWriter* writer = NULL;

	Factory* factory = factoryForFormat(imageFormat);
	if( factory != NULL ) {
		writer = factory->create();
	}

	if( writer != NULL ) {
		if( !writer->initWithStorage(output) ) {
			delete writer;
			writer = NULL;
		}
	}

	return writer;
}

ImageWriter::Factory* ImageWriter::factoryForFormat(EImageFormat imageFormat)
{
	// First try to find an exact match.
	for( unsigned int i = 0; i < s_NumWriterFactories; ++i ) {
		if( s_WriterFactories[i]->getFormat() == imageFormat ) {
			return s_WriterFactories[i];
		}
	}

	// Then try to find a fallback.
	for( unsigned int i = 0; i < s_NumWriterFactories; ++i ) {
		if( s_WriterFactories[i]->appropriateForInputFormat(imageFormat) ) {
			return s_WriterFactories[i];
		}
	}

	if( s_NumWriterFactories > 0 ) {
		return s_WriterFactories[0];
	}

	return NULL;
}

bool ImageWriter::outputFormatSupportsColorModel(EImageFormat imageFormat, EImageColorModel colorModel)
{
	Factory* factory = factoryForFormat(imageFormat);
	if( factory != NULL ) {
		return factory->supportsInputColorModel(colorModel);
	}
	return false;
}

bool ImageWriter::copyLossless(ImageReader* reader)
{
	bool success = false;
	// Preserve RGBX/RGBA.
	EImageColorModel colorModel = Image::colorModelIsRGBA(reader->getNativeColorModel()) ? reader->getNativeColorModel() : kColorModel_RGBA;
	Image* image = Image::create(colorModel, reader->getWidth(), reader->getHeight());
	if( image != NULL ) {
		if( reader->readImage(image) ) {
			if( writeImage(image) ) {
				success = true;
			}
		}
		delete image;
	}
	return success;
}

// ImageWriter::FileStorage

ImageWriter::FileStorage* ImageWriter::FileStorage::open(const char* filePath)
{
	if( strcmp(filePath, "-") == 0 ) {
		return new FileStorage(stdout, false);
	} else {
		FILE* f = fopen(filePath, "wb");
		if( f != NULL) {
			return new FileStorage(f, true);
		}
	}
	return NULL;
}

ImageWriter::FileStorage::FileStorage(FILE* file, bool ownsFile)
:	m_File(file)
,	m_BytesWritten(0)
,	m_OwnsFile(ownsFile)
{
}

ImageWriter::FileStorage::~FileStorage()
{
	if( m_OwnsFile ) {
		fflush(m_File);
		fclose(m_File);
		m_File = NULL;
	}
}

uint64_t ImageWriter::FileStorage::write(const void* sourceBuffer, uint64_t numBytes)
{
	uint64_t written = (uint64_t)fwrite(sourceBuffer, 1, (size_t)numBytes, m_File);
	m_BytesWritten = SafeUAdd(m_BytesWritten, written);
	return written;
}

uint64_t ImageWriter::Storage::writeStream(class ImageReader::Storage *stream)
{
	uint8_t buffer[1024];
	uint64_t totalBytesRead = 0;
	uint64_t bytesRead = 0;
	do {
		bytesRead = stream->read(buffer, 1024);
		if( bytesRead > 0 ) {
			write(buffer, bytesRead);
		}
		totalBytesRead += bytesRead;
	} while ( bytesRead > 0 );
	return totalBytesRead;
}

bool ImageWriter::FileStorage::asFile(FILE*& file)
{
	file = m_File;
	return true;
}

bool ImageWriter::FileStorage::asBuffer(uint8_t*& buffer, uint64_t& length)
{
	return false;
}

uint64_t ImageWriter::FileStorage::totalBytesWritten()
{
	return m_BytesWritten;
}

void ImageWriter::FileStorage::flush()
{
	fflush(m_File);
}

bool ImageWriter::FileStorage::fileError()
{
	return ferror(m_File) != 0;
}

// ImageWriter::MemoryStorage
const size_t defaultBufferSize = 256 * 1024;
const size_t maxBufferGrowth = 512 * 1024;

ImageWriter::MemoryStorage::MemoryStorage()
{
	m_Buffer = (uint8_t*)malloc(defaultBufferSize);
    m_UsedBytes = 0;
	m_TotalBytes = defaultBufferSize;
	m_OwnsBuffer = true;
}

ImageWriter::MemoryStorage::MemoryStorage(uint64_t bufferLength)
{
	m_Buffer = (uint8_t*)malloc((size_t)bufferLength);
	m_UsedBytes = 0;
	m_TotalBytes = bufferLength;
	m_OwnsBuffer = true;
}

ImageWriter::MemoryStorage::MemoryStorage(void* buffer, uint64_t length)
{
	m_Buffer = (uint8_t*)buffer;
	m_UsedBytes = 0;
	m_TotalBytes = length;
	m_OwnsBuffer = false;
}

ImageWriter::MemoryStorage::~MemoryStorage()
{
	if (m_OwnsBuffer) {
		free(m_Buffer);
		m_Buffer = NULL;
		m_OwnsBuffer = false;
	}
}

bool ImageWriter::MemoryStorage::grow(uint64_t numBytes)
{
	if (!m_OwnsBuffer) {
		return false;
	}
	uint64_t newBufferSize = SafeUAdd(m_TotalBytes, max(SafeUMul(numBytes, 2), min(m_TotalBytes, (uint64_t)maxBufferGrowth)));
	m_Buffer = (uint8_t*)realloc(m_Buffer, (size_t)newBufferSize);
	if( !m_Buffer ) {
		return false;
	}
	m_TotalBytes = newBufferSize;
	return true;
}

uint64_t ImageWriter::MemoryStorage::write(const void* sourceBuffer, uint64_t numBytes)
{
	uint64_t bytesToWrite = numBytes;
	if( SafeUAdd(m_UsedBytes, numBytes) > m_TotalBytes ) {
		if( !grow(numBytes) ) {
			return 0;
		}
	}
	if( bytesToWrite > 0 ) {
		memcpy(m_Buffer + m_UsedBytes, sourceBuffer, (size_t)bytesToWrite);
		m_UsedBytes = SafeUAdd(m_UsedBytes, bytesToWrite);
	}
	return bytesToWrite;
}

bool ImageWriter::MemoryStorage::asFile(FILE*& file)
{
	return false;
}

bool ImageWriter::MemoryStorage::asBuffer(uint8_t*& buffer, uint64_t& length)
{
	buffer = m_Buffer;
	length = m_TotalBytes;
	return true;
}

bool ImageWriter::MemoryStorage::ownBuffer(uint8_t*& buffer, uint64_t& length)
{
	buffer = m_Buffer;
	length = m_TotalBytes;
	m_OwnsBuffer = false;
	return true;
}

void ImageWriter::MemoryStorage::flush()
{
	m_TotalBytes = m_UsedBytes;
}

uint64_t ImageWriter::MemoryStorage::totalBytesWritten()
{
	return m_UsedBytes;
}

// ImageReader::MemoryMappedStorage

ImageWriter::MemoryMappedStorage* ImageWriter::MemoryMappedStorage::map(FILE* f)
{
	int fd = fileno(f);
	if (fd >= 0) {
		struct stat sb;
		if( fstat(fd, &sb) != -1 ) {
			size_t size = (size_t)sb.st_size;
			return MemoryMappedStorage::map(fd, size);
		}
	}
	return NULL;
}

ImageWriter::MemoryMappedStorage* ImageWriter::MemoryMappedStorage::map(int fd, uint64_t length)
{
	void* mapBuffer = mmap(NULL, (size_t)length, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
	if( mapBuffer != MAP_FAILED ) {
		return new MemoryMappedStorage(mapBuffer, length);
	} else {
		printf("err: %i\n", errno);
	}
	return NULL;
}

ImageWriter::MemoryMappedStorage::MemoryMappedStorage(void* buffer, uint64_t length)
:	MemoryStorage(buffer, length)
{
}

ImageWriter::MemoryMappedStorage::~MemoryMappedStorage()
{
	munmap(m_Buffer, (size_t)m_TotalBytes);
	m_Buffer = NULL;
}

void ImageWriter::writeToFile(const char* file, Image* image, EImageFormat format)
{
	FILE* fd = fopen(file, "wb");
	ImageWriter::FileStorage storage(fd);
	ImageWriter* writer = createWithFormat(format, &storage);
	writer->writeImage(image);
	delete writer;
	fclose(fd);
}

void ImageWriter::writeToFile(const char* file, ImagePlane8* image, EImageFormat format)
{
	ImageGrayscale* img = ImageGrayscale::create(image->getWidth(), image->getHeight());
	image->copy(img->getPlane());
	writeToFile(file, img, format);
	delete img;
}

void ImageWriter::writeToFile(const char* file, ImagePlaneRGBA* image, EImageFormat format)
{
	ImageRGBA* img = ImageRGBA::create(image->getWidth(), image->getHeight());
	image->copy(img->getPlane());
	writeToFile(file, img, format);
	delete img;
}

}
