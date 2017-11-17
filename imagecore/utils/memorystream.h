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

#include "imagecore/imagecore.h"
#include "imagecore/utils/endianutils.h"

class MemoryStreamReader
{
public:
	MemoryStreamReader(const uint8_t* data, uint32_t size, bool is_be) : m_dataStart(data), m_dataEnd(data + size), m_dataCur(data), m_isBe(is_be), m_isLastReadValid(true) {}

	/* reading */
	inline uint8_t get_byte()
	{
		uint8_t result = 0;
		m_isLastReadValid = (m_dataCur + sizeof(uint8_t) <= m_dataEnd);
		if(m_isLastReadValid) {
			result = m_dataCur[0];
		}
		return result;
	}

	inline uint8_t get_byte_advance()
	{
		uint8_t res = get_byte();
		m_dataCur += sizeof(uint8_t);
		return res;
	}

	inline uint16_t get_short()
	{
		m_isLastReadValid = (m_dataCur + sizeof(uint16_t) <= m_dataEnd);
		uint16_t result = 0;
		if(m_isLastReadValid) {
			if (m_isBe ) {
				result = (unsigned int)m_dataCur[0] << 8 | ((unsigned int)m_dataCur[1]);
			} else {
				result =  (unsigned int)m_dataCur[1] << 8 | ((unsigned int)m_dataCur[0]);
			}
		}
		return result;
	}

	inline uint16_t get_short_advance()
	{
		uint16_t res = get_short();
		m_dataCur += sizeof(uint16_t);
		return res;
	}

	inline uint32_t get_uint()
	{
		m_isLastReadValid = (m_dataCur + sizeof(uint32_t) <= m_dataEnd);
		unsigned int result = 0;
		if(m_isLastReadValid) {
			if( m_isBe ) {
				result = (m_dataCur[0] << 24) | (m_dataCur[1] << 16) | (m_dataCur[2] << 8) | (m_dataCur[3]);
			} else {
				result = (m_dataCur[3] << 24) | (m_dataCur[2] << 16) | (m_dataCur[1] << 8) | (m_dataCur[0]);
			}
		}
		return result;
	}

	inline uint32_t get_uint_advance()
	{
		uint32_t res = get_uint();
		m_dataCur += sizeof(uint32_t);
		return res;
	}

	inline void advance(uint16_t length)
	{
		m_dataCur += length;
	}

	inline void seek(uint16_t length)
	{
		m_dataCur = m_dataStart + length;
	}

	inline bool isLastReadValid() const
	{
		return m_isLastReadValid;
	}

private:
	const uint8_t* m_dataStart;
	const uint8_t* m_dataEnd;
	const uint8_t* m_dataCur;
	bool m_isBe;
	bool m_isLastReadValid;
};

class MemoryStreamWriter
{
public:
	MemoryStreamWriter(uint8_t* data, uint32_t size, bool is_be) : m_dataStart(data), m_dataEnd(data + size), m_dataCur(data),  m_isBe(is_be) {}

	/* writing */
	inline void put_byte(uint8_t value)
	{
		SECURE_ASSERT(m_dataCur + sizeof(uint8_t) <= m_dataEnd);
		m_dataCur[0] = value;
	}

	inline void put_byte_advance(uint8_t value)
	{
		put_byte(value);
		m_dataCur += sizeof(uint8_t);
	}

	inline void put_short(uint16_t value)
	{
		SECURE_ASSERT(m_dataCur + sizeof(uint16_t) <= m_dataEnd);
		if (m_isBe ) {
			value = htobe16(value);
		}
		memcpy(m_dataCur, &value, sizeof(uint16_t));
	}

	inline void put_short_advance(uint16_t value)
	{
		put_short(value);
		m_dataCur += sizeof(uint16_t);
	}

	inline void put_uint(uint32_t value)
	{
		SECURE_ASSERT(m_dataCur + sizeof(uint32_t) <= m_dataEnd);
		if (m_isBe ) {
			value = htobe32(value);
		}
		memcpy(m_dataCur, &value, sizeof(uint32_t));
	}

	inline void put_uint_advance(uint32_t value)
	{
		put_uint(value);
		m_dataCur += sizeof(uint32_t);
	}

	inline void put_raw_data_advance(const uint8_t* data, uint32_t size)
	{
		SECURE_ASSERT(m_dataCur + size <= m_dataEnd);
		memcpy(m_dataCur, data, size);
		m_dataCur += size;
	}

	inline void advance(uint16_t length)
	{
		m_dataCur += length;
	}

	inline uint32_t get_offset()
	{
		return (uint32_t)(m_dataCur - m_dataStart);
	}

	inline const uint8_t* getData()
	{
		return m_dataStart;
	}

	inline uint32_t getSize()
	{
		return (uint32_t)(m_dataCur - m_dataStart);
	}

private:
	const uint8_t* m_dataStart;
	const uint8_t* m_dataEnd;
	uint8_t* m_dataCur;
	bool m_isBe;
};
