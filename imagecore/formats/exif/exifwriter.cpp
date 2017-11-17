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

#include "exifwriter.h"

namespace imagecore {

ExifWriter::ExifWriter(bool is_be)
{
	for( uint16_t dirIndex = 0; dirIndex < (uint16_t)ExifCommon::kDirectoryType::kDirectoryMaxDirs; dirIndex++) {
		m_directories[dirIndex] = new ExifCommon::ExifDirectory(is_be);
	}
}

ExifWriter::~ExifWriter()
{
	for( uint16_t dirIndex = 0; dirIndex < (uint16_t)ExifCommon::kDirectoryType::kDirectoryMaxDirs; dirIndex++) {
		delete m_directories[dirIndex];
	}
}

uint32_t ExifWriter::writeValue(uint8_t value, MemoryStreamWriter& memoryStream, uint32_t& value_offset, bool& isOffest) const
{
	value_offset = value;
	isOffest = false;
	return 1;
}


uint32_t ExifWriter::writeValue(uint16_t value, MemoryStreamWriter& memoryStream, uint32_t& value_offset, bool& isOffest) const
{
	value_offset = value;
	isOffest = false;
	return 1;
}

uint32_t ExifWriter::writeValue(const ExifCommon::ExifString& value, MemoryStreamWriter& memoryStream, uint32_t& value_offset, bool& isOffest) const
{
	if(value.m_length <= 4)  // values of 4 bytes in length or less are stored directly
	{
		for( uint32_t i = 0; i < value.m_length; i++) {
			char* str = (char*)&value_offset;
			str[i] = value.m_string[i];
		}
		isOffest = false;
	}
	else
	{
		value_offset = memoryStream.get_offset();
		for( uint32_t i = 0; i < value.m_length; i++) {
			memoryStream.put_byte_advance(value.m_string[i]);
		}
		isOffest = true;
	}
	return value.m_length;
}

uint32_t ExifWriter::writeValue(const Rational<uint32_t>& value, MemoryStreamWriter& memoryStream, uint32_t& value_offset, bool& isOffest) const
{
	value_offset = memoryStream.get_offset();
	memoryStream.put_uint_advance(value.m_Nominator);
	memoryStream.put_uint_advance(value.m_Denominator);
	isOffest = true;
	return 1;
}

uint32_t ExifWriter::writeValue(const ExifCommon::ExifU64Rational3& value, MemoryStreamWriter& memoryStream, uint32_t& value_offset, bool& isOffest) const
{
	value_offset = memoryStream.get_offset();
	memoryStream.put_uint_advance(value.m_value[0].m_Nominator);
	memoryStream.put_uint_advance(value.m_value[0].m_Denominator);
	memoryStream.put_uint_advance(value.m_value[1].m_Nominator);
	memoryStream.put_uint_advance(value.m_value[1].m_Denominator);
	memoryStream.put_uint_advance(value.m_value[2].m_Nominator);
	memoryStream.put_uint_advance(value.m_value[2].m_Denominator);
	isOffest = true;
	return 3;
}

uint32_t ExifWriter::writeValue(const GPSSubDirectory value, MemoryStreamWriter& memoryStream, uint32_t& value_offset, bool& isOffest) const
{
	value_offset = memoryStream.get_offset();
	isOffest = true;
	return 1;
}

bool ExifWriter::isEmpty() const
{
	for(uint32_t dirIndex = 0; dirIndex < (uint32_t)ExifCommon::kDirectoryType::kDirectoryMaxDirs; dirIndex++) {
		if(m_directories[dirIndex]->m_numEntries > 0) {
			return false;
		}
	}
	return true;
}

void ExifWriter::WriteToStream(MemoryStreamWriter& memoryStream)
{
	memoryStream.put_byte_advance('E');
	memoryStream.put_byte_advance('x');
	memoryStream.put_byte_advance('i');
	memoryStream.put_byte_advance('f');
	memoryStream.put_short_advance(0); // 6 bytes
	memoryStream.put_short_advance(EXIF_BIG_ENDIAN);
	memoryStream.put_short_advance(TFIF_MARKER);
	memoryStream.put_uint_advance(8); // +8 bytes

	uint32_t metadataOffset = 8;

	ExifCommon::ExifDirectory* directoryExif = m_directories[(uint32_t)ExifCommon::kDirectoryType::kDirectoryExif];
	ExifCommon::ExifDirectory* directoryGPS = m_directories[(uint32_t)ExifCommon::kDirectoryType::kDirectoryGPS];

	if(directoryGPS->m_numEntries > 0) {
		GPSSubDirectory value;
		putValue(value, ExifCommon::kTagId::kGPSSubDirectory);
	}
	directoryExif->write(memoryStream, metadataOffset); // this modifies metadatOffset to include all written data
	directoryGPS->write(memoryStream, metadataOffset);
}

}
