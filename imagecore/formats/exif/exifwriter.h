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

#include "exifreader.h"

namespace imagecore {

class ExifWriter
{
	public:
		ExifWriter(bool is_be);
		~ExifWriter();

		template<typename valueType>
		void putValue(valueType value, ExifCommon::kTagId internalId)
		{
			const ExifCommon::TagHeaderType* tagHeader = ExifCommon::Instance().getTagHeader(internalId);
			ExifCommon::ExifDirectory* directory = m_directories[(uint32_t)tagHeader->m_dirType];
			ExifCommon::IFDStructure* newEntry = directory->getNewEntry();
			newEntry->tag = tagHeader->m_exifId;
			newEntry->type = (uint16_t)tagHeader->m_type;
			newEntry->count = writeValue(value, *directory->metaDataWriter, newEntry->value_offset, newEntry->isOffset);
		}

		void WriteToStream(MemoryStreamWriter& memoryStream);
		bool isEmpty() const;

	private:

		ExifCommon::ExifDirectory* m_directories[(uint16_t)ExifCommon::kDirectoryType::kDirectoryMaxDirs];
		struct GPSSubDirectory {};

		uint32_t writeValue(uint8_t value, MemoryStreamWriter& memoryStream, uint32_t& value_offset, bool& isOffest) const;
		uint32_t writeValue(uint16_t value, MemoryStreamWriter& memoryStream, uint32_t& value_offset, bool& isOffest) const;
		uint32_t writeValue(const ExifCommon::ExifString& value, MemoryStreamWriter& memoryStream, uint32_t& value_offset, bool& isOffest) const;
		uint32_t writeValue(const Rational<uint32_t>& value, MemoryStreamWriter& memoryStream, uint32_t& value_offset, bool& isOffest) const;
		uint32_t writeValue(const ExifCommon::ExifU64Rational3& value, MemoryStreamWriter& memoryStream, uint32_t& value_offset, bool& isOffest) const;
		uint32_t writeValue(const GPSSubDirectory value, MemoryStreamWriter& memoryStream, uint32_t& value_offset, bool& isOffest) const;
};

}
