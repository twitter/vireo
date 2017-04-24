//
//  webp.cpp
//  ImageCore
//
//  Created by Luke Alonso on 2/18/15.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#include "webp.h"
#include "imagecore/utils/mathutils.h"
#include "imagecore/utils/securemath.h"
#include "imagecore/image/rgba.h"
#include "imagecore/image/yuv.h"
#include <string.h>

namespace imagecore {

REGISTER_IMAGE_READER(ImageReaderWebP);
REGISTER_IMAGE_WRITER(ImageWriterWebP);

bool ImageReaderWebP::Factory::matchesSignature(const uint8_t* sig, unsigned int sigLen)
{
	if( sigLen >= 4 && sig[0] == 0x52 && sig[1] == 0x49 && sig[2] == 0x46 && sig[3] == 0x46 ) {
		return true;
	}
	return false;
}

ImageReaderWebP::ImageReaderWebP()
:	m_Source(NULL)
,	m_Width(0)
,	m_Height(0)
,	m_TotalRowsRead(0)
,	m_NativeColorModel(kColorModel_RGBX)
,	m_DecodeBuffer(NULL)
,	m_DecodeLength(0)
,	m_OwnDecodeBuffer(false)
{
}

ImageReaderWebP::~ImageReaderWebP()
{
	if( m_OwnDecodeBuffer ) {
		free(m_DecodeBuffer);
	}
}

bool ImageReaderWebP::initWithStorage(Storage* source)
{
	m_Source = source;
	return true;
}

bool ImageReaderWebP::readHeader()
{
	if( WebPInitDecoderConfig(&m_DecoderConfig) ) {
		if( m_Source->asBuffer(m_DecodeBuffer, m_DecodeLength) ) {
			m_OwnDecodeBuffer = false;
		} else {
			// WebP requires having the entire image in memory, so if the source was unable to provide a buffer, read it in.
			ImageWriter::MemoryStorage tempStorage;
			tempStorage.writeStream(m_Source);
			if( tempStorage.ownBuffer(m_DecodeBuffer, m_DecodeLength) ) {
				m_OwnDecodeBuffer = true;
			}
		}
		if( WebPGetFeatures(m_DecodeBuffer, (size_t)m_DecodeLength, &m_Features) == VP8_STATUS_OK ) {
			m_Width = m_Features.width > 0 ? m_Features.width : 0;
			m_Height = m_Features.height > 0 ? m_Features.height : 0;
			m_NativeColorModel = m_Features.has_alpha ? kColorModel_RGBA : kColorModel_YUV_420;
			return true;
		}
	}
	return false;
}

void ImageReaderWebP::computeReadDimensions(unsigned int desiredWidth, unsigned int desiredHeight, unsigned int& readWidth, unsigned int& readHeight)
{
	readWidth = m_Width;
	readHeight = m_Height;

	unsigned int reduceCount = 0;
	while( div2_round(readWidth) >= desiredWidth && div2_round(readHeight) >= desiredHeight && reduceCount < 2 ) {
		readWidth = div2_round(readWidth);
		readHeight = div2_round(readHeight);
		reduceCount++;
	}
}

bool ImageReaderWebP::beginRead(unsigned int outputWidth, unsigned int outputHeight, EImageColorModel outputColorModel)
{
	return false;
}

unsigned int ImageReaderWebP::readRows(Image* dest, unsigned int destRow, unsigned int numRows)
{
	return false;
}

bool ImageReaderWebP::endRead()
{
	return false;
}

bool ImageReaderWebP::readImage(Image* destImage)
{
	if( !supportsOutputColorModel(destImage->getColorModel()) ) {
		return false;
	}

	unsigned int destWidth = destImage->getWidth();
	unsigned int destHeight = destImage->getHeight();

	m_DecoderConfig.output.width = destWidth;
	m_DecoderConfig.output.height = destHeight;
	m_DecoderConfig.output.is_external_memory = true;
	m_DecoderConfig.options.use_threads = 0;
	m_DecoderConfig.options.no_fancy_upsampling = 0;

	if( destWidth != m_Width || destHeight != m_Height ) {
		m_DecoderConfig.options.use_scaling = 1;
		m_DecoderConfig.options.scaled_width = destWidth;
		m_DecoderConfig.options.scaled_height = destHeight;
	}

	EImageColorModel colorModel = destImage->getColorModel();

	bool success = false;

	if( Image::colorModelIsRGBA(colorModel)) {
		ImageInterleaved* image = destImage->asInterleaved();
		unsigned int destPitch = 0;

		uint8_t* destBuffer = image->lockRect(destWidth, destHeight, destPitch);

		SECURE_ASSERT(SafeUMul(destWidth, image->getComponentSize()) <= destPitch);
		m_DecoderConfig.output.colorspace = MODE_RGBA;
		m_DecoderConfig.output.u.RGBA.rgba = destBuffer;
		m_DecoderConfig.output.u.RGBA.size = image->getImageSize();
		m_DecoderConfig.output.u.RGBA.stride = destPitch;
		START_CLOCK(decodeRGB);
		if( WebPDecode(m_DecodeBuffer, (size_t)m_DecodeLength, &m_DecoderConfig) == VP8_STATUS_OK ) {
			success = true;
		}
		END_CLOCK(decodeRGB);
	} else if( Image::colorModelIsYUV(colorModel) ) {
		ImageYUV* image = destImage->asYUV();
		ImagePlane8* planeY = image->getPlaneY();
		ImagePlane8* planeU = image->getPlaneU();
		ImagePlane8* planeV = image->getPlaneV();
		EYUVRange desiredRange = image->getRange();

		unsigned int pitchY = 0;
		uint8_t* bufferY = planeY->lockRect(0, 0, planeY->getWidth(), planeY->getHeight(), pitchY);
		unsigned int pitchU = 0;
		uint8_t* bufferU = planeU->lockRect(0, 0, planeU->getWidth(), planeU->getHeight(), pitchU);
		unsigned int pitchV = 0;
		uint8_t* bufferV = planeV->lockRect(0, 0, planeV->getWidth(), planeV->getHeight(), pitchV);

		m_DecoderConfig.output.colorspace = MODE_YUV;
		m_DecoderConfig.output.u.YUVA.y = bufferY;
		m_DecoderConfig.output.u.YUVA.y_size = planeY->getImageSize();
		m_DecoderConfig.output.u.YUVA.y_stride = pitchY;
		m_DecoderConfig.output.u.YUVA.u = bufferU;
		m_DecoderConfig.output.u.YUVA.u_size = planeU->getImageSize();
		m_DecoderConfig.output.u.YUVA.u_stride = pitchU;
		m_DecoderConfig.output.u.YUVA.v = bufferV;
		m_DecoderConfig.output.u.YUVA.v_size = planeV->getImageSize();
		m_DecoderConfig.output.u.YUVA.v_stride = pitchV;

		START_CLOCK(decodeYUV);
		if( WebPDecode(m_DecodeBuffer, (size_t)m_DecodeLength, &m_DecoderConfig) == VP8_STATUS_OK ) {
			success = true;
		}
		END_CLOCK(decodeYUV);

		START_CLOCK(remap);
		image->setRange(kYUVRange_Compressed);
		if( desiredRange == kYUVRange_Full ) {
			image->expandRange(image);
		}
		END_CLOCK(remap);
	}

	m_TotalRowsRead += m_Height;
	return success;
}

EImageFormat ImageReaderWebP::getFormat()
{
	return kImageFormat_WebP;
}

const char* ImageReaderWebP::getFormatName()
{
	return "WebP";
}

unsigned int ImageReaderWebP::getWidth()
{
	return m_Width;
}

unsigned int ImageReaderWebP::getHeight()
{
	return m_Height;
}

EImageColorModel ImageReaderWebP::getNativeColorModel()
{
	return m_NativeColorModel;
}

bool ImageReaderWebP::supportsOutputColorModel(EImageColorModel colorModel)
{
	return Image::colorModelIsRGBA(colorModel) || colorModel == m_NativeColorModel;
}

//////

bool ImageWriterWebP::Factory::matchesExtension(const char *extension)
{
	return strcasecmp(extension, "webp") == 0;
}

EImageFormat ImageWriterWebP::Factory::getFormat()
{
	return kImageFormat_WebP;
}

bool ImageWriterWebP::Factory::appropriateForInputFormat(EImageFormat inputFormat)
{
	return true;
}

bool ImageWriterWebP::Factory::supportsInputColorModel(EImageColorModel colorModel)
{
	return Image::colorModelIsRGBA(colorModel) || Image::colorModelIsYUV(colorModel);
}

ImageWriterWebP::ImageWriterWebP()
:	m_SourceReader(NULL)
,	m_OutputStorage(NULL)
{
	WebPConfigPreset(&m_Config, WEBP_PRESET_PHOTO, 80.0f);
}

ImageWriterWebP::~ImageWriterWebP()
{
}

bool ImageWriterWebP::initWithStorage(ImageWriter::Storage* output)
{
	m_OutputStorage = output;
	return true;
}

void ImageWriterWebP::setSourceReader(ImageReader* hintReader)
{
	m_SourceReader = hintReader;
}

void ImageWriterWebP::setQuality(unsigned int quality)
{
	m_Config.quality = (float)quality;
}

bool ImageWriterWebP::applyExtraOptions(const char** optionNames, const char** optionValues, unsigned int numOptions)
{
	for( unsigned int i = 0; i < numOptions; i++ ) {
		if( strcasecmp(optionNames[i], "filter_strength") == 0) {
			m_Config.filter_strength = atoi(optionValues[i]);
		} else if( strcasecmp(optionNames[i], "filter_sharpness") == 0) {
			m_Config.filter_sharpness = atoi(optionValues[i]);
		} else if( strcasecmp(optionNames[i], "filter_type") == 0) {
			m_Config.filter_type = atoi(optionValues[i]);
		} else if( strcasecmp(optionNames[i], "method") == 0) {
			m_Config.method = atoi(optionValues[i]);
		} else if( strcasecmp(optionNames[i], "sns_strength") == 0) {
			m_Config.sns_strength = atoi(optionValues[i]);
		} else if( strcasecmp(optionNames[i], "preprocessing") == 0) {
			m_Config.preprocessing = atoi(optionValues[i]);
		} else if( strcasecmp(optionNames[i], "segments") == 0) {
			m_Config.segments = atoi(optionValues[i]);
		} else if( strcasecmp(optionNames[i], "partitions") == 0) {
			m_Config.partitions = atoi(optionValues[i]);
		} else if( strcasecmp(optionNames[i], "target_size") == 0) {
			m_Config.target_size = atoi(optionValues[i]);
		} else {
			return false;
		}
	}
	return true;
}

bool ImageWriterWebP::beginWrite(unsigned int width, unsigned int height, EImageColorModel colorModel)
{
	return false;
}

unsigned int ImageWriterWebP::writeRows(Image* source, unsigned int sourceRow, unsigned int numRows)
{
	return false;
}

bool ImageWriterWebP::endWrite()
{
	return false;
}

int webpWrite(const uint8_t* data, size_t size, const WebPPicture* picture)
{
	ImageWriter::Storage* storage = (ImageWriter::Storage*)picture->custom_ptr;
	return storage->write(data, size) == size;
}

bool ImageWriterWebP::writeImage(Image* sourceImage)
{
	if( !Image::colorModelIsRGBA(sourceImage->getColorModel()) && !Image::colorModelIsYUV(sourceImage->getColorModel()) ) {
		return false;
	}

	if( !WebPValidateConfig(&m_Config) ) {
		return false;
	}

	WebPPicture pic;
	if( !WebPPictureInit(&pic) ) {
		return false;
	}

	pic.width = sourceImage->getWidth();
	pic.height = sourceImage->getHeight();

	bool success = false;

	if( Image::colorModelIsRGBA(sourceImage->getColorModel()) ) {
		ImageRGBA* image = sourceImage->asRGBA();

		unsigned int pitch = image->getPitch();
		uint32_t* imageRGBA = (uint32_t*)image->getBytes();

		uint32_t* swapBufferARGB = (uint32_t*)malloc(image->getImageSize());
		if( swapBufferARGB != NULL ) {
			// Swap RGBA -> BGRA.
			for( unsigned int y = 0; y < pic.height; y++ ) {
				for( unsigned int x = 0; x < pic.width; x++ ) {
					unsigned int offset = (y * pitch / 4) + x;
					uint32_t rgba = imageRGBA[offset];
#if __BYTE_ORDER == __LITTLE_ENDIAN
					uint32_t argb = ((rgba & 0xFF) << 16) | ((rgba >> 16) & 0xFF) | (rgba & 0xFF00FF00);
#else
					uint32_t argb = (((rgba >> 24) & 0xFF) << 8) | (((rgba >> 8) & 0xFF) << 24) | (rgba & 0x00FF00FF);
#endif
					swapBufferARGB[offset] = argb;
				}
			}
			if( WebPPictureAlloc(&pic) ) {
				pic.writer = &webpWrite;
				pic.custom_ptr = m_OutputStorage;
				pic.use_argb = 1;
				pic.argb = (uint32_t*)swapBufferARGB;
				pic.argb_stride = image->getPitch() / image->getComponentSize();

				success = WebPEncode(&m_Config, &pic);

				WebPPictureFree(&pic);
			}
			free(swapBufferARGB);
		}
	} else if( Image::colorModelIsYUV(sourceImage->getColorModel()) ) {
		ImageYUV* yuvImage = sourceImage->asYUV();
		ImageYUV* image = yuvImage;
		ImageYUV* tempImage = NULL;
		if( yuvImage->getRange() == kYUVRange_Full ) {
			tempImage = ImageYUV::create(yuvImage->getWidth(), yuvImage->getHeight(), yuvImage->getPadding(), 16);
			if( tempImage == NULL ) {
				return 0;
			}
			START_CLOCK(remap);
			yuvImage->compressRange(tempImage);
			END_CLOCK(remap);
			image = tempImage;
		}

		if( (image->getHeight() & 1) == 1 ) {
			ImagePlane8* uPlane = image->getPlaneU();
			ImagePlane8* vPlane = image->getPlaneV();
			// The WebP codec seems to read into the UV padding region, make sure it's filled out correctly.
			uPlane->copyRect(uPlane, 0, uPlane->getHeight() - 2, 0, uPlane->getHeight() - 1, uPlane->getWidth(), 1);
			vPlane->copyRect(vPlane, 0, vPlane->getHeight() - 2, 0, vPlane->getHeight() - 1, vPlane->getWidth(), 1);
		}

		if( WebPPictureAlloc(&pic) ) {
			pic.writer = &webpWrite;
			pic.custom_ptr = m_OutputStorage;
			pic.use_argb = 0;
			pic.y = (uint8_t*)image->getPlaneY()->getBytes();
			pic.u = (uint8_t*)image->getPlaneU()->getBytes();
			pic.v = (uint8_t*)image->getPlaneV()->getBytes();
			pic.y_stride = image->getPlaneY()->getPitch();
			pic.uv_stride = image->getPlaneU()->getPitch();

			success = WebPEncode(&m_Config, &pic);

			WebPPictureFree(&pic);
		}

		delete tempImage;
	}

	return success;
}

}
