//
//  jpeg.cpp
//  ImageTool
//
//  Created by Luke Alonso on 10/24/12.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#include "jpeg.h"
#include "imagecore/utils/mathutils.h"
#include "imagecore/utils/securemath.h"
#include "imagecore/utils/memorystream.h"
#include "imagecore/image/rgba.h"
#include "imagecore/image/yuv.h"
#include "imagecore/formats/exif/exifwriter.h"

#include "jerror.h"
extern "C" {
	#include "iccjpeg.h"
#if IMAGECORE_WITH_JPEG_TRANSFORMS
	#include "transupp.h"
#endif
}

#if JPEG_LIB_VERSION >= 70
#define _DCT_h_scaled_size DCT_h_scaled_size
#define _DCT_v_scaled_size DCT_v_scaled_size
#else
#define _DCT_h_scaled_size DCT_scaled_size
#define _DCT_v_scaled_size DCT_scaled_size
#endif

#ifdef JCS_EXTENSIONS
#define HAVE_RGBX 1
#else
#warning libjpeg-turbo is strongly recommended, but not detected. performance will suffer.
#define HAVE_RGBX 0
#define JCS_EXT_RGBX (J_COLOR_SPACE)0
#endif

namespace imagecore {

REGISTER_IMAGE_READER(ImageReaderJPEG);
REGISTER_IMAGE_WRITER(ImageWriterJPEG);


#if IMAGECORE_WITH_LCMS

unsigned int getLCMSInputFormat(J_COLOR_SPACE colorSpace, bool sawAdobeMarker)
{
	if( colorSpace == JCS_GRAYSCALE ) {
		return TYPE_GRAY_8;
	}
	if( colorSpace == JCS_YCCK || colorSpace == JCS_CMYK ) {
		if( sawAdobeMarker ) {
			return TYPE_YUVK_8;
		} else {
			return TYPE_CMYK_8;
		}
	}
	return TYPE_RGBA_8;
}

#endif

static char s_JPEGLastError[JMSG_LENGTH_MAX] = "";

static void jpegError(j_common_ptr jinfo)
{
	// The address of JPEGErrorMgr::pub and JPEGErrorMgr are the same, so we can just cast
	// the pointer to the standard error mgr (pub) to ours.
	JPEGErrorMgr* err = (JPEGErrorMgr*)jinfo->err;
	longjmp(err->jmp, 1);
}

static void jpegMessage(j_common_ptr jinfo)
{
	(*jinfo->err->format_message)(jinfo, s_JPEGLastError);
}

bool ImageReaderJPEG::Factory::matchesSignature(const uint8_t* sig, unsigned int sigLen)
{
	if( sigLen >= 2 && sig[0] == 0xFF && sig[1] == 0xD8 ) {
		return true;
	}
	return false;
}

ImageReaderJPEG::ImageReaderJPEG()
:	m_Source(NULL)
,	m_SourceManager(NULL)
,	m_Width(0)
,	m_Height(0)
,	m_Orientation(kImageOrientation_Up)
,	m_TotalRowsRead(0)
,	m_EXIFData(NULL)
,	m_EXIFDataSize(0)
,	m_RawColorProfile(NULL)
,	m_ReadOptions(0)
,	m_MarkerReadError(false)
,	m_NativeColorModel(kColorModel_RGBX)
#if IMAGECORE_WITH_LCMS
,	m_ColorProfile(NULL)
,	m_sRGBProfile(NULL)
,	m_ColorTransform(NULL)
#endif
{
}

ImageReaderJPEG::~ImageReaderJPEG()
{
	free(m_EXIFData);
	m_EXIFData = NULL;
	m_EXIFDataSize = 0;

	if ( m_JPEGDecompress.global_state ) {
		m_JPEGDecompress.client_data = NULL;
		jpeg_abort_decompress(&m_JPEGDecompress);
		jpeg_destroy_decompress(&m_JPEGDecompress);
	}

	free(m_RawColorProfile);
	m_RawColorProfile = NULL;

	delete m_SourceManager;
	m_SourceManager = NULL;

#if IMAGECORE_WITH_LCMS
	if( m_ColorProfile != NULL ) {
		cmsCloseProfile(m_ColorProfile);
		m_ColorProfile = NULL;
	}
	if( m_sRGBProfile != NULL  ) {
		cmsCloseProfile(m_sRGBProfile);
		m_sRGBProfile = NULL;
	}
	if( m_ColorTransform != NULL  ) {
		cmsDeleteTransform(m_ColorTransform);
		m_ColorTransform = NULL;
	}
#endif
}

bool ImageReaderJPEG::initWithStorage(ImageReader::Storage* source)
{
	if( source == NULL ) {
		return false;
	}

	m_Source = source;
	// Set up custom error handler.
	m_JPEGDecompress.client_data = this;
	m_JPEGDecompress.err = jpeg_std_error(&m_JPEGError.pub);
	m_JPEGError.pub.error_exit = jpegError;
	m_JPEGError.pub.output_message = jpegMessage;
	if( setjmp(m_JPEGError.jmp) ) {
		fprintf(stderr, "error during jpeg init: %s\n", s_JPEGLastError);
		return false;
	}

	jpeg_create_decompress(&m_JPEGDecompress);

	FILE* sourceFile;
	uint8_t* buffer;
	uint64_t length;
	if( source->asBuffer(buffer, length) ) {
		jpeg_mem_src(&m_JPEGDecompress, buffer, (unsigned long)length);
	} else if( source->asFile(sourceFile) ) {
		jpeg_stdio_src(&m_JPEGDecompress, sourceFile);
	} else {
		m_SourceManager = new SourceManager(source, this);
		m_JPEGDecompress.src = m_SourceManager;
	}

	// EXIF
	jpeg_set_marker_processor(&m_JPEGDecompress, EXIF_MARKER, &handleJPEGMarker);

	// ICC Color Profile
	setup_read_icc_profile(&m_JPEGDecompress);

	return true;
}

unsigned int jpegRead(j_decompress_ptr dinfo, uint8_t* destBuffer, unsigned int numBytes)
{
	unsigned int remainingBytes = numBytes;
	while( remainingBytes > 0 ) {
		if( dinfo->src->bytes_in_buffer == 0 && !dinfo->src->fill_input_buffer(dinfo) ) {
			break;
		}
		unsigned int bytesToRead = min(remainingBytes, (unsigned int)dinfo->src->bytes_in_buffer);
		memcpy(destBuffer + (numBytes - remainingBytes), dinfo->src->next_input_byte, bytesToRead);
		dinfo->src->next_input_byte += bytesToRead;
		dinfo->src->bytes_in_buffer -= bytesToRead;
		remainingBytes -= bytesToRead;
	}
	return numBytes - remainingBytes;
}

boolean ImageReaderJPEG::handleJPEGMarker(j_decompress_ptr dinfo)
{
	ImageReaderJPEG* reader = (ImageReaderJPEG*)dinfo->client_data;
	uint8_t rawLength[2];
	jpegRead(dinfo, rawLength, 2);
	uint16_t segmentLength = (((uint16_t)rawLength[0] << 8) + (uint16_t)rawLength[1]) - 2;
	uint8_t* segmentData = (uint8_t*)malloc(segmentLength);
	if( segmentData == NULL ) {
		reader->m_MarkerReadError = true;
		return false;
	}
	jpegRead(dinfo, segmentData, segmentLength);
	reader->processJPEGSegment(dinfo->unread_marker, segmentData, segmentLength);
	return true;
}

void ImageReaderJPEG::processJPEGSegment(unsigned int marker, uint8_t* segmentData, unsigned int segmentLength)
{
	// Do the first few bytes equal "Exif"? Sometimes APP1 markers don't contain EXIF metadata.
	if( segmentLength >= 4 && marker == EXIF_MARKER && m_EXIFData == NULL && segmentData[0] == 0x45 && segmentData[1] == 0x78 && segmentData[2] == 0x69 && segmentData[3] == 0x66 ) {
		m_EXIFData = segmentData;
		m_EXIFDataSize = segmentLength;
		return;
	}

	free(segmentData);
}

bool ImageReaderJPEG::readHeader()
{
	if( setjmp(m_JPEGError.jmp) ) {
		fprintf(stderr, "error reading JPEG header: %s\n", s_JPEGLastError);
		return false;
	}

	jpeg_read_header(&m_JPEGDecompress, TRUE);

	if( m_MarkerReadError ) {
		return false;
	}

	if( m_JPEGDecompress.num_components == 3 ) {
		bool scaleY2 = m_JPEGDecompress.comp_info[0].h_samp_factor == 2 && m_JPEGDecompress.comp_info[0].v_samp_factor == 2;
		bool scaleU1 = m_JPEGDecompress.comp_info[1].h_samp_factor == 1 && m_JPEGDecompress.comp_info[1].v_samp_factor == 1;
		bool scaleV1 = m_JPEGDecompress.comp_info[2].h_samp_factor == 1 && m_JPEGDecompress.comp_info[2].v_samp_factor == 1;
		if( m_JPEGDecompress.jpeg_color_space == JCS_YCbCr && scaleY2 && scaleU1 && scaleV1 ) {
			m_NativeColorModel = kColorModel_YUV_420;
		}
	}

	m_Width = m_JPEGDecompress.image_width;
	m_Height = m_JPEGDecompress.image_height;

	if( m_EXIFData != NULL ) {
		ExifCommon::ExifString emptyStr;
		ExifCommon::ExifU64Rational3 defaultRational3;
		Rational<uint32_t> defaultRational;
		int8_t defaultAltitudeRef = 0;
		m_ExifReader.initialize(m_EXIFData + 6, m_EXIFDataSize - 6); // skip some JPEG data
		m_Orientation = (EImageOrientation)m_ExifReader.getValue((uint16_t)kImageOrientation_Up, ExifCommon::kTagId::kOrientation);
		m_GPSLatitudeRef = m_ExifReader.getValue(emptyStr, ExifCommon::kTagId::kGPSLatitudeRef);
		m_GPSLatitude = m_ExifReader.getValue(defaultRational3, ExifCommon::kTagId::kGPSLatitude);
		m_GPSLongitudeRef = m_ExifReader.getValue(emptyStr, ExifCommon::kTagId::kGPSLongitudeRef);
		m_GPSLongitude = m_ExifReader.getValue(defaultRational3, ExifCommon::kTagId::kGPSLongitude);
		m_AltitudeRef = (EAltitudeRef)m_ExifReader.getValue(defaultAltitudeRef, ExifCommon::kTagId::kGPSAltitudeRef);
		m_GPSAltitude = m_ExifReader.getValue(defaultRational, ExifCommon::kTagId::kGPSAltitude);
		m_GPSTimeStamp = m_ExifReader.getValue(defaultRational3, ExifCommon::kTagId::kGPSTimeStamp);
		m_GPSSpeedRef = m_ExifReader.getValue(emptyStr, ExifCommon::kTagId::kGPSSpeedRef);
		m_GPSSpeed = m_ExifReader.getValue(defaultRational, ExifCommon::kTagId::kGPSSpeed);
		m_GPSImgDirectionRef = m_ExifReader.getValue(emptyStr, ExifCommon::kTagId::kGPSImgDirectionRef);
		m_GPSImgDirection = m_ExifReader.getValue(defaultRational, ExifCommon::kTagId::kGPSImgDirection);
		m_GPSDestBearingRef = m_ExifReader.getValue(emptyStr, ExifCommon::kTagId::kGPSDestBearingRef);
		m_GPSDestBearing = m_ExifReader.getValue(defaultRational, ExifCommon::kTagId::kGPSDestBearing);
	}
	read_icc_profile(&m_JPEGDecompress, &m_RawColorProfile, &m_RawColorProfileSize);
	return true;
}

bool ImageReaderJPEG::beginReadInternal(unsigned int destWidth, unsigned int destHeight, EImageColorModel destColorModel)
{
	J_COLOR_SPACE outputColorSpace = destColorModel == kColorModel_YUV_420 ? JCS_YCbCr : (HAVE_RGBX ? JCS_EXT_RGBX : JCS_RGB);

#if IMAGECORE_WITH_LCMS
	if( m_RawColorProfileSize > 0 && (m_ReadOptions & kReadOption_ApplyColorProfile) != 0 && Image::colorModelIsRGBA(destColorModel) ) {
		m_ColorProfile = cmsOpenProfileFromMem(m_RawColorProfile, m_RawColorProfileSize);
		m_IgnoreColorProfile = true;
		if( m_ColorProfile != NULL ) {
			// Ignore mismatched color profiles.
			if( m_JPEGDecompress.jpeg_color_space != JCS_GRAYSCALE || cmsGetColorSpace(m_ColorProfile) == cmsSigGrayData ) {
				m_sRGBProfile = cmsCreate_sRGBProfile();
				if( m_sRGBProfile == NULL) {
					fprintf(stderr, "error: unable to allocate sRGB profile\n");
					return false;
				}

				unsigned int inputFormat = getLCMSInputFormat(m_JPEGDecompress.jpeg_color_space, m_JPEGDecompress.saw_Adobe_marker);
				m_ColorTransform = cmsCreateTransform(m_ColorProfile, inputFormat, m_sRGBProfile, HAVE_RGBX ? TYPE_RGBA_8 : TYPE_RGB_8, INTENT_PERCEPTUAL, 0);
				if( m_ColorTransform == NULL ) {
					fprintf(stderr, "error: unable to create ICC transform\n");
					return false;
				}

				if( m_JPEGDecompress.jpeg_color_space == JCS_GRAYSCALE ) {
					outputColorSpace = JCS_GRAYSCALE;
				} else if( m_JPEGDecompress.jpeg_color_space == JCS_YCCK || m_JPEGDecompress.jpeg_color_space == JCS_CMYK ) {
					outputColorSpace = JCS_CMYK;
				}

				m_IgnoreColorProfile = false;
			}
		}
	}
#endif

	m_JPEGDecompress.out_color_space = outputColorSpace;
	m_JPEGDecompress.scale_num = 1;
	m_JPEGDecompress.scale_denom = (unsigned int)roundf((float)m_Width / (float)destWidth);
	// This is the default.
	m_JPEGDecompress.dct_method = JDCT_ISLOW;

	if( (m_ReadOptions & kReadOption_DecompressQualityFast) != 0) {
		m_JPEGDecompress.do_fancy_upsampling = 0;
		m_JPEGDecompress.dct_method = JDCT_FASTEST;
		m_JPEGDecompress.do_block_smoothing = 0;
	}

	if( destColorModel == kColorModel_YUV_420 ) {
		m_JPEGDecompress.raw_data_out = 1;
	}

	if( setjmp(m_JPEGError.jmp) ) {
		fprintf(stderr, "error reading JPEG data: %s\n", s_JPEGLastError);
		jpeg_abort_decompress(&m_JPEGDecompress);
		return false;
	}

	jpeg_start_decompress(&m_JPEGDecompress);

	if( destColorModel == kColorModel_YUV_420 ) {
		// Make sure libjpeg doesn't try to upsample the UV components, we need them at half the resolution of Y.
		// Recompute all of the scaling parameters jpeg_start_decompress configured.
		for (int i = 1; i < 3; i++) {
			m_JPEGDecompress.comp_info[i]._DCT_h_scaled_size = m_JPEGDecompress.comp_info[0]._DCT_h_scaled_size;
			m_JPEGDecompress.comp_info[i]._DCT_v_scaled_size = m_JPEGDecompress.comp_info[0]._DCT_v_scaled_size;
			m_JPEGDecompress.comp_info[i].MCU_sample_width = m_JPEGDecompress.comp_info[0].MCU_sample_width / 2;
			m_JPEGDecompress.comp_info[i].downsampled_width = m_JPEGDecompress.comp_info[i].downsampled_width / 2;
			m_JPEGDecompress.comp_info[i].downsampled_height = m_JPEGDecompress.comp_info[i].downsampled_height / 2;
		}
		// Re-trigger the dct method and table calculations.
		int oldState = m_JPEGDecompress.global_state;
		m_JPEGDecompress.global_state = 207;
		jpeg_start_output(&m_JPEGDecompress, 1);
		m_JPEGDecompress.global_state = oldState;
	}

	if( m_JPEGDecompress.output_width != destWidth || m_JPEGDecompress.output_height != destHeight ) {
		fprintf(stderr, "JPEG scaled size mismatch");
		jpeg_abort_decompress(&m_JPEGDecompress);
		return false;
	}

	return true;
}

bool ImageReaderJPEG::beginRead(unsigned int outputWidth, unsigned int outputHeight, EImageColorModel outputColorModel)
{
	return beginReadInternal(outputWidth, outputHeight, outputColorModel);
}

bool ImageReaderJPEG::postProcessScanlines(uint8_t* buf, unsigned int size)
{
#if IMAGECORE_WITH_LCMS
	// Grayscale images can't operate on the same buffer, because of the different component count
	uint8_t* dest;
	if ( m_JPEGDecompress.jpeg_color_space == JCS_GRAYSCALE ) {
		dest = (uint8_t*)malloc(size);
		if( dest == NULL ) {
			fprintf(stderr, "error: out of memory applying color profile");
			return false;
		}
	} else {
		dest = buf;
	}

	cmsDoTransform(m_ColorTransform, buf, dest, size / 4);

	if( m_JPEGDecompress.jpeg_color_space == JCS_GRAYSCALE ) {
		memcpy(buf, dest, size);
		free(dest);
	}
	return true;
#else
	return false;
#endif
}

unsigned int ImageReaderJPEG::readRows(Image* dest, unsigned int destRow, unsigned int numRows)
{
	bool failed = true;

	if( Image::colorModelIsRGBA(dest->getColorModel()) ) {
		ImageRGBA* destImage = dest->asRGBA();

		unsigned int destHeight = destImage->getHeight();
		unsigned int destPitch = 0;
		SECURE_ASSERT(destRow + numRows <= destHeight);
		uint8_t* destBuffer = destImage->lockRect(0, destRow, destImage->getWidth(), numRows, destPitch);

		// Prepare row pointers, not essential, but it makes the following read loop simpler.
		uint8_t** rows = (uint8_t**)malloc(numRows * sizeof(uint8_t*));
		if( rows == NULL ) {
			fprintf(stderr, "error: allocation failed\n");
			jpeg_abort_decompress(&m_JPEGDecompress);
			jpeg_destroy_decompress(&m_JPEGDecompress);
			return 0;
		}

		if( setjmp(m_JPEGError.jmp) ) {
			fprintf(stderr, "error reading JPEG data: %s\n", s_JPEGLastError);
			free(rows);
			jpeg_abort_decompress(&m_JPEGDecompress);
			jpeg_destroy_decompress(&m_JPEGDecompress);
			return 0;
		}

#if !HAVE_RGBX
		unsigned int destWidth = destImage->getWidth();
		uint8_t* jpegBuffer = (uint8_t*)malloc(destWidth * numRows * 3);
		if( jpegBuffer == NULL ) {
			free(rows);
			jpeg_abort_decompress(&m_JPEGDecompress);
			jpeg_destroy_decompress(&m_JPEGDecompress);
			return 0;
		}
		unsigned int jpegPitch = destWidth * 3;
#else
		uint8_t* jpegBuffer = destBuffer;
		unsigned int jpegPitch = destPitch;
#endif

		for( unsigned int y = 0; y < numRows; y++ ) {
			rows[y] = jpegBuffer + jpegPitch * y;
		}

		unsigned int currentRow = 0;
		bool shouldProcessScanlines = false;

#if IMAGECORE_WITH_LCMS
		shouldProcessScanlines = (m_ReadOptions & kReadOption_ApplyColorProfile) != 0 && !m_IgnoreColorProfile && m_ColorProfile != NULL && m_ColorTransform != NULL;
#endif
		if( shouldProcessScanlines ) {
			while( currentRow < numRows ) {
				unsigned int numRowsRead = jpeg_read_scanlines(&m_JPEGDecompress, rows + currentRow, numRows - currentRow);
				if( !postProcessScanlines(destBuffer + (currentRow * destPitch), numRowsRead * destPitch) ) {
					break;
				}
				currentRow += numRowsRead;
				m_TotalRowsRead++;
			}
		} else {
			while( currentRow < numRows ) {
				unsigned int numRowsRead = jpeg_read_scanlines(&m_JPEGDecompress, rows + currentRow, numRows - currentRow);
				currentRow += numRowsRead;
				m_TotalRowsRead++;
			}
		}
#if !HAVE_RGBX
		// Provided purely for compatibility for vanilla libjpeg, not recommended.
		for (unsigned int y = 0; y < numRows; y++) {
			for (unsigned int x = 0; x < destWidth; x++) {
				uint8_t* in = jpegBuffer + y * jpegPitch + x * 3;
				uint8_t* out = destBuffer + y * destPitch + x * 4;
				out[0] = in[0];
				out[1] = in[1];
				out[2] = in[2];
			}
		}
		free(jpegBuffer);
#endif
		failed = false;
		destImage->unlockRect();
		free(rows);
	} else if ( Image::colorModelIsYUV(dest->getColorModel()) ) {
		// jpeg_read_raw_data requires you to handle the edge padding data inserted to align
		// the image to the DCT block sizes. Make sure the destination image has enough space in each
		// row beyond the normal image data. The bottom edge is handled by reading into junk buffers
		// and discarding the information.
		// TODO: Relax this by reading into temporary buffers, and copying.
		ImageYUV* destImage = dest->asYUV();
		EYUVRange desiredRange = destImage->getRange();
		unsigned int destPadding = destImage->getPadding();
		unsigned int dctAlignedPitchY = align(destImage->getPlaneY()->getWidth(), DCTSIZE * 2);
		unsigned int dctAlignedPitchU = align(destImage->getPlaneU()->getWidth(), DCTSIZE * 2);
		unsigned int dctAlignedPitchV = align(destImage->getPlaneV()->getWidth(), DCTSIZE * 2);
		unsigned int heightY = destImage->getPlaneY()->getHeight();
		unsigned int heightUV = destImage->getPlaneU()->getHeight();
		SECURE_ASSERT(destImage->getPlaneY()->getPitch() >= dctAlignedPitchY);
		SECURE_ASSERT(destImage->getPlaneU()->getPitch() >= dctAlignedPitchU);
		SECURE_ASSERT(destImage->getPlaneV()->getPitch() >= dctAlignedPitchV);

		ImagePlane8* planeY = destImage->getPlaneY();
		ImagePlane8* planeU = destImage->getPlaneU();
		ImagePlane8* planeV = destImage->getPlaneV();

		unsigned int pitchY = 0;
		uint8_t* bufferY = planeY->lockRect(0, destRow, planeY->getWidth(), numRows, pitchY);

		unsigned int pitchU = 0;
		uint8_t* bufferU = planeU->lockRect(0, destRow / 2, planeU->getWidth(), div2_round(numRows), pitchU);

		unsigned int pitchV = 0;
		uint8_t* bufferV = planeV->lockRect(0, destRow / 2, planeV->getWidth(), div2_round(numRows), pitchV);

		unsigned int currentRowY = 0;
		unsigned int currentRowUV = 0;
		constexpr unsigned int rowStepY = DCTSIZE * 2;
		constexpr unsigned int rowStepUV = DCTSIZE;

		if (destPadding < 16) {
			// If the image has insufficient padding to use to write the unused edge padding, allocate space for it.
			uint8_t* bottomEdgeJunkY = (uint8_t*)malloc(dctAlignedPitchY);
			uint8_t* bottomEdgeJunkU = (uint8_t*)malloc(dctAlignedPitchU);
			uint8_t* bottomEdgeJunkV = (uint8_t*)malloc(dctAlignedPitchV);
			if( bottomEdgeJunkY != NULL && bottomEdgeJunkU != NULL && bottomEdgeJunkV != NULL ) {
				while( currentRowY < numRows ) {
					JSAMPROW rowsY[rowStepY];
					JSAMPROW rowsU[rowStepY];
					JSAMPROW rowsV[rowStepY];
					for( unsigned int y = 0; y < rowStepY; y++ ) {
						unsigned int destY = y + currentRowY;
						if( destY < heightY ) {
							rowsY[y] = bufferY + pitchY * destY;
						} else {
							rowsY[y] = bottomEdgeJunkY;
						}
					}
					for( unsigned int y = 0; y < rowStepUV; y++ ) {
						unsigned int destUV = y + currentRowUV;
						if( destUV < heightUV ) {
							rowsU[y] = bufferU + pitchU * destUV;
							rowsV[y] = bufferV + pitchV * destUV;
						} else {
							rowsU[y] = bottomEdgeJunkU;
							rowsV[y] = bottomEdgeJunkV;
						}
					}
					JSAMPARRAY rows[] = { rowsY, rowsU, rowsV };
					unsigned int numRowsRead = jpeg_read_raw_data(&m_JPEGDecompress, rows, rowStepY);
					currentRowY += numRowsRead;
					currentRowUV += numRowsRead / 2;
					m_TotalRowsRead += numRowsRead;
				}
				failed = false;
			}
			free(bottomEdgeJunkY);
			free(bottomEdgeJunkU);
			free(bottomEdgeJunkV);
		} else {
			// Enough space to read the raw image.
			while( currentRowY < numRows ) {
				JSAMPROW rowsY[rowStepY];
				JSAMPROW rowsU[rowStepY];
				JSAMPROW rowsV[rowStepY];
				for( unsigned int y = 0; y < rowStepY; y++ ) {
					rowsY[y] = bufferY + pitchY * (y + currentRowY);
				}
				for( unsigned int y = 0; y < rowStepUV; y++ ) {
					rowsU[y] = bufferU + pitchU * (y + currentRowUV);
					rowsV[y] = bufferV + pitchV * (y + currentRowUV);
				}
				JSAMPARRAY rows[] = { rowsY, rowsU, rowsV };
				unsigned int numRowsRead = jpeg_read_raw_data(&m_JPEGDecompress, rows, rowStepY);
				currentRowY += numRowsRead;
				currentRowUV += numRowsRead / 2;
				m_TotalRowsRead += numRowsRead;
			}
			failed = false;
		}

		destImage->setRange(kYUVRange_Full);

		if( desiredRange == kYUVRange_Compressed ) {
			destImage->compressRange(destImage);
		}
	}

	if (failed) {
		jpeg_abort_decompress(&m_JPEGDecompress);
		jpeg_destroy_decompress(&m_JPEGDecompress);
		return 0;
	}

	return numRows;
}

bool ImageReaderJPEG::endRead()
{
	if( setjmp(m_JPEGError.jmp) ) {
		return false;
	}

	if( m_TotalRowsRead < m_JPEGDecompress.output_height ) {
		jpeg_abort_decompress(&m_JPEGDecompress);
		jpeg_destroy_decompress(&m_JPEGDecompress);
	} else {
		jpeg_finish_decompress(&m_JPEGDecompress);
		jpeg_destroy_decompress(&m_JPEGDecompress);
	}

	return true;
}

bool ImageReaderJPEG::readImage(Image* destImage)
{
	unsigned int destWidth = destImage->getWidth();
	unsigned int destHeight = destImage->getHeight();

	if( !beginReadInternal(destWidth, destHeight, destImage->getColorModel()) ) {
		return false;
	}

	if( destWidth != m_JPEGDecompress.output_width || destHeight != m_JPEGDecompress.output_height ) {
		fprintf(stderr, "error: unable to scale jpeg to desired dimensions\n");
		return false;
	}

	if( readRows(destImage, 0, destHeight) != destHeight ) {
		return false;
	}

	endRead();

	return true;
}

void ImageReaderJPEG::computeReadDimensions(unsigned int desiredWidth, unsigned int desiredHeight, unsigned int& readWidth, unsigned int& readHeight)
{
	readWidth = m_Width;
	readHeight = m_Height;
	unsigned int reduceCount = 0;
	while( div2_round(readWidth) >= desiredWidth && div2_round(readHeight) >= desiredHeight && reduceCount < 3 ) {
		readWidth = div2_round(readWidth);
		readHeight = div2_round(readHeight);
		reduceCount++;
	}
}

EImageFormat ImageReaderJPEG::getFormat()
{
	return kImageFormat_JPEG;
}

const char* ImageReaderJPEG::getFormatName()
{
	return "JPEG";
}

unsigned int ImageReaderJPEG::getWidth()
{
	return m_Width;
}

unsigned int ImageReaderJPEG::getHeight()
{
	return m_Height;
}

EImageOrientation ImageReaderJPEG::getOrientation()
{
	return m_Orientation;
}

void ImageReaderJPEG::storeGeoTagData(ExifWriter& exifWriter) const
{
	exifWriter.putValue(m_GPSLatitudeRef, ExifCommon::kTagId::kGPSLatitudeRef);
	exifWriter.putValue(m_GPSLatitude, ExifCommon::kTagId::kGPSLatitude);
	exifWriter.putValue(m_GPSLongitudeRef, ExifCommon::kTagId::kGPSLongitudeRef);
	exifWriter.putValue(m_GPSLongitude, ExifCommon::kTagId::kGPSLongitude);
	exifWriter.putValue((uint8_t)m_AltitudeRef, ExifCommon::kTagId::kGPSAltitudeRef);
	exifWriter.putValue(m_GPSAltitude, ExifCommon::kTagId::kGPSAltitude);
	exifWriter.putValue(m_GPSTimeStamp, ExifCommon::kTagId::kGPSTimeStamp);
	exifWriter.putValue(m_GPSSpeedRef, ExifCommon::kTagId::kGPSSpeedRef);
	exifWriter.putValue(m_GPSSpeed, ExifCommon::kTagId::kGPSSpeed);
	exifWriter.putValue(m_GPSImgDirectionRef, ExifCommon::kTagId::kGPSImgDirectionRef);
	exifWriter.putValue(m_GPSImgDirection, ExifCommon::kTagId::kGPSImgDirection);
	exifWriter.putValue(m_GPSDestBearingRef, ExifCommon::kTagId::kGPSDestBearingRef);
	exifWriter.putValue(m_GPSDestBearing, ExifCommon::kTagId::kGPSDestBearing);
}

bool ImageReaderJPEG::hasValidGeoTagData() const
{
	return (m_GPSLatitude.m_value[0].getInt() != 0) || (m_GPSLatitude.m_value[1].getInt() != 0) || (m_GPSLatitude.m_value[2].getInt() != 0);
}

void ImageReaderJPEG::setReadOptions(unsigned int readOptions)
{
	m_ReadOptions = readOptions;
}

uint8_t* ImageReaderJPEG::getEXIFData(unsigned int& size)
{
	size = m_EXIFDataSize;
	return m_EXIFData;
}

EImageColorModel ImageReaderJPEG::getNativeColorModel()
{
	return m_NativeColorModel;
}

bool ImageReaderJPEG::supportsOutputColorModel(EImageColorModel colorSpace)
{
	return Image::colorModelIsRGBA(colorSpace) || colorSpace == m_NativeColorModel;
}

uint8_t* ImageReaderJPEG::getColorProfile(unsigned int& size)
{
	size = m_RawColorProfileSize;
	return m_RawColorProfile;
}

ImageReaderJPEG::SourceManager::SourceManager(ImageReader::Storage* storage, ImageReaderJPEG* reader)
:	storage(storage)
,	reader(reader)
,	startOfFile(false)
{
	// jpeg_source_mgr
	this->init_source = initSource;
	this->fill_input_buffer = fillInputBuffer;
	this->skip_input_data = skipInputData;
	this->resync_to_restart = jpeg_resync_to_restart;
	this->term_source = term_source;
	this->bytes_in_buffer = 0;
	this->next_input_byte = NULL;
}

void ImageReaderJPEG::SourceManager::initSource(j_decompress_ptr cinfo)
{
	ImageReaderJPEG::SourceManager* self = (ImageReaderJPEG::SourceManager*)cinfo->src;
	self->startOfFile = true;
}

boolean ImageReaderJPEG::SourceManager::fillInputBuffer(j_decompress_ptr cinfo)
{
	ImageReaderJPEG::SourceManager* self = (ImageReaderJPEG::SourceManager*)cinfo->src;

	size_t nbytes = (size_t)self->storage->read(self->buffer, kReadBufferSize);

	if (nbytes <= 0) {
		if (self->startOfFile) {
			ERREXIT(cinfo, JERR_INPUT_EMPTY);
		}
		WARNMS(cinfo, JWRN_JPEG_EOF);
		// Insert a fake EOI marker
		self->buffer[0] = (JOCTET)0xFF;
		self->buffer[1] = (JOCTET)JPEG_EOI;
		nbytes = 2;
	}

	self->next_input_byte = self->buffer;
	self->bytes_in_buffer = nbytes;
	self->startOfFile = false;

	return true;
}

void ImageReaderJPEG::SourceManager::skipInputData(j_decompress_ptr cinfo, long numBytes)
{
	ImageReaderJPEG::SourceManager* self = (ImageReaderJPEG::SourceManager*)cinfo->src;
	if (numBytes > 0) {
		while( numBytes > (long)self->bytes_in_buffer ) {
			numBytes -= (long)self->bytes_in_buffer;
			fillInputBuffer(cinfo);
		}
		self->next_input_byte += (size_t)numBytes;
		self->bytes_in_buffer -= (size_t)numBytes;
	}
}

void ImageReaderJPEG::SourceManager::termSource(j_decompress_ptr cinfo)
{

}

//////

bool ImageWriterJPEG::Factory::matchesExtension(const char *extension)
{
	return strcasecmp(extension, "jpg") == 0 || strcasecmp(extension, "jpeg") == 0;
}

bool ImageWriterJPEG::Factory::appropriateForInputFormat(EImageFormat inputFormat)
{
	return inputFormat == kImageFormat_JPEG;
}

bool ImageWriterJPEG::Factory::supportsInputColorModel(EImageColorModel colorModel)
{
#if JCS_EXTENSIONS
	return Image::colorModelIsRGBA(colorModel) || Image::colorModelIsYUV(colorModel);
#else
	return Image::colorModelIsRGBA(colorModel);
#endif
}

EImageFormat ImageWriterJPEG::Factory::getFormat()
{
	return kImageFormat_JPEG;
}

ImageWriterJPEG::ImageWriterJPEG()
:	m_WriteError(false)
,	m_WriteOptions(kWriteOption_CopyColorProfile)
,	m_Quality(75)
,	m_SourceReader(NULL)
,	m_DestinationManager(NULL)
,	m_QuantTables(NULL)
{
}

ImageWriterJPEG::~ImageWriterJPEG()
{
	delete m_DestinationManager;
	m_DestinationManager = NULL;
	delete m_QuantTables;
	m_QuantTables = NULL;
}

bool ImageWriterJPEG::initWithStorage(Storage* output)
{
	// Set up custom error handler.
	m_JPEGCompress.client_data = this;
	m_JPEGCompress.err = jpeg_std_error(&m_JPEGError.pub);
	m_JPEGError.pub.error_exit = jpegError;
	m_JPEGError.pub.output_message = jpegMessage;

	if( setjmp(m_JPEGError.jmp) ) {
		fprintf(stderr, "error during jpeg compress init: %s", s_JPEGLastError);
		jpeg_destroy_compress(&m_JPEGCompress);
		return false;
	}

	jpeg_create_compress(&m_JPEGCompress);

	m_DestinationManager = new DestinationManager(output, this);
	if( m_DestinationManager == NULL) {
		fprintf(stderr, "error: could not create destination manager\n");
		jpeg_destroy_compress(&m_JPEGCompress);
		return false;
	}
	m_JPEGCompress.dest = m_DestinationManager;

	return true;
}

void ImageWriterJPEG::setSourceReader(ImageReader* sourceReader)
{
	if( sourceReader != NULL && sourceReader->getFormat() == kImageFormat_JPEG ) {
		m_SourceReader = (ImageReaderJPEG*)sourceReader;
	}
}

void ImageWriterJPEG::setWriteOptions(unsigned int options)
{
	m_WriteOptions = options;
}

void ImageWriterJPEG::setQuality(unsigned int quality)
{
	m_Quality = quality;
}

void ImageWriterJPEG::setCopyMetaData(bool copyMetaData)
{
	m_CopyMetaData = copyMetaData;
}

bool ImageWriterJPEG::beginWrite(unsigned int width, unsigned int height, EImageColorModel colorModel)
{
	if( setjmp(m_JPEGError.jmp) ) {
		fprintf(stderr, "error during jpeg compress init: %s", s_JPEGLastError);
		jpeg_destroy_compress(&m_JPEGCompress);
		return false;
	}

	m_JPEGCompress.image_width = width;
	m_JPEGCompress.image_height = height;
	if( Image::colorModelIsRGBA(colorModel) ) {
		m_JPEGCompress.input_components = HAVE_RGBX ? 4 : 3;
		m_JPEGCompress.in_color_space = HAVE_RGBX ? JCS_EXT_RGBX : JCS_RGB;
	} else if( Image::colorModelIsYUV(colorModel) ) {
		m_JPEGCompress.input_components = 3;
		m_JPEGCompress.in_color_space = JCS_YCbCr;
	} else {
		return false;
	}

	jpeg_set_defaults(&m_JPEGCompress);
	jpeg_set_quality(&m_JPEGCompress, m_Quality, TRUE);
	jpeg_set_colorspace(&m_JPEGCompress, JCS_YCbCr);

	if( m_QuantTables != NULL ) {
		jpeg_add_quant_table(&m_JPEGCompress, 0, m_QuantTables, 100, true);
		jpeg_add_quant_table(&m_JPEGCompress, 1, m_QuantTables + DCTSIZE2, 100, true);
	}

	if( (m_WriteOptions & kWriteOption_QualityFast) == 0 ) {
		// Compressing takes about 50% longer with this on, but produces files a few percent smaller.
		m_JPEGCompress.optimize_coding = true;
	}

	// This makes more of a difference than the documentation suggests, and it's not really slower (faster in some tests).
	m_JPEGCompress.dct_method = JDCT_ISLOW;

	if( (m_WriteOptions & kWriteOption_Progressive) != 0 ) {
		jpeg_simple_progression(&m_JPEGCompress);
	}

	m_JPEGCompress.comp_info[1].h_samp_factor = 1;
	m_JPEGCompress.comp_info[1].h_samp_factor = 1;
	m_JPEGCompress.comp_info[2].v_samp_factor = 1;
	m_JPEGCompress.comp_info[2].v_samp_factor = 1;

	if( Image::colorModelIsRGBA(colorModel) ) {
		if( m_Quality == 100 ) {
			m_JPEGCompress.comp_info[0].h_samp_factor = 1;
			m_JPEGCompress.comp_info[0].v_samp_factor = 1;
		} else if( m_Quality > 95 ) {
			m_JPEGCompress.comp_info[0].h_samp_factor = 2;
			m_JPEGCompress.comp_info[0].v_samp_factor = 1;
		} else {
			m_JPEGCompress.comp_info[0].h_samp_factor = 2;
			m_JPEGCompress.comp_info[0].v_samp_factor = 2;
		}
	} else if( colorModel == kColorModel_YUV_420 ) {
		m_JPEGCompress.raw_data_in = 1;
		m_JPEGCompress.comp_info[0].h_samp_factor = 2;
		m_JPEGCompress.comp_info[0].v_samp_factor = 2;
	} else {
		SECURE_ASSERT(0);
	}

	jpeg_start_compress(&m_JPEGCompress, TRUE);

	if( !writeMarkers() ) {
		jpeg_destroy_compress(&m_JPEGCompress);
		return false;
	}

	return true;
}

bool ImageWriterJPEG::writeMarkers()
{
	bool didWriteColorProfile = false;
	if( (m_WriteOptions & kWriteOption_CopyColorProfile) != 0 && m_SourceReader != NULL ) {
		unsigned int colorProfileSize = 0;
		uint8_t* colorProfile = m_SourceReader->getColorProfile(colorProfileSize);
		if( colorProfile != NULL && colorProfileSize > 0 ) {
			write_icc_profile(&m_JPEGCompress, colorProfile, colorProfileSize);
			didWriteColorProfile = true;
		}
	}
	if( (m_WriteOptions & kWriteOption_WriteDefaultColorProfile) != 0 && !didWriteColorProfile) {
#if IMAGECORE_WITH_LCMS
		cmsHPROFILE sRGBProfile = cmsCreate_sRGBProfile();
		if( !sRGBProfile ) {
			return false;
		}
		// First call with a null output buffer to determine output size
		unsigned int outputSize = 0;
		if( cmsSaveProfileToMem(sRGBProfile, NULL, &outputSize) ) {
			if( outputSize > 0 ) {
				// Next allocate the bytes for the profile and write into it
				cmsUInt8Number* buffer = (cmsUInt8Number*)malloc(outputSize + 1);
				if( buffer != NULL ) {
					if( cmsSaveProfileToMem(sRGBProfile, buffer, &outputSize) ) {
						write_icc_profile(&m_JPEGCompress, buffer, outputSize);
						didWriteColorProfile = true;
					}
					free(buffer);
				}
			}
		}
		cmsCloseProfile(sRGBProfile);
#endif
	}
	if( (m_WriteOptions & kWriteOption_CopyMetaData) != 0 && m_SourceReader != NULL ) {
		unsigned int exifDataSize = 0;
		uint8_t* exifData = m_SourceReader->getEXIFData(exifDataSize);
		if( exifData != NULL && exifDataSize > 0 ) {
			jpeg_write_marker(&m_JPEGCompress, EXIF_MARKER, exifData, exifDataSize);
		}
	} else {
		ExifWriter exifWriter(true);
		if ((m_WriteOptions & kWriteOption_WriteExifOrientation) != 0 && m_SourceReader != NULL && m_SourceReader->getOrientation() != kImageOrientation_Up) {
			exifWriter.putValue((uint16_t)m_SourceReader->getOrientation(), ExifCommon::kTagId::kOrientation);
		}
		if ((m_WriteOptions & kWriteOption_GeoTagData) != 0 && m_SourceReader != NULL && m_SourceReader->hasValidGeoTagData()) {
			exifWriter.putValue((uint16_t)m_SourceReader->getOrientation(), ExifCommon::kTagId::kOrientation);
			m_SourceReader->storeGeoTagData(exifWriter);
		}
		if(!exifWriter.isEmpty()) { // if anything got written to EXIF section, flush it to the jpeg writer
			uint8_t* exifData = new uint8_t[64 * 1024];
			MemoryStreamWriter exifStream(exifData, 64 * 1024, true);
			exifWriter.WriteToStream(exifStream);
			jpeg_write_marker(&m_JPEGCompress, EXIF_MARKER, exifStream.getData(), exifStream.getSize());
			delete [] exifData;
		}
	}
	return true;
}

unsigned int ImageWriterJPEG::writeRows(Image* source, unsigned int sourceRow, unsigned int numRows)
{
	if( Image::colorModelIsRGBA(source->getColorModel()) ) {

		ImageRGBA* sourceImage = source->asRGBA();

		uint8_t** rows = (uint8_t**)malloc(numRows * sizeof(uint8_t*));
		if( rows == NULL ) {
			return 0;
		}

		if( setjmp(m_JPEGError.jmp) ) {
			fprintf(stderr, "error during jpeg compress: %s", s_JPEGLastError);
			jpeg_destroy_compress(&m_JPEGCompress);
			free(rows);
			return 0;
		}

		unsigned int sourceHeight = sourceImage->getHeight();
		unsigned int sourcePitch = sourceImage->getPitch();
		uint8_t* sourceBuffer = sourceImage->lockRect(0, sourceRow, sourceImage->getWidth(), numRows, sourcePitch);
		SECURE_ASSERT(sourceRow + numRows <= sourceHeight);

#if !HAVE_RGBX
		unsigned int sourceWidth = sourceImage->getWidth();
		// Provided purely for compatibility for vanilla libjpeg, not recommended.
		uint8_t* jpegBuffer = (uint8_t*)malloc(sourceWidth * numRows * 3);
		if( jpegBuffer == NULL ) {
			free(rows);
			jpeg_destroy_compress(&m_JPEGCompress);
			return 0;
		}
		unsigned int jpegPitch = sourceWidth * 3;
		for (unsigned int y = 0; y < numRows; y++) {
			for (unsigned int x = 0; x < sourceWidth; x++) {
				uint8_t* out = jpegBuffer + y * jpegPitch + x * 3;
				uint8_t* in = sourceBuffer + y * sourcePitch + x * 4;
				out[0] = in[0];
				out[1] = in[1];
				out[2] = in[2];
			}
		}
#else
		uint8_t* jpegBuffer = sourceBuffer;
		unsigned int jpegPitch = sourcePitch;
#endif

		for( unsigned int i = 0; i < numRows; i++ ) {
			rows[i] = jpegBuffer + i * jpegPitch;
		}

		unsigned int currentRow = 0;
		while( currentRow < numRows ) {
			unsigned int numRowsWritten = jpeg_write_scanlines(&m_JPEGCompress, rows + currentRow, numRows - currentRow);
			if( numRowsWritten == 0 ) {
				break;
			}
			currentRow += numRowsWritten;
		}

		free(rows);

#if !HAVE_RGBX
		free(jpegBuffer);
#endif
		return numRows;
	} else if( Image::colorModelIsYUV(source->getColorModel()) ) {
		// TODO: Incremental writing
		ASSERT(sourceRow == 0);
		ASSERT(numRows == m_JPEGCompress.image_height);
		if( setjmp(m_JPEGError.jmp) ) {
			fprintf(stderr, "error during jpeg compress: %s", s_JPEGLastError);
			jpeg_destroy_compress(&m_JPEGCompress);
			return 0;
		}

		ImageYUV* yuvImage = source->asYUV();

		unsigned int minPitchY = align(DCTSIZE * 2, yuvImage->getPlaneY()->getWidth());
		unsigned int minPitchUV = align(DCTSIZE, yuvImage->getPlaneU()->getWidth());

		ImageYUV* sourceImage = yuvImage;
		ImageYUV* tempImage = NULL;
		// Image doesn't meet padding or range requirements, copy to a new buffer and correct.
		if( yuvImage->getRange() == kYUVRange_Compressed || (sourceImage->getPadding() < 16 && (m_WriteOptions & kWriteOption_AssumeMCUPaddingFilled) == 0) || yuvImage->getPlaneY()->getPitch() < minPitchY || yuvImage->getPlaneU()->getPitch() < minPitchUV ) {
			tempImage = ImageYUV::create(yuvImage->getWidth(), yuvImage->getHeight(), 16, 16);
			if( tempImage == NULL ) {
				return 0;
			}
			if( yuvImage->getRange() == kYUVRange_Compressed ) {
				yuvImage->expandRange(tempImage);
			} else {
				yuvImage->copy(tempImage);
			}
			sourceImage = tempImage;
			// Fill padding region of new image.
			sourceImage->getPlaneY()->fillPadding(kEdge_Right);
			sourceImage->getPlaneU()->fillPadding(kEdge_Right);
			sourceImage->getPlaneV()->fillPadding(kEdge_Right);
		} else if( (m_WriteOptions & kWriteOption_AssumeMCUPaddingFilled) == 0 ) {
			// Image was large enough and in the correct range, just fill the padding (unless it's been requested that we don't).
			// Use the existing padding region to satisfy jpeg_write_raw_data's requirement that we write
			// data aligned to the block size. Fill the edge padding data with data from the image (clamped)
			// to prevent any problems with the DCT.
			sourceImage->getPlaneY()->fillPadding(kEdge_Right);
			sourceImage->getPlaneU()->fillPadding(kEdge_Right);
			sourceImage->getPlaneV()->fillPadding(kEdge_Right);
		}

		SECURE_ASSERT(sourceImage->getPlaneY()->getPitch() >= minPitchY);
		SECURE_ASSERT(sourceImage->getPlaneU()->getPitch() >= minPitchUV);
		SECURE_ASSERT(sourceImage->getPlaneV()->getPitch() >= minPitchUV);

		ImagePlane8* planeY = sourceImage->getPlaneY();
		ImagePlane8* planeU = sourceImage->getPlaneU();
		ImagePlane8* planeV = sourceImage->getPlaneV();

		uint8_t* bufferY = (uint8_t*)planeY->getBytes();
		uint8_t* bufferU = (uint8_t*)planeU->getBytes();
		uint8_t* bufferV = (uint8_t*)planeV->getBytes();

		unsigned int pitchY = planeY->getPitch();
		unsigned int pitchU = planeU->getPitch();
		unsigned int pitchV = planeV->getPitch();

		unsigned int currentRowY = 0;
		unsigned int currentRowUV = 0;
		constexpr unsigned int rowStepY = DCTSIZE * 2;
		constexpr unsigned int rowStepUV = DCTSIZE;
		while( currentRowY < numRows ) {
			JSAMPROW rowsY[rowStepY];
			JSAMPROW rowsU[rowStepY];
			JSAMPROW rowsV[rowStepY];
			for( unsigned int y = 0; y < rowStepY; y++ ) {
				rowsY[y] = bufferY + pitchY * clamp(0, planeY->getHeight() - 1, y + currentRowY);
			}
			for( unsigned int y = 0; y < rowStepUV; y++ ) {
				rowsU[y] = bufferU + pitchU * clamp(0, planeU->getHeight() - 1, y + currentRowUV);
				rowsV[y] = bufferV + pitchV * clamp(0, planeV->getHeight() - 1, y + currentRowUV);
			}
			JSAMPARRAY rows[] = { rowsY, rowsU, rowsV };
			unsigned int numRowsWritten = jpeg_write_raw_data(&m_JPEGCompress, rows, rowStepY);
			currentRowY += numRowsWritten;
			currentRowUV += numRowsWritten / 2;
		}

		delete tempImage;

		return numRows;
	} else {
		return 0;
	}
}

bool ImageWriterJPEG::endWrite()
{
	if( setjmp(m_JPEGError.jmp) ) {
		return false;
	}

	jpeg_finish_compress(&m_JPEGCompress);
	jpeg_destroy_compress(&m_JPEGCompress);

	if( m_WriteError ) {
		return false;
	}

	return true;
}

void ImageWriterJPEG::setWriteError()
{
	m_WriteError = true;
}

bool ImageWriterJPEG::writeImage(Image* sourceImage)
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

bool ImageWriterJPEG::copyLossless(ImageReader* reader)
{
#if IMAGECORE_WITH_JPEG_TRANSFORMS
	if( setjmp(m_JPEGError.jmp) ) {
		fprintf(stderr, "error during jpeg lossless copy: %s", s_JPEGLastError);
		jpeg_destroy_compress(&m_JPEGCompress);
		return false;
	}

	if( reader->getFormat() != kImageFormat_JPEG ) {
		jpeg_destroy_compress(&m_JPEGCompress);
		return false;
	}

	ImageReaderJPEG* jpegReader = (ImageReaderJPEG*)reader;
	setSourceReader(reader);

	if( setjmp(jpegReader->m_JPEGError.jmp) ) {
		fprintf(stderr, "error during jpeg lossless copy: %s", s_JPEGLastError);
		jpeg_destroy_compress(&m_JPEGCompress);
		return false;
	}

	jpeg_transform_info transform;
	memset(&transform, 0, sizeof(jpeg_transform_info));
	if( (m_WriteOptions & kWriteOption_LosslessPerfect) != 0 ) {
		transform.perfect = true;
	} else{
		// Trim edge blocks, otherwise the garbage padding data will be visible.
		transform.trim = true;
	}

	bool haveTransform = false;

	EImageOrientation orientation = reader->getOrientation();

	// If we're preserving the EXIF orientation tag, we can skip this.
	bool skipRotate = (m_WriteOptions & kWriteOption_WriteExifOrientation) != 0 || (m_WriteOptions & kWriteOption_CopyMetaData) != 0;
	if( !skipRotate && (orientation == kImageOrientation_Down || orientation == kImageOrientation_Left || orientation == kImageOrientation_Right) ) {
		switch( orientation ) {
			case kImageOrientation_Down:
				transform.transform = JXFORM_ROT_180;
				break;
			case kImageOrientation_Left:
				transform.transform = JXFORM_ROT_90;
				break;
			case kImageOrientation_Right:
				transform.transform = JXFORM_ROT_270;
				break;
			case kImageOrientation_Up:
				break;
		}
		haveTransform = true;
	}

	if( !jtransform_request_workspace(&jpegReader->m_JPEGDecompress, &transform) ) {
		// This will fail if perfect is specified, but the rotation results in lost edge blocks.
		jpeg_destroy_compress(&m_JPEGCompress);
		return false;
	}

	jpeg_copy_critical_parameters(&jpegReader->m_JPEGDecompress, &m_JPEGCompress);

	if( (m_WriteOptions & kWriteOption_Progressive) != 0 ) {
		jpeg_simple_progression(&m_JPEGCompress);
	}

	// Always re-optimize the huffman table for the new JPEG.
	m_JPEGCompress.optimize_coding = true;

	jvirt_barray_ptr* sourceCoeffs = jpeg_read_coefficients(&jpegReader->m_JPEGDecompress);
	jvirt_barray_ptr* destCoeffs = sourceCoeffs;

	if( haveTransform ) {
		destCoeffs = jtransform_adjust_parameters(&jpegReader->m_JPEGDecompress, &m_JPEGCompress, sourceCoeffs, &transform);
	}

	jpeg_write_coefficients(&m_JPEGCompress, destCoeffs);

	if( haveTransform ) {
		jtransform_execute_transformation(&jpegReader->m_JPEGDecompress, &m_JPEGCompress, sourceCoeffs, &transform);
	}

	if( !writeMarkers() ) {
		jpeg_destroy_compress(&m_JPEGCompress);
		return false;
	}

	if( !endWrite() ) {
		jpeg_destroy_compress(&m_JPEGCompress);
		return false;
	}
	return true;
#else
	return false;
#endif
}

void ImageWriterJPEG::setQuantizationTables(uint32_t* table)
{
	if( m_QuantTables == NULL ) {
		m_QuantTables = (uint32_t*)malloc(sizeof(uint32_t) * DCTSIZE2 * 2);
	}
	memcpy(m_QuantTables, table, sizeof(uint32_t) * DCTSIZE2 * 2);
}

ImageWriterJPEG::DestinationManager::DestinationManager(ImageWriter::Storage* storage, ImageWriterJPEG* writer)
:	storage(storage)
,	writer(writer)
{
	this->init_destination = initDestination;
	this->empty_output_buffer = emptyOutputBuffer;
	this->term_destination = termDestination;
}

void ImageWriterJPEG::DestinationManager::initDestination(j_compress_ptr cinfo)
{
	ImageWriterJPEG::DestinationManager* dest = (ImageWriterJPEG::DestinationManager*)cinfo->dest;
	dest->next_output_byte = dest->m_Buffer;
	dest->free_in_buffer = kWriteBufferSize;
}

boolean ImageWriterJPEG::DestinationManager::emptyOutputBuffer(j_compress_ptr cinfo)
{
	ImageWriterJPEG::DestinationManager* dest = (ImageWriterJPEG::DestinationManager*)cinfo->dest;
	uint64_t written = dest->storage->write(dest->m_Buffer, kWriteBufferSize);
	if( written < kWriteBufferSize ) {
		return false;
	}
	dest->next_output_byte = dest->m_Buffer;
	dest->free_in_buffer = kWriteBufferSize;
	return true;
}

void ImageWriterJPEG::DestinationManager::termDestination(j_compress_ptr cinfo)
{
	ImageWriterJPEG::DestinationManager* dest = (ImageWriterJPEG::DestinationManager*)cinfo->dest;
	unsigned int size = (unsigned int)(kWriteBufferSize - dest->free_in_buffer);
	if( size > 0 && (dest->storage->write(dest->m_Buffer, size) < size) ) {
		dest->writer->setWriteError();
	}
	dest->storage->flush();
}

}
