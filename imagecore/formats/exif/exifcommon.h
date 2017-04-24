//
//  exifcommon.h
//  ImageCore
//
//  Created by Paul Melamed on 11/26/14.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include <inttypes.h>
#include "imagecore/image/image.h"

#include "imagecore/utils/memorystream.h"
#include "imagecore/imagecore.h"
#include "imagecore/utils/mathtypes.h"

#define TFIF_MARKER 0x2A
#define EXIF_BIG_ENDIAN 0x4D4D
#define EXIF_MARKER 0xE1

namespace imagecore {

class ExifCommon
{
public:
	enum class kTagId : uint16_t
	{
		kMake = 0,
		kModel,
		kOrientation,
		kXResolution,
		kYResolution,
		kResolutionUnit,
		kSoftware,
		kModifyDate,
		kExposureTime,
		kExifSubDirectory,
		kGPSSubDirectory,
		kGPSLatitudeRef,
		kGPSLatitude,
		kGPSLongitudeRef,
		kGPSLongitude,
		kGPSAltitudeRef,
		kGPSAltitude,
		kGPSTimeStamp,
		kGPSSpeedRef,
		kGPSSpeed,
		kGPSImgDirectionRef,
		kGPSImgDirection,
		kGPSDestBearingRef,
		kGPSDestBearing,
		kGPSDateStamp,
		kTagIdMax
	};

	enum class kTagType : unsigned short
	{
		kUnsignedByte = 1,      // (1 byte per component)
		kASCIIString = 2,       // (1 bpc)
		kUnsignedShort = 3,     // (2 bpc)
		kUnsignedLong = 4,      // (4 bpc)
		kUnsignedRational = 5,  // (8 bpc, 4 for the numerator and 4 for the denominator)
		kSignedByte = 6,
		kUndefined = 7,         // (1 bpc)
		kSignedShort = 8,
		kSignedLong = 9,
		kSignedRational = 10,
		kSingle = 11,           // (4 bpc)
		kDouble = 12            // (8 bpc)
	};

	// This is needed, because the same exif tag can be used to id different data depending on which subdirectory its in.
	enum class kDirectoryType
	{
		kDirectoryExif = 0,
		kDirectoryGPS,
		kDirectoryMaxDirs
	};

	typedef bool (*validateRange)(const void*, uint32_t);

	struct TagHeaderType
	{
		uint16_t        m_exifId;           // Exif ID
		kDirectoryType  m_dirType;
		kTagType        m_type;
		uint32_t    m_count;
		uint16_t        m_internalId;       // our ID
		validateRange   m_rangeValidator;
		bool verifyTypeAndCount(uint16_t exifType, uint32_t count) const;
		bool relaxedTypeAndCountCheck(uint16_t exifType, uint32_t count, uint16_t signedType, uint16_t unsignedType) const;
	};

	static const uint32_t kMaxExifStringLength = 256;

	struct ExifString
	{
		ExifString()
		{
			m_string[0] = 0;
			m_length = 0;
		}

		ExifString(const char* str)
		{
			m_length = strlen(str) + 1;
			SECURE_ASSERT(m_length < kMaxExifStringLength)
			strcpy(m_string, str);
			m_string[m_length - 1] = 0; // zero terminate
		}

		char m_string[kMaxExifStringLength];
		uint16_t m_length;
	};

	struct ExifU64Rational3 // Array of 3 rationals used in GPS data
	{
		ExifU64Rational3()
		{
		}

		ExifU64Rational3(Rational<uint32_t> v0, Rational<uint32_t> v1, Rational<uint32_t> v2)
		{
			m_value[0] = v0;
			m_value[1] = v1;
			m_value[2] = v2;
		}

		bool operator == (const ExifU64Rational3 &other) const {
			return (m_value[0] == other.m_value[0]) && (m_value[1] == other.m_value[1]) && (m_value[2] == other.m_value[2]);
		}

		void setSign(bool isSigned) {
			m_value[0].m_Signed = isSigned;
			m_value[1].m_Signed = isSigned;
			m_value[2].m_Signed = isSigned;
		}

		static uint32_t sizeOf() {
			return 3 * Rational<uint32_t>::sizeOf();
		}

		Rational<uint32_t> m_value[3];
	};

	static const uint32_t kMaxUniqueTags = 64 * 1024;

	enum class kExifTagID : uint16_t
	{
		kExifMake = 0x010f,
		kExifModel = 0x0110,
		kExifOrientation = 0x0112,
		kExifXResolution = 0x011A,
		kExifYResolution = 0x011B,
		kExifResolutionUnit = 0x0128,
		kExifSoftware = 0x0131,
		kExifModifyDate = 0x0132,
		kExifYCbCrPositioning = 0x0213,
		kExifOffset = 0x8769,
		kExifGPSInfo = 0x8825,
		kExifExposureTime = 0x829a,
		kExifFNumber = 0x829d,
		kExifExposureProgram = 0x8822,
		kExifISO = 0x8827,
		kExifVersion = 0x9000,
		kExifDateTimeOriginal = 0x9003,
		kExifCreateDate = 0x9004,
		kExifComponentsConfiguration = 0x9101,
		kExifShutterSpeedValue = 0x9201,
		kExifApertureValue = 0x9202,
		kExifBrightnessValue = 0x9203,
		kExifExposureCompensation = 0x9204,
		kExifMeteringMode = 0x9207,
		kExifFlash = 0x9209,
		kExifFocalLength = 0x920a,
		kExifSubjectArea = 0x9214,
		kExifManufacturerTags = 0x927c,
		kExifSubSecTimeOriginal = 0x9291,
		kExifSubSecTimeDigitized = 0x9292,
		kExifFlashpixVersion = 0xa000,
		kExifColorSpace = 0xa001,
		kExifImageWidth = 0xa002,
		kExifImageHeight = 0xa003,
		kExifSensingMethod = 0xa217,
		kExifSceneType = 0xa301,
		kExifExposureMode = 0xa402,
		kExifWhiteBalance = 0xa403,
		kExifFocalLengthIn35mmFormat = 0xa405,
		kExifSceneCaptureType = 0xa406,
		kExifLensInfo = 0xa432,
		kExifLensMake = 0xa433,
		kExifLensModel = 0xa434,
	};

	enum class kExifGPSTagID : uint16_t
	{
		kExifGPSLatitudeRef = 0x1,
		kExifGPSLatitude = 0x2,
		kExifGPSLongitudeRef = 0x3,
		kExifGPSLongitude = 0x4,
		kExifGPSAltitudeRef = 0x5,
		kExifGPSAltitude = 0x6,
		kExifGPSTimeStamp = 0x7,
		kExifGPSSpeedRef = 0xc,
		kExifGPSSpeed = 0xd,
		kExifGPSImgDirectionRef = 0x10,
		kExifGPSImgDirection = 0x11,
		kExifGPSDestBearingRef = 0x17,
		kExifGPSDestBearing = 0x18,
		kExifGPSDateStamp = 0x1d,
	};

	class IFDStructure
	{
		public:
			void write(MemoryStreamWriter& memoryStream, uint32_t offset);
			uint16_t tag;
			uint16_t type;
			uint32_t count;
			uint32_t value_offset;
			bool isOffset;
	};

	static const uint32_t kMaxExifDirectoryEntries = 256;
	static const uint32_t kMaxMetaDataSize = 64 * 1024;
	class ExifDirectory
	{
		public:
			ExifDirectory(bool is_be);
			~ExifDirectory();
			IFDStructure* getNewEntry();
			void write(MemoryStreamWriter& memoryStream, uint32_t& offset) const;
			uint16_t m_numEntries;
			IFDStructure* m_entries;
			uint8_t* metaDataMemory;
			MemoryStreamWriter* metaDataWriter;
	};

	ExifCommon();

    static ExifCommon& Instance();
	const TagHeaderType* getTagHeader(kTagId tagId);
	kDirectoryType getDirectoryType(kExifTagID exifDirectoryTag);
	int32_t getTagId(uint32_t dirType, uint32_t exifDirectoryTag);

private:
	// validators
	static bool validateOrientation(const void* valueAddress, uint32_t count);
	static bool validateString(const void* valueAddress, uint32_t count);
	static bool validateU64Rational(const void* valueAddress, uint32_t count);
	static bool validateU64Rational3(const void* valueAddress, uint32_t count);
	static bool validateResolutionUnit(const void* valueAddress, uint32_t count);
	static bool validateOffset(const void* valueAddress, uint32_t count);
	static bool validateAltitude(const void* valueAddress, uint32_t count);

	// static headers defining parameters for all supported query types
	static TagHeaderType s_mTagHeaders[(uint32_t)kTagId::kTagIdMax];
	int32_t m_TagHeaderLookup[(uint32_t)kDirectoryType::kDirectoryMaxDirs][kMaxUniqueTags];
};

}
