//
//  exifcommon.cpp
//  ImageCore
//
//  Created by Paul Melamed on 11/26/14.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#include "exifcommon.h"

namespace imagecore {

// Various exif tags
ExifCommon::TagHeaderType  ExifCommon::s_mTagHeaders[(int) ExifCommon::kTagId::kTagIdMax] = // table for mapping our internal types to actual exif IDs
{
	{(uint16_t)ExifCommon::kExifTagID::kExifMake,                  ExifCommon::kDirectoryType::kDirectoryExif, ExifCommon::kTagType::kASCIIString,      1, (uint16_t)ExifCommon::kTagId::kMake,               &ExifCommon::validateString},
	{(uint16_t)ExifCommon::kExifTagID::kExifModel,                 ExifCommon::kDirectoryType::kDirectoryExif, ExifCommon::kTagType::kASCIIString,      1, (uint16_t)ExifCommon::kTagId::kModel,              &ExifCommon::validateString},
	{(uint16_t)ExifCommon::kExifTagID::kExifOrientation,           ExifCommon::kDirectoryType::kDirectoryExif, ExifCommon::kTagType::kUnsignedShort,    1, (uint16_t)ExifCommon::kTagId::kOrientation,        &ExifCommon::validateOrientation},
	{(uint16_t)ExifCommon::kExifTagID::kExifXResolution,           ExifCommon::kDirectoryType::kDirectoryExif, ExifCommon::kTagType::kUnsignedRational, 1, (uint16_t)ExifCommon::kTagId::kXResolution,        &ExifCommon::validateU64Rational},
	{(uint16_t)ExifCommon::kExifTagID::kExifYResolution,           ExifCommon::kDirectoryType::kDirectoryExif, ExifCommon::kTagType::kUnsignedRational, 1, (uint16_t)ExifCommon::kTagId::kYResolution,        &ExifCommon::validateU64Rational},
	{(uint16_t)ExifCommon::kExifTagID::kExifResolutionUnit,        ExifCommon::kDirectoryType::kDirectoryExif, ExifCommon::kTagType::kUnsignedShort,    1, (uint16_t)ExifCommon::kTagId::kResolutionUnit,     &ExifCommon::validateResolutionUnit},
	{(uint16_t)ExifCommon::kExifTagID::kExifSoftware,              ExifCommon::kDirectoryType::kDirectoryExif, ExifCommon::kTagType::kASCIIString,      1, (uint16_t)ExifCommon::kTagId::kSoftware,           &ExifCommon::validateString},
	{(uint16_t)ExifCommon::kExifTagID::kExifModifyDate,            ExifCommon::kDirectoryType::kDirectoryExif, ExifCommon::kTagType::kASCIIString,      1, (uint16_t)ExifCommon::kTagId::kModifyDate,         &ExifCommon::validateString},
	{(uint16_t)ExifCommon::kExifTagID::kExifExposureTime,          ExifCommon::kDirectoryType::kDirectoryExif, ExifCommon::kTagType::kUnsignedRational, 1, (uint16_t)ExifCommon::kTagId::kExposureTime,       &ExifCommon::validateU64Rational},
	{(uint16_t)ExifCommon::kExifTagID::kExifOffset,                ExifCommon::kDirectoryType::kDirectoryExif, ExifCommon::kTagType::kUnsignedLong,     1, (uint16_t)ExifCommon::kTagId::kExifSubDirectory,   &ExifCommon::validateOffset},
	{(uint16_t)ExifCommon::kExifTagID::kExifGPSInfo,               ExifCommon::kDirectoryType::kDirectoryExif, ExifCommon::kTagType::kUnsignedLong,     1, (uint16_t)ExifCommon::kTagId::kGPSSubDirectory,    &ExifCommon::validateOffset},
	{(uint16_t)ExifCommon::kExifGPSTagID::kExifGPSLatitudeRef,     ExifCommon::kDirectoryType::kDirectoryGPS,  ExifCommon::kTagType::kASCIIString,      1, (uint16_t)ExifCommon::kTagId::kGPSLatitudeRef,     &ExifCommon::validateString},
	{(uint16_t)ExifCommon::kExifGPSTagID::kExifGPSLatitude,        ExifCommon::kDirectoryType::kDirectoryGPS,  ExifCommon::kTagType::kUnsignedRational, 3, (uint16_t)ExifCommon::kTagId::kGPSLatitude,        &ExifCommon::validateU64Rational3},
	{(uint16_t)ExifCommon::kExifGPSTagID::kExifGPSLongitudeRef,    ExifCommon::kDirectoryType::kDirectoryGPS,  ExifCommon::kTagType::kASCIIString,      1, (uint16_t)ExifCommon::kTagId::kGPSLongitudeRef,    &ExifCommon::validateString},
	{(uint16_t)ExifCommon::kExifGPSTagID::kExifGPSLongitude,       ExifCommon::kDirectoryType::kDirectoryGPS,  ExifCommon::kTagType::kUnsignedRational, 3, (uint16_t)ExifCommon::kTagId::kGPSLongitude,       &ExifCommon::validateU64Rational3},
	{(uint16_t)ExifCommon::kExifGPSTagID::kExifGPSAltitudeRef,     ExifCommon::kDirectoryType::kDirectoryGPS,  ExifCommon::kTagType::kSignedByte,       1, (uint16_t)ExifCommon::kTagId::kGPSAltitudeRef,     &ExifCommon::validateAltitude},
	{(uint16_t)ExifCommon::kExifGPSTagID::kExifGPSAltitude,        ExifCommon::kDirectoryType::kDirectoryGPS,  ExifCommon::kTagType::kUnsignedRational, 1, (uint16_t)ExifCommon::kTagId::kGPSAltitude,        &ExifCommon::validateU64Rational},
	{(uint16_t)ExifCommon::kExifGPSTagID::kExifGPSTimeStamp,       ExifCommon::kDirectoryType::kDirectoryGPS,  ExifCommon::kTagType::kUnsignedRational, 3, (uint16_t)ExifCommon::kTagId::kGPSTimeStamp,       &ExifCommon::validateU64Rational3},
	{(uint16_t)ExifCommon::kExifGPSTagID::kExifGPSSpeedRef,        ExifCommon::kDirectoryType::kDirectoryGPS,  ExifCommon::kTagType::kASCIIString,      1, (uint16_t)ExifCommon::kTagId::kGPSSpeedRef,        &ExifCommon::validateString},
	{(uint16_t)ExifCommon::kExifGPSTagID::kExifGPSSpeed,           ExifCommon::kDirectoryType::kDirectoryGPS,  ExifCommon::kTagType::kUnsignedRational, 1, (uint16_t)ExifCommon::kTagId::kGPSSpeed,           &ExifCommon::validateU64Rational},
	{(uint16_t)ExifCommon::kExifGPSTagID::kExifGPSImgDirectionRef, ExifCommon::kDirectoryType::kDirectoryGPS,  ExifCommon::kTagType::kASCIIString,      1, (uint16_t)ExifCommon::kTagId::kGPSImgDirectionRef, &ExifCommon::validateString},
	{(uint16_t)ExifCommon::kExifGPSTagID::kExifGPSImgDirection,    ExifCommon::kDirectoryType::kDirectoryGPS,  ExifCommon::kTagType::kUnsignedRational, 1, (uint16_t)ExifCommon::kTagId::kGPSImgDirection,    &ExifCommon::validateU64Rational},
	{(uint16_t)ExifCommon::kExifGPSTagID::kExifGPSDestBearingRef,  ExifCommon::kDirectoryType::kDirectoryGPS,  ExifCommon::kTagType::kASCIIString,      1, (uint16_t)ExifCommon::kTagId::kGPSDestBearingRef,  &ExifCommon::validateString},
	{(uint16_t)ExifCommon::kExifGPSTagID::kExifGPSDestBearing,     ExifCommon::kDirectoryType::kDirectoryGPS,  ExifCommon::kTagType::kUnsignedRational, 1, (uint16_t)ExifCommon::kTagId::kGPSDestBearing,     &ExifCommon::validateU64Rational},
	{(uint16_t)ExifCommon::kExifGPSTagID::kExifGPSDateStamp,       ExifCommon::kDirectoryType::kDirectoryGPS,  ExifCommon::kTagType::kASCIIString,      1, (uint16_t)ExifCommon::kTagId::kGPSDateStamp,       &ExifCommon::validateString},
};

ExifCommon::ExifCommon()
{
	memset(m_TagHeaderLookup, 0xFF, sizeof(m_TagHeaderLookup));
	for(uint32_t tagIndex = 0; tagIndex < (uint32_t)kTagId::kTagIdMax; tagIndex++) {
		// This verification will catch any tag headers that were added out of order
#if DEBUG
		SECURE_ASSERT(s_mTagHeaders[tagIndex].m_internalId == tagIndex); // Sanity check
#endif
		m_TagHeaderLookup[(uint32_t)s_mTagHeaders[tagIndex].m_dirType][s_mTagHeaders[tagIndex].m_exifId] = tagIndex; // create reverse lookup from exifId to our internal id.
	}
}

ExifCommon& ExifCommon::Instance()
{
	static ExifCommon s_exifcommon;
	return s_exifcommon;
}

bool ExifCommon::TagHeaderType::relaxedTypeAndCountCheck(uint16_t exifType, uint32_t count, uint16_t signedType, uint16_t unsignedType) const
{
	return (count == m_count) && ((exifType == signedType) || (exifType == unsignedType));
}

bool ExifCommon::TagHeaderType::verifyTypeAndCount(unsigned short exifType, uint32_t count) const
{
	switch(m_type) {
		// relax type-checking for signed vs unsigned of the same type, since there are images out there, that don't stick to the exact spec
		case kTagType::kUnsignedByte:
		case kTagType::kSignedByte: {      // (1 byte per component)
			return relaxedTypeAndCountCheck(exifType, count, (uint16_t)kTagType::kUnsignedByte, (uint16_t)kTagType::kSignedByte);
			break;
		}

		case kTagType::kUnsignedShort:
		case kTagType::kSignedShort: {      // (2 bytes per component)
			return relaxedTypeAndCountCheck(exifType, count, (uint16_t)kTagType::kUnsignedShort, (uint16_t)kTagType::kSignedShort);
			break;
		}

		case kTagType::kUnsignedLong:
		case kTagType::kSignedLong: {      // (4 bytes per component)
			return relaxedTypeAndCountCheck(exifType, count, (uint16_t)kTagType::kUnsignedLong, (uint16_t)kTagType::kSignedLong);
			break;
		}

		case kTagType::kUnsignedRational:
		case kTagType::kSignedRational: {  // (8 bytes per component)
			return relaxedTypeAndCountCheck(exifType, count, (uint16_t)kTagType::kUnsignedRational, (uint16_t)kTagType::kSignedRational);
			break;
		}

		case kTagType::kSingle: {  // (4 bytes per component)
			return relaxedTypeAndCountCheck(exifType, count, (uint16_t)kTagType::kSingle, (uint16_t)kTagType::kSingle);
			break;
		}

		case kTagType::kDouble: {  // (8 bytes per component)
			return relaxedTypeAndCountCheck(exifType, count, (uint16_t)kTagType::kDouble, (uint16_t)kTagType::kDouble);
			break;
		}

		case kTagType::kASCIIString: {
			if(((ExifCommon::kTagType)exifType != kTagType::kASCIIString) ||
			(count >= kMaxExifStringLength) ||
			(count == 0)) {
				return false;
			}
			break;
		}

		default: {
			return false;
		}
	}
	return true;
}

const ExifCommon::TagHeaderType* ExifCommon::getTagHeader(kTagId tagId)
{
	return &s_mTagHeaders[(uint16_t)tagId];
}

ExifCommon::kDirectoryType ExifCommon::getDirectoryType(kExifTagID exifDirectoryTag)
{
	if(exifDirectoryTag == ExifCommon::kExifTagID::kExifOffset) {
		return ExifCommon::kDirectoryType::kDirectoryExif;
	} else {
		return ExifCommon::kDirectoryType::kDirectoryGPS;
	}
}

int32_t ExifCommon::getTagId(uint32_t dirType, uint32_t exifDirectoryTag)
{
	return m_TagHeaderLookup[dirType][exifDirectoryTag];
}

//-----------------------------------------------------------------------------------------
// All validation functions
bool ExifCommon::validateOrientation(const void* valueAddress, uint32_t)
{
	unsigned short value = *(const unsigned short*)valueAddress;
	if( value >= kImageOrientation_Up && value <= kImageOrientation_Right ) {
		return true;
	}
	return false;
}

bool ExifCommon::validateString(const void* valueAddress, uint32_t count)
{
	const ExifCommon::ExifString* string = (const ExifCommon::ExifString*)valueAddress;
	if (string->m_length != count) { // failed to read the string
		return false;
	}
	if(string->m_length > 0) {
		return (string->m_string[count - 1] == 0); // should be null terminated
	} else {
		return false; // string of 0 length
	}
}

bool ExifCommon::validateU64Rational(const void* valueAddress, uint32_t count)
{
	Rational<uint32_t> value = *(Rational<uint32_t>*)valueAddress;
	if(value.m_Denominator == 0) {   // divide by 0
		return false;
	}
	if(((int32_t)value.m_Nominator < 0) || ((int32_t)value.m_Denominator < 0)) {
		return false;
	}
	return true;
}

bool ExifCommon::validateU64Rational3(const void* valueAddress, uint32_t count)
{
	Rational<uint32_t>* value = (Rational<uint32_t>*)valueAddress;
	if(!validateU64Rational(value, count)) {
		return false;
	}
	value++;
	if(!validateU64Rational(value, count)) {
		return false;
	}
	value++;
	if(!validateU64Rational(value, count)) {
		return false;
	}
	return true;
}

bool ExifCommon::validateResolutionUnit(const void* valueAddress, uint32_t count)
{
	unsigned short value = *(const unsigned short*)valueAddress;
	if( value >= EResolutionUnit::kNone && value <= EResolutionUnit::kCm ) {
		return true;
	}
	return false;
}

bool ExifCommon::validateOffset(const void* valueAddress, uint32_t count)
{
	return true;
}

bool ExifCommon::validateAltitude(const void* valueAddress, uint32_t count)
{
	int8_t value = *(int8_t*)valueAddress;
	if( value >= EAltitudeRef::kAboveSeaLevel && value <= EAltitudeRef::kBelowSeaLevel ) {
		return true;
	}
	return false;
}

void ExifCommon::IFDStructure::write(MemoryStreamWriter& memoryStream, uint32_t offset)
{
	memoryStream.put_short_advance(tag);
	memoryStream.put_short_advance(type);
	memoryStream.put_uint_advance(count);
	if(isOffset) {
		memoryStream.put_uint_advance(value_offset + offset);
	} else {
		switch(type) {
			case (int)ExifCommon::kTagType::kUnsignedByte:
			case (int)ExifCommon::kTagType::kSignedByte:{
				memoryStream.put_byte_advance((uint8_t)value_offset);
				memoryStream.put_byte_advance((uint8_t)0);
				memoryStream.put_short_advance((uint16_t)0);
				break;
			}
			case (int)ExifCommon::kTagType::kUnsignedShort: {
				memoryStream.put_short_advance((uint16_t)value_offset);
				memoryStream.put_short_advance((uint16_t)0);
				break;
			}
			case (int)ExifCommon::kTagType::kUnsignedLong: {
				memoryStream.put_uint_advance(value_offset);
				break;
			}
			case (int)ExifCommon::kTagType::kASCIIString: { // string of length 4 or less
				const uint8_t* pStr = (const uint8_t*)&value_offset;
				memoryStream.put_raw_data_advance(pStr, 4);
				break;
			}
			default: {
				memoryStream.put_uint_advance(value_offset);
				break;
			}
		}
	}
}

ExifCommon::ExifDirectory::ExifDirectory(bool is_be) : m_numEntries(0)
{
	m_entries = new IFDStructure[kMaxExifDirectoryEntries];
	metaDataMemory = new uint8_t[kMaxMetaDataSize];
	metaDataWriter = new MemoryStreamWriter(metaDataMemory, kMaxMetaDataSize, is_be);
}

ExifCommon::ExifDirectory::~ExifDirectory()
{
	delete[] m_entries;
	delete[] metaDataMemory;
	delete metaDataWriter;
}

ExifCommon::IFDStructure* ExifCommon::ExifDirectory::getNewEntry()
{
	SECURE_ASSERT(m_numEntries < kMaxExifDirectoryEntries);
	IFDStructure* entry = &m_entries[m_numEntries];
	m_numEntries++;
	return entry;
}

void ExifCommon::ExifDirectory::write(MemoryStreamWriter& memoryStream, uint32_t& offset) const
{
	// write all directory info
	if(m_numEntries > 0) {
		offset += (2 + m_numEntries * 12);
		memoryStream.put_short_advance(m_numEntries);
		for(uint16_t entryIndex = 0; entryIndex < m_numEntries; entryIndex++) {
			m_entries[entryIndex].write(memoryStream, offset);
		}
		memoryStream.put_raw_data_advance(metaDataWriter->getData(), metaDataWriter->getSize());
		offset += metaDataWriter->getSize();
	}
}

}
