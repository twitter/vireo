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

#include <inttypes.h>
#include "imagecore/image/image.h"
#include "imagecore/utils/endianutils.h"
#include "imagecore/utils/memorystream.h"
#include "imagecore/imagecore.h"
#include "imagecore/utils/mathtypes.h"
#include "imagecore/utils/mathutils.h"
#include "exifcommon.h"

namespace imagecore {

class ExifReader
{
public:

	ExifReader();
	~ExifReader();
	void initialize(const uint8_t* Data, uint32_t dataSize);

	// Most functionality is in this function, given an internal exif tag id it tries to find it in provided exif data. If tag is no found, default value is returned,
	// if tag is found, but contains invalid data, default value is returned.
	// TODO: Optimize this to avoid full traverse for multiple queries
	template<typename ReturnType>
	ReturnType getValue(ReturnType DefaultValue, ExifCommon::kTagId inId)
	{
		if(m_cachedEntries[(uint32_t)inId]) { // value already been found and cached?
			return *(ReturnType*)m_cachedEntries[(uint32_t)inId];
		}
		if((!m_valid) || (m_directoryInfo == NULL)) {
			return DefaultValue;
		}
		do
		{
			const uint8_t* directoryData = m_directoryInfo->m_data;
			MemoryStreamReader memoryStream(directoryData, (uint16_t)(m_dataSize - (directoryData - m_TIFFHeader)), m_isBe);
			uint16_t dirSize = memoryStream.get_short_advance();
			if(!memoryStream.isLastReadValid()) {
				return DefaultValue;
			}

			for(; m_tagIndex < dirSize; m_tagIndex++ ) {
				memoryStream.seek(2 + m_tagIndex * kDirectoryEntrySize); // advance to current tag index
				uint16_t exifTagId = memoryStream.get_short_advance(); // exif id
				if(!memoryStream.isLastReadValid()) {
					m_tagIndex++;
					return DefaultValue;
				}
				int32_t tagId = ExifCommon::Instance().getTagId((uint32_t)m_directoryInfo->m_type, exifTagId); // translate exif id to our internal id
                if(tagId != -1) // is this an id that we potentially care about?
				{
					switch((ExifCommon::kExifTagID)exifTagId)
					{
						// offset to another subdirectory
						case ExifCommon::kExifTagID::kExifOffset:
						case ExifCommon::kExifTagID::kExifGPSInfo:
						{
							memoryStream.advance(6); // skip type and count
							uint32_t offset = memoryStream.get_uint_advance();
							if(!memoryStream.isLastReadValid()) {
								m_tagIndex++;
								return DefaultValue;
							}
							if(validateOffset(offset, sizeof(uint16_t) + kDirectoryEntrySize)) { // should have at least enough space for dir size and one entry.
								if(m_directoryQueue.push(m_TIFFHeader + offset, ExifCommon::Instance().getDirectoryType((ExifCommon::kExifTagID)exifTagId)) != 0) {
									m_tagIndex++;
									return DefaultValue;
								}
							}
							break;
						}

						default:
						{
							uint16_t tagType = memoryStream.get_short_advance();
							if(!memoryStream.isLastReadValid()) {
								m_tagIndex++;
								return DefaultValue;
							}
							uint32_t tagCount = memoryStream.get_uint_advance();
							if(!memoryStream.isLastReadValid()) {
								m_tagIndex++;
								return DefaultValue;
							}

							const ExifCommon::TagHeaderType* TagHeaderRef = ExifCommon::Instance().getTagHeader((ExifCommon::kTagId)tagId);
							if(TagHeaderRef->verifyTypeAndCount(tagType, tagCount)) {
								uint8_t readBuffer[512]; // we currently dont have any data type that exceeds 512 bytes, biggest one is ExifString at 258 bytes
								uint32_t size;
								readValue(readBuffer, size, memoryStream, tagType, tagCount);
								if(!memoryStream.isLastReadValid()) {
									m_tagIndex++;
									return DefaultValue;
								}
								if( TagHeaderRef->m_rangeValidator(readBuffer, tagCount)) {
									m_cachedEntries[tagId] = createCacheEntry(size, readBuffer);
									if((uint32_t)inId == tagId) { // is it the actual value we are looking for?
										m_tagIndex++;
										ReturnType res;
										memcpy(&res, readBuffer, sizeof(ReturnType));
										return res;
									}
								}
							}
							break;
						}
					}
				}
			}
			m_directoryInfo = m_directoryQueue.pop();   // returns null if queue is empty
			m_tagIndex = 0;
		}while(m_directoryInfo != nullptr);
		return DefaultValue;
	}

private:

	struct directoryInfoType
	{
		directoryInfoType() : m_data(nullptr),
		m_type(ExifCommon::kDirectoryType::kDirectoryExif)
		{}

		directoryInfoType(const uint8_t* data, ExifCommon::kDirectoryType type) : m_data(data),
		m_type(type)
		{}

		const uint8_t* m_data;
		ExifCommon::kDirectoryType m_type;  // currently exif or gps
	};

	// This should be abstracted with some generic queue implementation.
	// Left in here for now since this method is the only thing that uses this helper class.
	const uint32_t kDirectoryEntrySize = 12;
	static const uint32_t kDirectoryQueueSize = 8;
	class DirectoryQueueType
	{
		public:
			DirectoryQueueType() : m_readPtr(0),
			m_writePtr(0)
			{}

			int push(const uint8_t* data, ExifCommon::kDirectoryType dirType)
			{
				if(m_writePtr == ExifReader::kDirectoryQueueSize) {
					return -1;
				}
				m_directoryInfos[m_writePtr] = ExifReader::directoryInfoType(data, dirType);
				m_writePtr++;
				return 0;
			}

			const ExifReader::directoryInfoType* pop()
			{
				if(isEmpty()) {
					return nullptr;
				}
				return &m_directoryInfos[m_readPtr++];
			}

		private:
			bool isEmpty()
			{
				return m_readPtr == m_writePtr;
			}

			ExifReader::directoryInfoType m_directoryInfos[ExifReader::kDirectoryQueueSize];
			uint16_t m_readPtr;
			uint16_t m_writePtr;
	};

	void readValue(uint8_t* buffer, uint32_t& size, MemoryStreamReader& memoryStream, uint16_t tagType, uint32_t tagCount);
	void readValue(uint16_t& value, MemoryStreamReader& memoryStream, uint32_t) const;
	void readValue(int8_t& value, MemoryStreamReader& memoryStream, uint32_t) const;
	void readValue(ExifCommon::ExifString& value, MemoryStreamReader& memoryStream, uint32_t count) const;
	void readValue(Rational<uint32_t>& value, MemoryStreamReader& memoryStream, uint32_t count) const;
	void readValue(ExifCommon::ExifU64Rational3& value, MemoryStreamReader& memoryStream, uint32_t count) const;
	bool validateOffset(uint32_t offset, uint32_t count) const;

	void* createCacheEntry(uint32_t entrySize, uint8_t* entryData);

	static const uint32_t kHeaderSize = 8;
	directoryInfoType m_rootDirectory;  // the main Exif directory
	DirectoryQueueType m_directoryQueue;
	const directoryInfoType* m_directoryInfo;
	uint32_t m_tagIndex;

	void* m_cachedEntries[(int32_t)ExifCommon::kTagId::kTagIdMax];

	static const uint32_t kCacheSize = 64 * 1024;
	uint32_t m_cacheOffset;
	uint8_t* m_cacheData;

	const uint8_t* m_TIFFHeader;        // beginning of the whole data block
	uint32_t m_dataSize;
	bool m_isBe;
	bool m_valid;
};

}
