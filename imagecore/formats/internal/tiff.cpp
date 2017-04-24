//
//  tiff.cpp
//  ImageTool
//
//  Created by Luke Alonso on 06/25/14.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#include "tiff.h"
#include "imagecore/image/rgba.h"
#include "imagecore/formats/writer.h"
#include "imagecore/utils/mathutils.h"
#include "imagecore/utils/securemath.h"

namespace imagecore {

REGISTER_IMAGE_READER(ImageReaderTIFF);

static void TIFFSilentWarningHandler( const char*, const char*, va_list ) {}

tsize_t tiffRead(thandle_t handle, tdata_t buffer, tsize_t size)
{
	ImageReader::Storage* storage = (ImageReader::Storage*)handle;
	return (tsize_t)storage->read(buffer, size);
}

tsize_t tiffWrite(thandle_t handle, tdata_t buffer, tsize_t size)
{
	ASSERT(0);
	return 0;
}

int tiffClose(thandle_t handle)
{
    return 0;
}

toff_t tiffSeek(thandle_t handle, toff_t pos, int whence)
{
    if( pos == 0xFFFFFFFF ) {
		return 0xFFFFFFFF;
	}
	ImageReader::Storage* storage = (ImageReader::Storage*)handle;
	if( whence == SEEK_CUR ) {
		storage->seek(pos, ImageReader::Storage::kSeek_Current);
	} else if( whence == SEEK_END ) {
		storage->seek(pos, ImageReader::Storage::kSeek_End);
	} else {
		storage->seek(pos, ImageReader::Storage::kSeek_Set);
	}
	return (toff_t)storage->tell();
}

toff_t tiffSize(thandle_t handle)
{
	ImageReader::Storage* storage = (ImageReader::Storage*)handle;
	uint64_t pos = storage->tell();
	storage->seek(0, ImageReader::Storage::kSeek_End);
	uint64_t size = storage->tell();
	storage->seek(pos, ImageReader::Storage::kSeek_Set);
	return (toff_t)size;
}

int tiffMap(thandle_t, tdata_t*, toff_t*)
{
    return 0;
}

void tiffUnmap(thandle_t, tdata_t, toff_t)
{
    return;
}

bool ImageReaderTIFF::Factory::matchesSignature(const uint8_t* sig, unsigned int sigLen)
{
	if( sigLen >= 2 && ((sig[0] == 'I' && sig[1] == 'I') || (sig[0] == 'M' && sig[1] == 'M')) ) {
		return true;
	}
	return false;
}

ImageReaderTIFF::ImageReaderTIFF()
:	m_Source(NULL)
,	m_TempSource(NULL)
,	m_TempStorage(NULL)
,	m_Width(0)
,	m_Height(0)
,	m_HasAlpha(false)
{
}

ImageReaderTIFF::~ImageReaderTIFF()
{
	if( m_Tiff != NULL ) {
		TIFFClose(m_Tiff);
		m_Tiff = NULL;
	}
	delete m_TempSource;
	m_TempSource = NULL;
	delete m_TempStorage;
	m_TempStorage = NULL;
}

bool ImageReaderTIFF::initWithStorage(ImageReader::Storage* source)
{
	m_Source = source;
	// libtiff requires random access, so if we have a non-seekable source, it needs to be fully read and copied.
	if( !m_Source->canSeek() ) {
		m_TempStorage = new ImageWriter::MemoryStorage();
		uint8_t buffer[1024];
		long totalBytesRead = 0;
		long bytesRead = 0;
		do {
			bytesRead = (long)source->read(buffer, 1024);
			if( bytesRead > 0 ) {
				m_TempStorage->write(buffer, bytesRead);
			}
			totalBytesRead += bytesRead;
		} while ( bytesRead > 0 );

		uint8_t* storageBuffer;
		uint64_t storageLength;
		if( m_TempStorage->asBuffer(storageBuffer, storageLength) ) {
			m_TempSource = new MemoryStorage(storageBuffer, totalBytesRead);
			m_Source = m_TempSource;
		}
	}
	return true;
}

bool ImageReaderTIFF::readHeader()
{
	TIFFSetErrorHandler(TIFFSilentWarningHandler);
	TIFFSetWarningHandler(TIFFSilentWarningHandler);
	// Don't allow memory mapping, read only.
	m_Tiff = TIFFClientOpen("None", "rm", m_Source, tiffRead, tiffWrite, tiffSeek, tiffClose, tiffSize, tiffMap, tiffUnmap);
	int width = 0;
	int height = 0;
	if( m_Tiff == NULL ) {
		return false;
	}
	if( TIFFGetField(m_Tiff, TIFFTAG_IMAGEWIDTH, &width) == 0 || TIFFGetField(m_Tiff, TIFFTAG_IMAGELENGTH, &height) == 0 ) {
		return false;
	}
	m_Width = width > 0 ? width : 0;
	m_Height = height > 0 ? height : 0;
	return true;
}

bool ImageReaderTIFF::readImage(Image* dest)
{
	if( !supportsOutputColorModel(dest->getColorModel()) ) {
		return false;
	}

	ImageRGBA* destImage = dest->asRGBA();

	char err[1024];
	if( TIFFRGBAImageOK(m_Tiff, err) == 0 ) {
		fprintf(stderr, "error reading TIFF: '%s'\n", err);
	}

	TIFFRGBAImage tiffImage;
	memset(&tiffImage, 0, sizeof(TIFFRGBAImage));
	if( TIFFRGBAImageBegin(&tiffImage, m_Tiff, 1, err) == 0 ) {
		fprintf(stderr, "error reading TIFF: '%s'\n", err);
	}

	m_HasAlpha = tiffImage.alpha > 0;

	bool success = false;
	ImageRGBA* tempImage = ImageRGBA::create(m_Width, m_Height);
	if( tempImage != NULL ) {
		unsigned int pitch = 0;
		uint8_t* buffer = tempImage->lockRect(m_Width, m_Height, pitch);
		TIFFReadRGBAImageOriented(m_Tiff, m_Width, m_Height, (uint32*)buffer, ORIENTATION_TOPLEFT, 1);
		tempImage->unlockRect();
		tempImage->copy(destImage);
		delete tempImage;
		success = true;
	}
	TIFFRGBAImageEnd(&tiffImage);
	return success;
}

EImageFormat ImageReaderTIFF::getFormat()
{
	return kImageFormat_TIFF;
}

const char* ImageReaderTIFF::getFormatName()
{
	return "TIFF";
}

unsigned int ImageReaderTIFF::getWidth()
{
	return m_Width;
}

unsigned int ImageReaderTIFF::getHeight()
{
	return m_Height;
}

EImageColorModel ImageReaderTIFF::getNativeColorModel()
{
	return m_HasAlpha ? kColorModel_RGBA : kColorModel_RGBX;
}

}
