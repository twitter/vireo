//
//  exifwriter.h
//  ImageCore
//
//  Created by Paul Melamed on 11/24/14.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

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
