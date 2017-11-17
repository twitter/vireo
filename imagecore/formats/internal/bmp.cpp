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

#include "bmp.h"
#include "imagecore/utils/endianutils.h"
#include "imagecore/utils/mathutils.h"
#include "imagecore/utils/securemath.h"
#include "imagecore/image/rgba.h"

namespace imagecore {

REGISTER_IMAGE_READER(ImageReaderBMP);

// BMP loading code based on http://www.kalytta.com/bitmap.h - Benjamin Kalytta

#ifndef __LITTLE_ENDIAN__
#ifndef __BIG_ENDIAN__
#define __LITTLE_ENDIAN__
#endif
#endif

#ifdef __LITTLE_ENDIAN__
#define BITMAP_SIGNATURE 0x4d42
#else
#define BITMAP_SIGNATURE 0x424d
#endif

#pragma pack(push, 1)

struct BGRA
{
	uint8_t Blue;
	uint8_t Green;
	uint8_t Red;
	uint8_t Alpha;
};

static inline unsigned int BitCountByMask(unsigned int Mask) {
	unsigned int BitCount = 0;
	while (Mask) {
		Mask &= Mask - 1;
		BitCount++;
	}
	return BitCount;
}

static inline unsigned int BitPositionByMask(unsigned int Mask) {
	return BitCountByMask((Mask & (~Mask + 1)) - 1);
}

static inline unsigned int ComponentByMask(unsigned int Color, unsigned int Mask) {
	unsigned int Component = Color & Mask;
	return Component >> BitPositionByMask(Mask);
}

static inline unsigned int BitCountToMask(unsigned int BitCount) {
	return (BitCount == 32) ? 0xFFFFFFFF : (1 << BitCount) - 1;
}

static unsigned int Convert(unsigned int Color, unsigned int FromBitCount, unsigned int ToBitCount) {
	if (ToBitCount < FromBitCount) {
		Color >>= (FromBitCount - ToBitCount);
	} else {
		Color <<= (ToBitCount - FromBitCount);
		if (Color > 0) {
			Color |= BitCountToMask(ToBitCount - FromBitCount);
		}
	}
	return Color;
}

bool ImageReaderBMP::Factory::matchesSignature(const uint8_t* sig, unsigned int sigLen)
{
	if( sigLen >= 2 && sig[0] == 0x42 && sig[1] == 0x4D ) {
		return true;
	}
	return false;
}

ImageReaderBMP::ImageReaderBMP()
:	m_BitmapHeader(NULL)
,	m_Source(NULL)
,	m_Width(0)
,	m_Height(0)
{
}

ImageReaderBMP::~ImageReaderBMP()
{
	if( m_BitmapHeader != NULL ) {
		free(m_BitmapHeader);
		m_BitmapHeader = NULL;
	}
}

bool ImageReaderBMP::initWithStorage(ImageReader::Storage* source)
{
	m_Source = source;
	return true;
}

bool ImageReaderBMP::readHeader()
{
	if( m_Source->read((char*)&m_BitmapFileHeader, sizeof(BITMAP_FILEHEADER)) != sizeof(BITMAP_FILEHEADER) ) {
		return false;
	}
	if( m_BitmapFileHeader.Signature != BITMAP_SIGNATURE ) {
		return false;
	}
	// Don't read more of the header structure than we have to, to avoid having to seek back later.
	unsigned int headerSize = 0;
	if( m_Source->read((char*)&headerSize, sizeof(unsigned int)) != sizeof(unsigned int) ) {
		return false;
	}
	if (headerSize < sizeof(BITMAP_HEADER) || headerSize > 64 * 1024) {
		return false;
	}
	m_BitmapHeader = (BITMAP_HEADER*)malloc(headerSize);
	if( m_BitmapHeader == NULL ) {
		return false;
	}
	// Read the rest of the header, minus the size field.
	unsigned int headerReadSize = SafeUSub(headerSize, 4U);
	if( m_Source->read((char*)m_BitmapHeader + 4U, headerReadSize) != headerReadSize ) {
		return false;
	}
	m_BitmapHeader->HeaderSize = headerSize;

	// Signed -> Unsigned conversion, don't let it underflow.
	m_Width = m_BitmapHeader->Width >= 0 ? m_BitmapHeader->Width : 0;
	m_Height = m_BitmapHeader->Height >= 0 ? m_BitmapHeader->Height : 0;
	m_NativeColorModel = kColorModel_RGBX;

	return true;
}

bool ImageReaderBMP::readImage(Image* dest)
{
	if( !supportsOutputColorModel(dest->getColorModel()) ) {
		return false;
	}

	ImageRGBA* destImage = dest->asRGBA();

	// Security checks - make sure we don't exceed the dest capacity, and that nothing has somehow changed since we read the header.
	SECURE_ASSERT(destImage->getWidth() == m_Width && destImage->getHeight() == m_Height);
	unsigned int destPitch = 0;
	uint8_t* destBuffer = destImage->lockRect(m_Width, m_Height, destPitch);
	unsigned int destCapacity = destImage->getImageSize();
	SECURE_ASSERT(SafeUMul(m_Width, 4U) <= destPitch);
	SECURE_ASSERT(destBuffer && destPitch);

	/* Load Color Table */
	unsigned int ColorTableSize = 0;
	if (m_BitmapHeader->BitCount == 1) {
		ColorTableSize = 2;
	} else if (m_BitmapHeader->BitCount == 4) {
		ColorTableSize = 16;
	} else if (m_BitmapHeader->BitCount == 8) {
		ColorTableSize = 256;
	}

	BGRA* ColorTable = NULL;
	if( ColorTableSize > 0 ) {
		ColorTable = new BGRA[ColorTableSize];
		if( ColorTable == NULL ) {
			return false;
		}
		if( m_BitmapHeader->ClrUsed == 0 ) {
			m_BitmapHeader->ClrUsed = ColorTableSize;
		}
        if (m_BitmapHeader->ClrUsed < ColorTableSize) {
            memset(ColorTable + m_BitmapHeader->ClrUsed, 0, sizeof(BGRA) * (ColorTableSize - m_BitmapHeader->ClrUsed));
        }
		if( m_BitmapHeader->ClrUsed > ColorTableSize || m_Source->read((char*)ColorTable, SafeUMul((unsigned int)sizeof(BGRA), m_BitmapHeader->ClrUsed)) != SafeUMul((unsigned int)sizeof(BGRA), m_BitmapHeader->ClrUsed) ) {
			delete[] ColorTable;
			return false;
		}
	}

	unsigned int LineWidth = align(m_Width * m_BitmapHeader->BitCount, 32) / 8;
	uint8_t* Line = new uint8_t[LineWidth];
	if( Line == NULL ) {
		delete [] ColorTable;
		return false;
	}

	// Read and discard everything up to the image data, can't seek.
	if( m_BitmapFileHeader.BitsOffset > m_Source->tell() ) {
		uint64_t remainingBytes = SafeUSub((uint64_t)m_BitmapFileHeader.BitsOffset, m_Source->tell());
		uint8_t discardBuffer[1024];
		while( remainingBytes > 0 ) {
			uint64_t bytesToRead = remainingBytes < 1024 ? remainingBytes : 1024;
			uint64_t bytesRead = m_Source->read(discardBuffer, bytesToRead);
			if( bytesRead == 0 ) {
				return false;
			}
			remainingBytes = SafeUSub(remainingBytes, bytesRead);
		}
	} else if( m_BitmapFileHeader.BitsOffset < m_Source->tell() ) {
		// Malformed BMP.
		return false;
	}

	bool result = true;

	if (m_BitmapHeader->Compression == 0) {
		for (unsigned int i = 0; i < m_Height; i++) {
			if( m_Source->read((char*) Line, LineWidth) != LineWidth ) {
				result = false;
				goto failedRead;
			}
			uint8_t* LinePtr = Line;
			SECURE_ASSERT(LinePtr < Line + LineWidth);
			uint8_t* DestPtr = destBuffer + destPitch * (m_Height - i - 1);
			SECURE_ASSERT(DestPtr >= destBuffer);

			for (unsigned int j = 0; j < m_Width; j++) {
				int OutIndex = j * 4;
				if (m_BitmapHeader->BitCount == 1) {
					unsigned int Color = *((uint8_t*) LinePtr);
					for (int k = 0; k < 8; k++) {
						DestPtr[OutIndex + 0] = ColorTable[Color & 0x80 ? 1 : 0].Red;
						DestPtr[OutIndex + 1] = ColorTable[Color & 0x80 ? 1 : 0].Green;
						DestPtr[OutIndex + 2] = ColorTable[Color & 0x80 ? 1 : 0].Blue;
						DestPtr[OutIndex + 3] = ColorTable[Color & 0x80 ? 1 : 0].Alpha;
						OutIndex += 4;
						if(OutIndex > destPitch - 4) { // handle widths that are not multiples of 8
							break;
						}
						Color <<= 1;
					}
					LinePtr++;
					j += 7;
				} else if (m_BitmapHeader->BitCount == 4) {
					unsigned int Color = *((uint8_t*) LinePtr);
					DestPtr[OutIndex + 0] = ColorTable[(Color >> 4) & 0x0f].Red;
					DestPtr[OutIndex + 1] = ColorTable[(Color >> 4) & 0x0f].Green;
					DestPtr[OutIndex + 2] = ColorTable[(Color >> 4) & 0x0f].Blue;
					DestPtr[OutIndex + 3] = ColorTable[(Color >> 4) & 0x0f].Alpha;
					j++;
					LinePtr++;
					if (j < m_Width) {
						OutIndex = j * 4;
						DestPtr[OutIndex + 0] = ColorTable[Color & 0x0f].Red;
						DestPtr[OutIndex + 1] = ColorTable[Color & 0x0f].Green;
						DestPtr[OutIndex + 2] = ColorTable[Color & 0x0f].Blue;
						DestPtr[OutIndex + 3] = ColorTable[Color & 0x0f].Alpha;
					}
				} else if (m_BitmapHeader->BitCount == 8) {
					unsigned int Color = *((uint8_t*) LinePtr);
					DestPtr[OutIndex + 0] = ColorTable[Color].Red;
					DestPtr[OutIndex + 1] = ColorTable[Color].Green;
					DestPtr[OutIndex + 2] = ColorTable[Color].Blue;
					DestPtr[OutIndex + 3] = ColorTable[Color].Alpha;
					LinePtr++;
				} else if (m_BitmapHeader->BitCount == 16) {
					uint16_t Color = letoh16(*((uint16_t*) LinePtr));
					DestPtr[OutIndex + 0] = ((Color >> 10) & 0x1f) << 3;
					DestPtr[OutIndex + 1] = ((Color >> 5) & 0x1f) << 3;
					DestPtr[OutIndex + 2] = (Color & 0x1f) << 3;
					DestPtr[OutIndex + 3] = 255;
					LinePtr += 2;
				} else if (m_BitmapHeader->BitCount == 24) {
					DestPtr[OutIndex + 2] = LinePtr[0];
					DestPtr[OutIndex + 1] = LinePtr[1];
					DestPtr[OutIndex + 0] = LinePtr[2];
					DestPtr[OutIndex + 3] = 255;
					LinePtr += 3;
				} else if (m_BitmapHeader->BitCount == 32) {
					DestPtr[OutIndex + 2] = LinePtr[0];
					DestPtr[OutIndex + 1] = LinePtr[1];
					DestPtr[OutIndex + 0] = LinePtr[2];
					DestPtr[OutIndex + 3] = 255;
					LinePtr += 4;
				} else {
					result = false;
					goto failedRead;
				}
			}
		}
	} else if (m_BitmapHeader->Compression == 1) { // RLE 8
        // Just memset the whole buffer, don't want to keep track of which regions actually get written to by the RLE decoder.
        memset(destBuffer, 0, destCapacity);
		uint8_t Count = 0;
		uint8_t ColorIndex = 0;
		int x = 0, y = 0;
		if( ColorTable == NULL || ColorTableSize == 0 ) {
			result = false;
			goto failedRead;
		}
		while( true ) {
			if( m_Source->read((char*) &Count, sizeof(uint8_t)) != sizeof(uint8_t) ) {
				result = false;
				goto failedRead;
			}
			if( m_Source->read((char*) &ColorIndex, sizeof(uint8_t)) != sizeof(uint8_t) ) {
				result = false;
				goto failedRead;
			}
			if( ColorIndex >= ColorTableSize ) {
				result = false;
				goto failedRead;
			}
			if (Count > 0) {
				int OutIndex = x * 4 + (m_Height - y - 1) * destPitch;
				if( OutIndex < 0 || OutIndex + Count * 4 > (int)destCapacity ) {
					result = false;
					goto failedRead;
				}
				for (int k = 0; k < Count; k++) {
					destBuffer[OutIndex + k * 4 + 0] = ColorTable[ColorIndex].Red;
					destBuffer[OutIndex + k * 4 + 1] = ColorTable[ColorIndex].Green;
					destBuffer[OutIndex + k * 4 + 2] = ColorTable[ColorIndex].Blue;
					destBuffer[OutIndex + k * 4 + 3] = ColorTable[ColorIndex].Alpha;
				}
				x += Count;
			} else if (Count == 0) {
				int Flag = ColorIndex;
				if (Flag == 0) {
					x = 0;
					y++;
				} else if (Flag == 1) {
					break;
				} else if (Flag == 2) {
					char rx = 0;
					char ry = 0;
					if( m_Source->read((char*) &rx, sizeof(char)) != (int)sizeof(char) ) {
						result = false;
						goto failedRead;
					}
					if( m_Source->read((char*) &ry, sizeof(char)) != (int)sizeof(char) ) {
						result = false;
						goto failedRead;
					}
					x += rx;
					y += ry;
				} else {
					Count = Flag;
					int OutIndex = x * 4 + (m_Height - y - 1) * destPitch;
					if( OutIndex < 0 || OutIndex + Count * 4 > (int)destCapacity ) {
						result = false;
						goto failedRead;
					}
					for (int k = 0; k < Count; k++) {
						if( m_Source->read((char*) &ColorIndex, sizeof(uint8_t)) != (int)sizeof(uint8_t) ) {
							result = false;
							goto failedRead;
						}
						if( ColorIndex >= ColorTableSize ) {
							result = false;
							goto failedRead;
						}
						destBuffer[OutIndex + k * 4 + 0] = ColorTable[ColorIndex].Red;
						destBuffer[OutIndex + k * 4 + 1] = ColorTable[ColorIndex].Green;
						destBuffer[OutIndex + k * 4 + 2] = ColorTable[ColorIndex].Blue;
						destBuffer[OutIndex + k * 4 + 3] = ColorTable[ColorIndex].Alpha;
					}
					x += Count;
					uint64_t filePos = m_Source->tell();
					if (filePos & 1) {
						uint8_t skip;
						m_Source->read(&skip, 1);
					}
				}
			}
		}
	} else if (m_BitmapHeader->Compression == 2) { // RLE 4
		result = false;
		goto failedRead;
	} else if (m_BitmapHeader->Compression == 3) { // BITFIELDS
		if( m_BitmapHeader->HeaderSize < sizeof(BITMAP_HEADER_EXTENDED) ||( m_BitmapHeader->BitCount != 16 && m_BitmapHeader->BitCount != 32) ) {
			result = false;
			goto failedRead;
		}

		BITMAP_HEADER_EXTENDED* extendedHeader = (BITMAP_HEADER_EXTENDED*)m_BitmapHeader;
		unsigned int BitCountRed = BitCountByMask(extendedHeader->RedMask);
		unsigned int BitCountGreen = BitCountByMask(extendedHeader->GreenMask);
		unsigned int BitCountBlue = BitCountByMask(extendedHeader->BlueMask);
		unsigned int BitCountAlpha = BitCountByMask(extendedHeader->AlphaMask);

		for (unsigned int i = 0; i < m_Height; i++) {
			if( m_Source->read((char*) Line, LineWidth) != LineWidth ) {
				result = false;
				goto failedRead;
			}
			uint8_t* LinePtr = Line;
			uint8_t* DestPtr = destBuffer + destPitch * (m_Height - i - 1);
			for (unsigned int j = 0; j < m_Width; j++) {
				unsigned int Color = 0;
				int OutIndex = j * 4;
				if (m_BitmapHeader->BitCount == 16) {
					Color = *((uint16_t*) LinePtr);
					LinePtr += 2;
				} else if (m_BitmapHeader->BitCount == 32) {
					Color = *((uint32_t*) LinePtr);
					LinePtr += 4;
				} else {
					// Other formats are not valid
				}
				DestPtr[OutIndex + 0] = Convert(ComponentByMask(Color, extendedHeader->RedMask), BitCountRed, 8);
				DestPtr[OutIndex + 1] = Convert(ComponentByMask(Color, extendedHeader->GreenMask), BitCountGreen, 8);
				DestPtr[OutIndex + 2] = Convert(ComponentByMask(Color, extendedHeader->BlueMask), BitCountBlue, 8);
				DestPtr[OutIndex + 3] = Convert(ComponentByMask(Color, extendedHeader->AlphaMask), BitCountAlpha, 8);
			}
		}
	} else {
		result = false;
		goto failedRead;
	}

failedRead:
	delete [] ColorTable;
	delete [] Line;

	destImage->unlockRect();

	return result;
}

EImageFormat ImageReaderBMP::getFormat()
{
	return kImageFormat_BMP;
}

const char* ImageReaderBMP::getFormatName()
{
	return "BMP";
}

unsigned int ImageReaderBMP::getWidth()
{
	return m_Width;
}

unsigned int ImageReaderBMP::getHeight()
{
	return m_Height;
}

EImageColorModel ImageReaderBMP::getNativeColorModel()
{
	return m_NativeColorModel;
}

}
