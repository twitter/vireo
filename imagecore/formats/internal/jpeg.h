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
#include "imagecore/formats/exif/exifreader.h"
#include "imagecore/formats/exif/exifwriter.h"

#define HAVE_BOOLEAN
typedef int boolean;

#include <setjmp.h>
#include "jpeglib.h"
#if IMAGECORE_WITH_LCMS
#include "lcms2.h"
#endif

namespace imagecore {

struct JPEGErrorMgr
{
	jpeg_error_mgr pub;
	jmp_buf jmp;
};

class ImageReaderJPEG : public ImageReader
{
public:
	DECLARE_IMAGE_READER(ImageReaderJPEG);

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
	virtual EImageOrientation getOrientation();
	virtual EImageColorModel getNativeColorModel();
	virtual bool supportsOutputColorModel(EImageColorModel colorSpace);
	virtual void setReadOptions(unsigned int readOptions);
	virtual void computeReadDimensions(unsigned int desiredWidth, unsigned int desiredHeight, unsigned int& readWidth, unsigned int& readHeight);
	virtual uint8_t* getColorProfile(unsigned int& size);
	ExifReader* getExifReader() { return &m_ExifReader; }

private:
	friend class ImageWriterJPEG;

	// JPEG-specific metadata.
	uint8_t* getEXIFData(unsigned int& size);

    void storeGeoTagData(ExifWriter& exifWriter) const;
    bool hasValidGeoTagData() const;

	static boolean handleJPEGMarker(j_decompress_ptr dinfo);
	void processJPEGSegment(unsigned int marker, uint8_t* segmentData, unsigned int segmentLength);
	bool beginReadInternal(unsigned int destWidth, unsigned int destHeight, EImageColorModel outputColorModel);
	bool postProcessScanlines(uint8_t* buf, unsigned int size);

	struct SourceManager : jpeg_source_mgr
	{
	public:
		static const unsigned int kReadBufferSize = 4096;
		SourceManager(ImageReader::Storage* storage, ImageReaderJPEG* reader);
		ImageReader::Storage* storage;
		ImageReaderJPEG* reader;
		bool startOfFile;
		uint8_t buffer[kReadBufferSize];
	private:
		static void initSource(j_decompress_ptr cinfo);
		static boolean fillInputBuffer(j_decompress_ptr cinfo);
		static void skipInputData(j_decompress_ptr cinfo, long numBytes);
		static void termSource(j_decompress_ptr cinfo);
	};

	JPEGErrorMgr m_JPEGError;
	Storage* m_Source;
	SourceManager* m_SourceManager;
	unsigned int m_Width;
	unsigned int m_Height;
	// Exif data
	EImageOrientation m_Orientation;
	ExifCommon::ExifString m_GPSLatitudeRef;
	ExifCommon::ExifU64Rational3 m_GPSLatitude;
	ExifCommon::ExifString m_GPSLongitudeRef;
	ExifCommon::ExifU64Rational3 m_GPSLongitude;
	EAltitudeRef m_AltitudeRef;
	Rational<uint32_t> m_GPSAltitude;
	ExifCommon::ExifU64Rational3 m_GPSTimeStamp;
	ExifCommon::ExifString m_GPSSpeedRef;
	Rational<uint32_t> m_GPSSpeed;
	ExifCommon::ExifString m_GPSImgDirectionRef;
	Rational<uint32_t> m_GPSImgDirection;
	ExifCommon::ExifString m_GPSDestBearingRef;
	Rational<uint32_t> m_GPSDestBearing;

	unsigned int m_TotalRowsRead;
	uint8_t* m_EXIFData;
	unsigned int m_EXIFDataSize;
	uint8_t* m_RawColorProfile;
	unsigned int m_RawColorProfileSize;
	jpeg_decompress_struct m_JPEGDecompress;
	unsigned int m_ReadOptions;
	bool m_MarkerReadError;
	EImageColorModel m_NativeColorModel;
	ExifReader m_ExifReader;

#if IMAGECORE_WITH_LCMS
	cmsHPROFILE m_ColorProfile;
	cmsHPROFILE m_sRGBProfile;
	cmsHTRANSFORM m_ColorTransform;
	bool m_IgnoreColorProfile;
#endif
};

class ImageWriterJPEG : public ImageWriter
{
public:
	DECLARE_IMAGE_WRITER(ImageWriterJPEG);

	virtual void setWriteOptions(unsigned int options);
	virtual void setQuality(unsigned int quality);
	virtual void setSourceReader(ImageReader* sourceReader);
	virtual void setCopyMetaData(bool copyMetaData);
	virtual bool writeImage(Image* sourceImage);

	virtual bool beginWrite(unsigned int width, unsigned int height, EImageColorModel colorModel);
	virtual unsigned int writeRows(Image* sourceImage, unsigned int sourceRow, unsigned int numRows);
	virtual bool endWrite();

	virtual void setWriteError();

	virtual bool copyLossless(ImageReader* reader);

	virtual void setQuantizationTables(uint32_t* tables);

private:
	virtual bool initWithStorage(Storage* output);
	virtual bool writeMarkers();

	struct DestinationManager : jpeg_destination_mgr
	{
	public:
		static const unsigned int kWriteBufferSize = 1024;
		DestinationManager(ImageWriter::Storage* storage, ImageWriterJPEG* writer);
		ImageWriter::Storage* storage;
		ImageWriterJPEG* writer;
		uint8_t m_Buffer[kWriteBufferSize];
	private:
		static void initDestination(j_compress_ptr cinfo);
		static boolean emptyOutputBuffer(j_compress_ptr cinfo);
		static void termDestination(j_compress_ptr cinfo);
	};

	bool m_WriteError;
	unsigned int m_WriteOptions;
	jpeg_compress_struct m_JPEGCompress;
	JPEGErrorMgr m_JPEGError;
	bool m_CopyMetaData;
	uint32_t* m_QuantTables;
	unsigned int m_Quality;
	ImageReaderJPEG* m_SourceReader;
	DestinationManager* m_DestinationManager;
};

}
