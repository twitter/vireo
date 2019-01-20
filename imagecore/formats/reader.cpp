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

#include "imagecore/imagecore.h"
#include "imagecore/formats/reader.h"
#include "imagecore/utils/securemath.h"
#include "internal/register.h"
#include "writer.h"
#include <sys/stat.h>
#include <sys/mman.h>

#define SIGNATURE_MAX_SIZE 8
#define MAX_FORMATS 32

namespace imagecore {

static ImageReader::Factory* s_ReaderFactories[MAX_FORMATS];
static unsigned int s_NumReaderFactories;
static int s_RegisteredDefaultReaders = registerDefaultImageReaders();

int ImageReader::registerReader(ImageReader::Factory* factory)
{
	ASSERT(s_NumReaderFactories < MAX_FORMATS);
	s_ReaderFactories[s_NumReaderFactories] = factory;
	++s_NumReaderFactories;
	return s_NumReaderFactories;
}

ImageReader* ImageReader::createFromSignature(const uint8_t* sig)
{
	for( unsigned int i = 0; i < s_NumReaderFactories; ++i ) {
		if( s_ReaderFactories[i]->matchesSignature(sig, SIGNATURE_MAX_SIZE) ) {
			return s_ReaderFactories[i]->create();
		}
	}
	return NULL;
}

ImageReader* ImageReader::create(Storage* source)
{
	uint8_t signature[SIGNATURE_MAX_SIZE];
	if( source->peekSignature(signature) ) {
		ImageReader* imageReader = ImageReader::createFromSignature(signature);
		if( imageReader ) {
			return ImageReader::initReader(source, imageReader);
		}
	}
	return NULL;
}

ImageReader* ImageReader::initReader(Storage* source, ImageReader* imageReader)
{
	if( !imageReader ) {
		return NULL;
	}
	if( !imageReader->initWithStorage(source) ) {
		delete imageReader;
		return NULL;
	}
	if( !imageReader->readHeader() ) {
		delete imageReader;
		return NULL;
	}
	return imageReader;
}

unsigned int ImageReader::getOrientedWidth()
{
	EImageOrientation orientation = getOrientation();
	if( orientation == kImageOrientation_Left || orientation == kImageOrientation_Right ) {
		return getHeight();
	}
	return getWidth();
}

unsigned int ImageReader::getOrientedHeight()
{
	EImageOrientation orientation = getOrientation();
	if( orientation == kImageOrientation_Left || orientation == kImageOrientation_Right ) {
		return getWidth();
	}
	return getHeight();
}

EImageOrientation ImageReader::getOrientation()
{
	return kImageOrientation_Up;
}

EImageColorModel ImageReader::getNativeColorModel()
{
	return kColorModel_RGBX;
}

bool ImageReader::supportsOutputColorModel(EImageColorModel colorSpace)
{
	return Image::colorModelIsRGBA(colorSpace);
}

bool ImageReader::beginRead(unsigned int outputWidth, unsigned int outputHeight, EImageColorModel outputColorModel)
{
	return false;
}

unsigned int ImageReader::readRows(Image* destImage, unsigned int destRow, unsigned int numRows)
{
	return false;
}

bool ImageReader::endRead()
{
	return false;
}

void ImageReader::computeReadDimensions(unsigned int desiredWidth, unsigned int desiredHeight, unsigned int& readWidth, unsigned int& readHeight)
{
	readWidth = getWidth();
	readHeight = getHeight();
}

// ImageReader::FileStorage

ImageReader::FileStorage* ImageReader::FileStorage::open(const char* filePath)
{
	if( strcmp(filePath, "-") == 0 ) {
		return new FileStorage(stdin, false, false);
	} else {
		FILE* f = fopen(filePath, "rb");
		if( f != NULL) {
			return new FileStorage(f, true, true);
		}
	}
	return NULL;
}

ImageReader::FileStorage::FileStorage(FILE* file)
{
	m_MmapStorage = NULL;
	m_File = file;
	// Test and see if we can seek with this file descriptor.
	size_t basePos = ftell(m_File);
	if( fseek(m_File, basePos, SEEK_SET) == 0 ) {
		m_CanSeek = true;
	} else {
		m_CanSeek = false;
	}
	m_OwnsFile = false;
}

ImageReader::FileStorage::FileStorage(FILE* file, bool canSeek, bool ownsFile)
{
	m_MmapStorage = NULL;
	m_File = file;
	m_CanSeek = canSeek;
	m_OwnsFile = ownsFile;
}

ImageReader::FileStorage::~FileStorage()
{
	if( m_MmapStorage != NULL ) {
		delete m_MmapStorage;
		m_MmapStorage = NULL;
	}
	if( m_OwnsFile ) {
		fclose(m_File);
		m_File = NULL;
	}
}

uint64_t ImageReader::FileStorage::read(void* destBuffer, uint64_t numBytes)
{
	return (unsigned int)fread(destBuffer, 1, (size_t)numBytes, m_File);
}

uint64_t ImageReader::FileStorage::tell()
{
	return ftell(m_File);
}

bool ImageReader::FileStorage::seek(int64_t pos, SeekMode mode)
{
	int whence = mode == kSeek_Set ? SEEK_SET : (mode == kSeek_Current ? kSeek_Current : kSeek_End);
	if( m_CanSeek && fseek(m_File, (long)pos, whence) == 0 ) {
		return true;
	}
	return false;
}

bool ImageReader::FileStorage::canSeek()
{
	return m_CanSeek;
}

bool ImageReader::FileStorage::asFile(FILE*& file)
{
	file = m_File;
	return true;
}

bool ImageReader::FileStorage::asBuffer(uint8_t*& buffer, uint64_t& length)
{
	buffer = NULL;
	length = 0;
	if( m_MmapStorage == NULL && m_CanSeek ) {
		m_MmapStorage = MemoryMappedStorage::map(m_File);
	}
	return m_MmapStorage != NULL ? m_MmapStorage->asBuffer(buffer, length) : false;
}

bool ImageReader::FileStorage::peekSignature(uint8_t* signature)
{
	if( signature != NULL ) {
		if( m_CanSeek ) {
			// We can seek, so just read the signature and seek back.
			size_t basePos = ftell(m_File);
			if( fread(signature, 1, SIGNATURE_MAX_SIZE, m_File) != SIGNATURE_MAX_SIZE ) {
				return false;
			}
			if( fseek(m_File, basePos, SEEK_SET) != 0 ) {
				return false;
			}
			return true;
		} else {
			// Can't seek back, probably stdin. Put the signature back into the stream buffer.
			for( int i = 0; i < SIGNATURE_MAX_SIZE; ++i ) {
				int b = fgetc(m_File);
				if( b != EOF ) {
					signature[i] = b;
				} else {
					return false;
				}
			}
			for( int i = SIGNATURE_MAX_SIZE - 1; i >= 0; --i ) {
				ungetc(signature[i], m_File);
			}
		}
		return true;
	}
	return false;
}

// ImageReader::MemoryStorage

ImageReader::MemoryStorage::MemoryStorage(void* buffer, uint64_t length, bool ownsBuffer)
{
	m_Buffer = (uint8_t*)buffer;
	m_UsedBytes = 0;
	m_TotalBytes = length;
	m_OwnsBuffer = ownsBuffer;
}

ImageReader::MemoryStorage::~MemoryStorage()
{
	if( m_OwnsBuffer ) {
		free(m_Buffer);
		m_Buffer = NULL;
	}
}

uint64_t ImageReader::MemoryStorage::read(void* destBuffer, uint64_t numBytes)
{
	uint64_t bytesToRead = numBytes;
	if( SafeUAdd(m_UsedBytes, numBytes) > m_TotalBytes ) {
		bytesToRead = SafeUSub(m_TotalBytes, m_UsedBytes);
	}
	if( bytesToRead > 0 ) {
		memcpy(destBuffer, m_Buffer + m_UsedBytes, (size_t)bytesToRead);
		m_UsedBytes = SafeUAdd(m_UsedBytes, bytesToRead);
	}
	return bytesToRead;
}

uint64_t ImageReader::MemoryStorage::tell()
{
	return m_UsedBytes;
}

bool ImageReader::MemoryStorage::seek(int64_t pos, SeekMode mode)
{
	if( mode == kSeek_Set ) {
		if( pos > (int64_t)m_TotalBytes || pos < 0 ) {
			return false;
		}
		m_UsedBytes = pos;
	} else if( mode == kSeek_Current ) {
		if( m_UsedBytes + pos > m_TotalBytes || (int64_t)m_UsedBytes + pos < 0 ) {
			return false;
		}
		m_UsedBytes = SafeUAdd(m_UsedBytes, pos);
	} else if( mode == kSeek_End ) {
		if( m_TotalBytes - pos > m_TotalBytes || (int64_t)m_TotalBytes - pos < 0 ) {
			return false;
		}
		m_UsedBytes = SafeUSub(m_TotalBytes, pos);
	}
	return true;
}

bool ImageReader::MemoryStorage::canSeek()
{
	return true;
}

bool ImageReader::MemoryStorage::asFile(FILE*& file)
{
	return false;
}

bool ImageReader::MemoryStorage::asBuffer(uint8_t*& buffer, uint64_t& length)
{
	buffer = m_Buffer;
	length = m_TotalBytes;
	return true;
}

bool ImageReader::MemoryStorage::peekSignature(uint8_t* signature)
{
	uint64_t remainingBytes = SafeUSub(m_TotalBytes, m_UsedBytes);
	if( remainingBytes >= SIGNATURE_MAX_SIZE ) {
		memcpy(signature, m_Buffer, SIGNATURE_MAX_SIZE);
		return true;
	}
	return false;
}

// ImageReader::MemoryMappedStorage

ImageReader::MemoryMappedStorage* ImageReader::MemoryMappedStorage::map(FILE* f)
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

ImageReader::MemoryMappedStorage* ImageReader::MemoryMappedStorage::map(int fd, uint64_t length)
{
	void* mapBuffer = mmap(NULL, (size_t)length, PROT_READ, MAP_SHARED, fd, 0);
	if( mapBuffer != MAP_FAILED ) {
		return new MemoryMappedStorage(mapBuffer, length);
	}
	return NULL;
}

ImageReader::MemoryMappedStorage::MemoryMappedStorage(void* buffer, uint64_t length)
:	MemoryStorage(buffer, length)
{
}

ImageReader::MemoryMappedStorage::~MemoryMappedStorage()
{
	munmap(m_Buffer, (size_t)m_TotalBytes);
	m_Buffer = NULL;
}

}
