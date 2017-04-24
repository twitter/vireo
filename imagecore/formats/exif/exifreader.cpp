//
//  ExifReader.cpp
//  ImageCore
//
//  Created by Paul Melamed on 9/29/14.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#include "imagecore.h"

#include "exifreader.h"

#include <new>

namespace imagecore {

ExifReader::ExifReader() : m_cacheOffset(0),
m_TIFFHeader(nullptr),
m_dataSize(0),
m_valid(false)
{
	m_cacheData = new uint8_t[kCacheSize];
}

ExifReader::~ExifReader()
{
	if(m_cacheData) {
		delete [] m_cacheData;
	}
}

void ExifReader::initialize(const uint8_t* EXIFData, uint32_t dataSize)
{
	SECURE_ASSERT(m_TIFFHeader == nullptr);
	m_TIFFHeader = EXIFData;
	m_dataSize = dataSize;
	memset(m_cachedEntries, 0, sizeof(m_cachedEntries));
	m_isBe = ((uint32_t)m_TIFFHeader[0] << 8 | ((uint32_t)m_TIFFHeader[1])) == EXIF_BIG_ENDIAN;

	MemoryStreamReader memoryStream(m_TIFFHeader, m_dataSize, m_isBe);
	memoryStream.advance(2);
	uint32_t tfif_marker = memoryStream.get_short_advance();
	if( tfif_marker != TFIF_MARKER ) {
		return;
	}
	uint32_t length = memoryStream.get_uint_advance();
	if( length != kHeaderSize) {
		m_valid = false;
		return;
	}
	m_valid = true;
	m_rootDirectory = directoryInfoType(m_TIFFHeader + kHeaderSize, ExifCommon::kDirectoryType::kDirectoryExif);
	m_directoryInfo = &m_rootDirectory;

	// IDF1 offset
	MemoryStreamReader IDF1Stream(m_directoryInfo->m_data, (uint16_t)(m_dataSize - (m_directoryInfo->m_data - m_TIFFHeader)), m_isBe);
	uint16_t dirSize = IDF1Stream.get_short_advance();
	IDF1Stream.seek(2 + dirSize * kDirectoryEntrySize); // advance past the last entry
	uint32_t IDF1Offset = IDF1Stream.get_uint();
	// The 0th IFD Offset of Next IFD indicates the start address of the 1st IFD (thumbnail images). When the 1st IFD is
	// not recorded, the 0th IFD Offset of Next IFD terminates with 00000000.H.
	// In some sample images it wasn't 0, but contained invalid offset
	if(IDF1Stream.isLastReadValid() && (IDF1Offset != 0) && (validateOffset(IDF1Offset, sizeof(uint16_t) + kDirectoryEntrySize))) {
		m_directoryQueue.push(m_TIFFHeader + IDF1Offset, ExifCommon::kDirectoryType::kDirectoryExif);
	}
	m_tagIndex = 0;
	m_cacheOffset = 0;
}

void ExifReader::readValue(uint8_t* buffer, uint32_t& size, MemoryStreamReader& memoryStream, uint16_t tagType, uint32_t tagCount)
{
	switch ((ExifCommon::kTagType)tagType) {
		case ExifCommon::kTagType::kASCIIString : {
			size = sizeof(ExifCommon::ExifString);
			ExifCommon::ExifString* value = new(buffer) ExifCommon::ExifString();
			readValue(*value, memoryStream, tagCount);
			break;
		}
		case ExifCommon::kTagType::kSignedByte:
		case ExifCommon::kTagType::kUnsignedByte: {
			size = sizeof(uint8_t);
			int8_t* value = (int8_t*)buffer;
			readValue(*value, memoryStream, tagCount);
			break;
		}
		case ExifCommon::kTagType::kSignedShort:
		case ExifCommon::kTagType::kUnsignedShort: {
			size = sizeof(uint16_t);
			uint16_t* value = (uint16_t*)buffer;
			readValue(*value, memoryStream, tagCount);
			break;
		}
		case ExifCommon::kTagType::kUnsignedRational: {
			if(tagCount == 1) {
				size = sizeof(Rational<uint32_t>); // use sizeof here, because that is how many bytes will be copied to the final return type
				Rational<uint32_t>* value = new(buffer) Rational<uint32_t>(); // use placement new so that constructor gets called and 'value' is initialized, since readValue can fail (Valgrind fix)
				readValue(*value, memoryStream, tagCount);
				value->m_Signed = false; // explicitly set the sign flag
			} else {
				size = sizeof(ExifCommon::ExifU64Rational3);
				ExifCommon::ExifU64Rational3* value = new(buffer) ExifCommon::ExifU64Rational3();
				readValue(*value, memoryStream, tagCount);
				value->setSign(false);
			}
			break;
		}
		case ExifCommon::kTagType::kSignedRational: {
			if(tagCount == 1) {
				size = sizeof(Rational<uint32_t>);
				Rational<uint32_t>* value = new(buffer) Rational<uint32_t>();
				readValue(*value, memoryStream, tagCount);
				value->m_Signed = true;
			} else {
				size = sizeof(ExifCommon::ExifU64Rational3);
				ExifCommon::ExifU64Rational3* value = new(buffer) ExifCommon::ExifU64Rational3();
				readValue(*value, memoryStream, tagCount);
				value->setSign(true);
			}
			break;
		}
		default:
			SECURE_ASSERT(0); // We have nothing that requires support for these types yet
			break;
	}
}

void ExifReader::readValue(int8_t& value, MemoryStreamReader& memoryStream, uint32_t) const
{
	value = memoryStream.get_byte();
}

void ExifReader::readValue(uint16_t& value, MemoryStreamReader& memoryStream, uint32_t) const
{
	value = memoryStream.get_short();
}

void ExifReader::readValue(ExifCommon::ExifString& value, MemoryStreamReader& memoryStream, uint32_t count) const
{
	if(count <= 4) {// values of 4 bytes in length or less are stored directly
		for( uint32_t i = 0; i < count; i++) {
			value.m_string[i] = memoryStream.get_byte_advance();
		}
	} else {
		count = count < ExifCommon::kMaxExifStringLength ? count : ExifCommon::kMaxExifStringLength;
		uint32_t offset = memoryStream.get_uint();
		if(!validateOffset(offset, count)) {
			return;
		}
		MemoryStreamReader mstream(&m_TIFFHeader[offset], count, m_isBe);
		for( uint32_t i = 0; i < count; i++) {
			value.m_string[i] = mstream.get_byte_advance();
		}
	}
	value.m_length = count;
}

void ExifReader::readValue(Rational<uint32_t>& value, MemoryStreamReader& memoryStream, uint32_t count) const
{
	uint32_t offset = memoryStream.get_uint();
	if(validateOffset(offset, count * Rational<uint32_t>::sizeOf())) {
		MemoryStreamReader mstream(&m_TIFFHeader[offset], count * Rational<uint32_t>::sizeOf(), m_isBe);
		value.m_Nominator = mstream.get_uint_advance();
		value.m_Denominator = mstream.get_uint();
	}
}

void ExifReader::readValue(ExifCommon::ExifU64Rational3& value, MemoryStreamReader& memoryStream, uint32_t count) const
{
	uint32_t offset = memoryStream.get_uint();
	if(validateOffset(offset, ExifCommon::ExifU64Rational3::sizeOf())) {
		MemoryStreamReader mstream(&m_TIFFHeader[offset], ExifCommon::ExifU64Rational3::sizeOf(), m_isBe);
		value.m_value[0].m_Nominator = mstream.get_uint_advance();
		value.m_value[0].m_Denominator = mstream.get_uint_advance();
		value.m_value[1].m_Nominator = mstream.get_uint_advance();
		value.m_value[1].m_Denominator = mstream.get_uint_advance();
		value.m_value[2].m_Nominator = mstream.get_uint_advance();
		value.m_value[2].m_Denominator = mstream.get_uint();
	}
}

bool ExifReader::validateOffset(uint32_t offset, uint32_t count) const
{
	return ((uint64_t)offset + count) < (uint64_t)m_dataSize; // uint64_t to avoid overflow
}

void* ExifReader::createCacheEntry(uint32_t entrySize, uint8_t* entryData)
{
	uint32_t alignedNewOffset = align(m_cacheOffset + entrySize, 4); // always align to 4 byte boundary
	if(alignedNewOffset <= kCacheSize) {
		void* entry = &m_cacheData[m_cacheOffset];
		memcpy(entry, entryData, entrySize);
		m_cacheOffset = alignedNewOffset;
		return entry;
	}
	return nullptr;
}

}
