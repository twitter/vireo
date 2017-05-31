//
//  png.cpp
//  ImageTool
//
//  Created by Luke Alonso on 10/24/12.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#include "png.h"
#include "imagecore/utils/mathutils.h"
#include "imagecore/utils/securemath.h"
#include "imagecore/image/interleaved.h"

#include "libpng16/png.h"
#include <stdlib.h>

namespace imagecore {

REGISTER_IMAGE_READER(ImageReaderPNG);
REGISTER_IMAGE_WRITER(ImageWriterPNG);

void pngRead(png_structp png_ptr, png_bytep outBuffer, png_size_t numBytes)
{
	ImageReader::Storage* io = (ImageReader::Storage*)png_get_io_ptr(png_ptr);
	uint64_t bytesRead = io->read(outBuffer, (unsigned int)numBytes);
	if( bytesRead != numBytes ) {
		png_error(png_ptr, "EOF");
		return;
	}
}

bool ImageReaderPNG::Factory::matchesSignature(const uint8_t* sig, unsigned int sigLen)
{
	if( png_sig_cmp(sig, 0, sigLen) == 0 ) {
		return true;
	}
	return false;
}

ImageReaderPNG::ImageReaderPNG()
:	m_Source(NULL)
,	m_Width(0)
,	m_Height(0)
,	m_TotalRowsRead(0)
,	m_NativeColorModel(kColorModel_RGBX)
,	m_PNGDecompress(NULL)
,	m_PNGInfo(NULL)
{
	m_PNGDecompress = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	m_PNGInfo = png_create_info_struct(m_PNGDecompress);
}

ImageReaderPNG::~ImageReaderPNG()
{
	if( m_PNGDecompress != NULL ) {
		png_destroy_read_struct(&m_PNGDecompress, &m_PNGInfo, NULL);
		m_PNGDecompress = NULL;
		m_PNGInfo = NULL;
	}
}

bool ImageReaderPNG::initWithStorage(Storage* source)
{
	m_Source = source;
	return true;
}

bool ImageReaderPNG::readHeader()
{
	if( setjmp(png_jmpbuf(m_PNGDecompress)) ) {
		return false;
	}
	png_set_read_fn(m_PNGDecompress, m_Source, pngRead);
	png_read_info(m_PNGDecompress, m_PNGInfo);
	m_Width = png_get_image_width(m_PNGDecompress, m_PNGInfo);
	m_Height = png_get_image_height(m_PNGDecompress, m_PNGInfo);
	unsigned int colorType = png_get_color_type(m_PNGDecompress, m_PNGInfo);
	if( colorType == PNG_COLOR_TYPE_GRAY ) {
		// Gray only, not gray with alpha.
		m_NativeColorModel = kColorModel_Grayscale;
	} else if( colorType == PNG_COLOR_TYPE_RGB_ALPHA || colorType == PNG_COLOR_TYPE_GRAY_ALPHA || png_get_valid(m_PNGDecompress, m_PNGInfo, PNG_INFO_tRNS) ) {
		m_NativeColorModel = kColorModel_RGBA;
	} else {
		m_NativeColorModel = kColorModel_RGBX;
	}
	return true;
}

bool ImageReaderPNG::beginRead(unsigned int outputWidth, unsigned int outputHeight, EImageColorModel outputColorModel)
{
	if( !supportsOutputColorModel(outputColorModel) ) {
		return false;
	}
	if( outputWidth != m_Width || outputHeight != m_Height ) {
		return false;
	}
	if( setjmp(png_jmpbuf(m_PNGDecompress)) ) {
		return false;
	}
	unsigned int color_type = png_get_color_type(m_PNGDecompress, m_PNGInfo);
	unsigned int bit_depth = png_get_bit_depth(m_PNGDecompress, m_PNGInfo);
	if( bit_depth == 16 ) {
		png_set_strip_16(m_PNGDecompress);
	}
	if( color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8 ) {
		png_set_expand_gray_1_2_4_to_8(m_PNGDecompress);
	}
	if( Image::colorModelIsRGBA(outputColorModel) ) {
		if( color_type == PNG_COLOR_TYPE_PALETTE ) {
			png_set_palette_to_rgb(m_PNGDecompress);
		}
		if( png_get_valid(m_PNGDecompress, m_PNGInfo, PNG_INFO_tRNS) ) {
			png_set_tRNS_to_alpha(m_PNGDecompress);
		} else if( color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE ) {
			png_set_filler(m_PNGDecompress, 255, PNG_FILLER_AFTER);
		}
		if( color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA ) {
			png_set_gray_to_rgb(m_PNGDecompress);
		}
	}

	png_read_update_info(m_PNGDecompress, m_PNGInfo);

	return true;
}

unsigned int ImageReaderPNG::readRows(Image* dest, unsigned int destRow, unsigned int numRows)
{
	if( !supportsOutputColorModel(dest->getColorModel()) ) {
		return false;
	}

	ImageInterleaved* destImage = dest->asInterleaved();

	if( png_get_interlace_type(m_PNGDecompress, m_PNGInfo) != PNG_INTERLACE_NONE ) {
		// We don't support sequential reading of interlaced PNGs.
		return false;
	}
	if( setjmp(png_jmpbuf(m_PNGDecompress)) ) {
		return false;
	}
	unsigned int destPitch = 0;
	uint8_t* destBuffer = destImage->lockRect(m_Width, numRows, destPitch);
	SECURE_ASSERT(!Image::colorModelIsRGBA(destImage->getColorModel()) || destImage->getComponentSize() == 4);
	SECURE_ASSERT(!Image::colorModelIsGrayscale(destImage->getColorModel()) || destImage->getComponentSize() == 1);
	SECURE_ASSERT(SafeUMul(m_Width, destImage->getComponentSize()) <= destPitch);
	SECURE_ASSERT(destBuffer && destPitch);
	unsigned int destHeight = destImage->getHeight();
	SECURE_ASSERT(destRow + numRows <= destHeight);
	unsigned int allocSize = SafeUMul((unsigned int)sizeof(png_bytep), numRows);
	png_bytep* rowPointers = (png_bytep*)malloc(allocSize);
	if( rowPointers == NULL ) {
		return false;
	}
	// libpng asks for an array of scanline pointers, into each of which it'll write width * size bytes.
	unsigned int finalRow = destRow + numRows;
	for( unsigned int y = destRow; y < finalRow; y++ ) {
		rowPointers[y] = destBuffer + destPitch * y;
	}
	png_read_rows(m_PNGDecompress, rowPointers, NULL, numRows);
	m_TotalRowsRead += numRows;
	return numRows;
}

bool ImageReaderPNG::endRead()
{
	if( setjmp(png_jmpbuf(m_PNGDecompress)) ) {
		return false;
	}
	if( m_TotalRowsRead == m_Height ) {
		png_read_end(m_PNGDecompress, m_PNGInfo);
	}
	png_destroy_read_struct(&m_PNGDecompress, &m_PNGInfo, NULL);
	return true;
}

bool ImageReaderPNG::readImage(Image* destImage)
{
	if( !supportsOutputColorModel(destImage->getColorModel()) ) {
		return false;
	}

	if( !beginRead(m_Width, m_Height, destImage->getColorModel()) ) {
		return false;
	}

	unsigned int destHeight = destImage->getHeight();
	unsigned int allocSize = SafeUMul((unsigned int)sizeof(png_bytep), destHeight);
	png_bytep* rowPointers = (png_bytep*)malloc(allocSize);
	if( rowPointers == NULL ) {
		return false;
	}
	if( setjmp(png_jmpbuf(m_PNGDecompress)) ) {
		free(rowPointers);
		return false;
	}

	ImageInterleaved* image = destImage->asInterleaved();

	// Security checks - make sure we don't exceed the dest capacity, and that nothing has somehow changed since we read the header.
	SECURE_ASSERT(destImage->getWidth() == m_Width && destImage->getHeight() == m_Height);
	SECURE_ASSERT(m_Width == png_get_image_width(m_PNGDecompress, m_PNGInfo) && m_Height == png_get_image_height(m_PNGDecompress, m_PNGInfo));

	SECURE_ASSERT(!Image::colorModelIsRGBA(destImage->getColorModel()) || image->getComponentSize() == 4);
	SECURE_ASSERT(!Image::colorModelIsGrayscale(destImage->getColorModel()) || image->getComponentSize() == 1);

	unsigned int destPitch = 0;
	uint8_t* destBuffer = image->lockRect(m_Width, m_Height, destPitch);

	SECURE_ASSERT(SafeUMul(m_Width, image->getComponentSize()) <= destPitch);
	SECURE_ASSERT(destBuffer && destPitch);
	// libpng asks for an array of scanline pointers, into each of which it'll write width * components * depth bytes.
	for( unsigned int y = 0; y < destHeight; y++ ) {
		rowPointers[y] = destBuffer + y * destPitch;
	}
	png_read_image(m_PNGDecompress, rowPointers);
	image->unlockRect();

	free(rowPointers);
	rowPointers = NULL;

	m_TotalRowsRead += m_Height;

	if( !endRead() ) {
		return false;
	}

	return true;
}

EImageFormat ImageReaderPNG::getFormat()
{
	return kImageFormat_PNG;
}

const char* ImageReaderPNG::getFormatName()
{
	return "PNG";
}

unsigned int ImageReaderPNG::getWidth()
{
	return m_Width;
}

unsigned int ImageReaderPNG::getHeight()
{
	return m_Height;
}

EImageColorModel ImageReaderPNG::getNativeColorModel()
{
	return m_NativeColorModel;
}

bool ImageReaderPNG::supportsOutputColorModel(EImageColorModel colorModel)
{
	return Image::colorModelIsRGBA(colorModel) || colorModel == m_NativeColorModel;
}

////

bool ImageWriterPNG::Factory::matchesExtension(const char *extension)
{
	return strcasecmp(extension, "png") == 0;
}

EImageFormat ImageWriterPNG::Factory::getFormat()
{
	return kImageFormat_PNG;
}

bool ImageWriterPNG::Factory::appropriateForInputFormat(EImageFormat inputFormat)
{
	return true;
}

bool ImageWriterPNG::Factory::supportsInputColorModel(EImageColorModel colorModel)
{
	return Image::colorModelIsGrayscale(colorModel) || Image::colorModelIsRGBA(colorModel);
}

ImageWriterPNG::ImageWriterPNG()
:	m_SourceReader(NULL)
,   m_WriteOptions(0)
{
}

ImageWriterPNG::~ImageWriterPNG()
{
}

static void png_write_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
	ImageWriter::Storage* storage = (ImageWriter::Storage*) png_get_io_ptr(png_ptr);
	storage->write(data, (unsigned int) length);
}

static void png_flush(png_structp png_ptr)
{
	ImageWriter::Storage* storage = (ImageWriter::Storage*) png_get_io_ptr(png_ptr);
	storage->flush();
}

bool ImageWriterPNG::initWithStorage(ImageWriter::Storage* output)
{
	m_PNGCompress = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	m_PNGInfo = png_create_info_struct(m_PNGCompress);
	if( setjmp(png_jmpbuf(m_PNGCompress)) ) {
		return false;
	}
	png_set_write_fn(m_PNGCompress, output, png_write_data, png_flush);
	return true;
}

void ImageWriterPNG::setSourceReader(ImageReader* hintReader)
{
	m_SourceReader = hintReader;
}

void ImageWriterPNG::setWriteOptions(unsigned int options)
{
	m_WriteOptions |= options;
}

bool ImageWriterPNG::copyLossless(ImageReader* reader)
{
	if( reader->getFormat() != EImageFormat::kImageFormat_PNG ) {
		bool ret = ImageWriter::copyLossless(reader); // call baseclass for non png->png cleans
		if (!ret) {
			png_destroy_write_struct(&m_PNGCompress, &m_PNGInfo);
		}
		return ret;
	}

	ImageReaderPNG* readerPNG = (ImageReaderPNG*)reader;
	if( setjmp(png_jmpbuf(readerPNG->m_PNGDecompress)) ) {
		png_destroy_read_struct(&readerPNG->m_PNGDecompress, &readerPNG->m_PNGInfo, NULL);
		png_destroy_write_struct(&m_PNGCompress, &m_PNGInfo);
		return false;
	}

	uint32_t pitch = (uint32_t)png_get_rowbytes(readerPNG->m_PNGDecompress, readerPNG->m_PNGInfo);

	int32_t width;
	int32_t height;
	int32_t bitDepth;
	int32_t colorType;
	int32_t interlace;
	int32_t compressionType;
	int32_t filterMethod; // this is actually filter method not filter type, even though libpng docs refer to it as filter type. The actual filter type needs to be set separately using png_set_filter function
	png_get_IHDR(readerPNG->m_PNGDecompress, readerPNG->m_PNGInfo, (png_uint_32*)&width, (png_uint_32*)&height, &bitDepth, &colorType, &interlace, &compressionType, &filterMethod);

	uint32_t allocSize = SafeUMul(pitch, height);
	uint8_t* imageData = (uint8_t*)malloc(allocSize);
	if( imageData == NULL ) {
		png_destroy_read_struct(&readerPNG->m_PNGDecompress, &readerPNG->m_PNGInfo, NULL);
		png_destroy_write_struct(&m_PNGCompress, &m_PNGInfo);
		return false;
	}
	allocSize = SafeUMul((unsigned int)sizeof(png_bytep), height);
	png_bytep* rowPointers = (png_bytep*)malloc(allocSize);
	if( rowPointers == NULL ) {
		png_destroy_read_struct(&readerPNG->m_PNGDecompress, &readerPNG->m_PNGInfo, NULL);
		png_destroy_write_struct(&m_PNGCompress, &m_PNGInfo);
		free(imageData);
		return false;
	}

	// libpng asks for an array of scanline pointers, into each of which it'll write width * components * depth bytes.
	for( unsigned int y = 0; y < height; y++ ) {
		const uint32_t rowOffset = SafeUMul(y, pitch);
		rowPointers[y] = imageData + rowOffset;
	}

	if( setjmp(png_jmpbuf(readerPNG->m_PNGDecompress)) ) {
		png_destroy_read_struct(&readerPNG->m_PNGDecompress, &readerPNG->m_PNGInfo, NULL);
		png_destroy_write_struct(&m_PNGCompress, &m_PNGInfo);
		free(imageData);
		free(rowPointers);
		return false;
	}
	png_read_image(readerPNG->m_PNGDecompress, rowPointers);

	png_set_IHDR(m_PNGCompress, m_PNGInfo, width, height, bitDepth, colorType, PNG_INTERLACE_NONE, compressionType, filterMethod);
	uint32_t filterType = PNG_ALL_FILTERS;
	if( colorType == PNG_COLOR_TYPE_PALETTE ) {
		png_colorp palette;
		int32_t numEntries;
		png_get_PLTE(readerPNG->m_PNGDecompress, readerPNG->m_PNGInfo, &palette, &numEntries);
		png_set_PLTE(m_PNGCompress, m_PNGInfo, palette, numEntries);
		png_bytep atRNS = NULL;
		png_color_16p ctRNS = NULL;
		int numtRNS = 0;
		png_get_tRNS(readerPNG->m_PNGDecompress, readerPNG->m_PNGInfo, &atRNS, &numtRNS, &ctRNS);
		png_set_tRNS(m_PNGCompress, m_PNGInfo, atRNS, numtRNS, ctRNS);
		filterType = 0;
	}

	if( setjmp(png_jmpbuf(m_PNGCompress)) ) {
		png_destroy_read_struct(&readerPNG->m_PNGDecompress, &readerPNG->m_PNGInfo, NULL);
		png_destroy_write_struct(&m_PNGCompress, &m_PNGInfo);
		free(rowPointers);
		free(imageData);
		return false;
	}

	png_write_info(m_PNGCompress, m_PNGInfo);

	png_set_filter(m_PNGCompress, PNG_FILTER_TYPE_BASE, filterType);
	png_set_compression_level(m_PNGCompress, 9); // maximum compression, tests showed 6 out of 13400 png images had to be cleaned, because transform produced higher sizes.

	png_write_rows(m_PNGCompress, rowPointers, height);

	readerPNG->endRead();

	png_write_end(m_PNGCompress, NULL);
	png_destroy_write_struct(&m_PNGCompress, &m_PNGInfo);
	free(rowPointers);
	free(imageData);
	return true;
}

bool ImageWriterPNG::beginWrite(unsigned int width, unsigned int height, EImageColorModel colorModel)
{
	if( setjmp(png_jmpbuf(m_PNGCompress)) ) {
		return false;
	}
	if( !Image::colorModelIsRGBA(colorModel) && !Image::colorModelIsGrayscale(colorModel) ) {
		return false;
	}
	unsigned int colorType = 0;
	if( colorModel == kColorModel_RGBX && m_SourceReader != NULL && m_SourceReader->getNativeColorModel() == kColorModel_RGBA ) {
		colorType = PNG_COLOR_TYPE_RGBA;
	} else if( colorModel == kColorModel_RGBA && m_SourceReader != NULL && m_SourceReader->getNativeColorModel() == kColorModel_RGBX ) {
		colorType = PNG_COLOR_TYPE_RGB;
	} else if( colorModel == kColorModel_RGBA ) {
		colorType = PNG_COLOR_TYPE_RGBA;
	} else if( colorModel == kColorModel_RGBX ) {
		colorType = PNG_COLOR_TYPE_RGB;
	} else if( colorModel == kColorModel_Grayscale ) {
		colorType = PNG_COLOR_TYPE_GRAY;
	} else {
		SECURE_ASSERT(0);
	}
	png_set_IHDR(m_PNGCompress, m_PNGInfo, width, height, 8, colorType, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_write_info(m_PNGCompress, m_PNGInfo);
	applyCompressionSettings();
	if( colorType == PNG_COLOR_TYPE_RGB ) {
		png_set_filler(m_PNGCompress, 0, PNG_FILLER_AFTER);
	}

	return true;
}

unsigned int ImageWriterPNG::writeRows(Image* source, unsigned int sourceRow, unsigned int numRows)
{
	EImageColorModel colorModel = source->getColorModel();
	if( !Image::colorModelIsInterleaved(colorModel) ) {
		return false;
	}

	if( setjmp(png_jmpbuf(m_PNGCompress)) ) {
		return false;
	}
	unsigned int allocSize = SafeUMul((unsigned int)sizeof(png_bytep), numRows);
	png_bytep* rowPointers = (png_bytep*)malloc(allocSize);
	if( rowPointers == NULL ) {
		return false;
	}
	const uint8_t* sourceBuffer = source->asInterleaved()->getBytes();
	unsigned int sourcePitch = source->asInterleaved()->getPitch();
	unsigned int finalRow = sourceRow + numRows;
	for( unsigned int y = sourceRow; y < finalRow; y++ ) {
		rowPointers[y] = (uint8_t*)(sourceBuffer + sourcePitch * y);
	}
	png_write_rows(m_PNGCompress, rowPointers, numRows);
	free(rowPointers);
	return numRows;
}

bool ImageWriterPNG::endWrite()
{
	if( setjmp(png_jmpbuf(m_PNGCompress)) ) {
		return false;
	}
	png_write_end(m_PNGCompress, NULL);
	png_destroy_write_struct(&m_PNGCompress, &m_PNGInfo);
	return true;
}

bool ImageWriterPNG::writeImage(Image* sourceImage)
{
	unsigned int sourceWidth = sourceImage->getWidth();
	unsigned int sourceHeight = sourceImage->getHeight();
	if( !beginWrite(sourceWidth, sourceHeight, sourceImage->getColorModel()) ) {
		return false;
	}
	if( writeRows(sourceImage, 0, sourceHeight) != sourceHeight ) {
		return false;
	}
	if( !endWrite() ) {
		return false;
	}
	return true;
}

void ImageWriterPNG::applyCompressionSettings()
{
	if( m_WriteOptions & ImageWriter::kWriteOption_ForcePNGRunLengthEncoding ) {
		// specialized settings for images that have large areas of fixed color gradient
		png_set_filter(m_PNGCompress, PNG_FILTER_TYPE_BASE, PNG_ALL_FILTERS);
		png_set_compression_level(m_PNGCompress, 4);
		png_set_compression_strategy(m_PNGCompress, 3);
	} else {
		// Learned parameters that provide the best size / speed performance for typical uploads.
		png_set_filter(m_PNGCompress, PNG_FILTER_TYPE_BASE, PNG_FILTER_SUB | PNG_FILTER_UP);
		png_set_compression_level(m_PNGCompress, 4);
		png_set_compression_strategy(m_PNGCompress, 0);
	}
}

}
