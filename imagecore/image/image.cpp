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

#include "imagecore/imagecore.h"
#include "imagecore/utils/mathutils.h"
#include "imagecore/utils/securemath.h"
#include "imagecore/image/image.h"
#include "imagecore/image/rgba.h"
#include "imagecore/image/grayscale.h"
#include "imagecore/image/yuv.h"
#include "internal/filters.h"

namespace imagecore {

ImageRegion* ImageRegion::fromString(const char* input)
{
	unsigned int rwidth = 0;
	unsigned int rheight = 0;
	unsigned int rleft = 0;
	unsigned int rtop = 0;

	if( sscanf(input, "%ux%uT%u", &rwidth, &rheight, &rtop) == 3 ) {
		return new ImageRegion(rwidth, rheight, 0, rtop);
	} else if( sscanf(input, "%ux%uL%uT%u", &rwidth, &rheight, &rleft, &rtop) >= 2 ) {
		return new ImageRegion(rwidth, rheight, rleft, rtop);
	}

	// the input is invalid at this point
	return NULL;
}

ImageRegion* ImageRegion::fromGravity(
	unsigned int width,
	unsigned int height,
	unsigned int targetWidth,
	unsigned int targetHeight,
	ECropGravity gravity)
{
	unsigned int rwidth = targetWidth;
	unsigned int rheight = targetHeight;
	unsigned int rleft = 0;
	unsigned int rtop = 0;
	ECropGravity computed_gravity = gravity;

	if( gravity == kGravityHeuristic ) {
		if( height > width ) {
			computed_gravity = kGravityTop;
		} else {
			computed_gravity = kGravityCenter;
		}
	}

	switch( computed_gravity ) {
		case kGravityCenter:
			rleft = (width - rwidth) / 2;
			rtop = (height - rheight) / 2;
			break;
		case kGravityRight:
			rleft = (width - rwidth);
		case kGravityLeft:
			rtop = (height - rheight) / 2;
			break;
		case kGravityBottom:
			rtop = (height - rheight);
		case kGravityTop:
			rleft = (width - rwidth) / 2;
			break;
		default: break;
	}

	// prevent invalid cropping
	if( width < targetWidth ) {
		rleft = 0;
		rwidth = width;
	}
	if( height < targetHeight ) {
		rtop = 0;
		rheight = height;
	}

	return new ImageRegion(rwidth, rheight, rleft, rtop);
}

#define IMAGEPLANE(t) template <uint32_t Channels> t ImagePlane<Channels>

// These capacity calculations are critical to the safety/security of the program - proceed with extreme caution.
IMAGEPLANE(unsigned int)::paddingOffset(unsigned int pitch, unsigned int pad_amount)
{
	// Return a pointer to the beginning of the real image data, skipping the left/top padding.
	return SafeUAdd(SafeUMul(pitch, pad_amount), SafeUMul(pad_amount, Channels));
}

IMAGEPLANE(unsigned int)::paddedPitch(unsigned int width, unsigned int pad_amount, unsigned int alignment)
{
	// We like being 16 byte aligned for SSE, so apply the padding (twice, once each for left/right), then align to 16.
	return align(SafeUMul(SafeUAdd(width, SafeUMul(pad_amount, 2U)), Channels), alignment);
}

IMAGEPLANE(unsigned int)::totalImageSize(unsigned int width, unsigned int height, unsigned int padAmount, unsigned int alignment)
{
	unsigned int pitch = paddedPitch(width, padAmount, alignment);
	SECURE_ASSERT(pitch >= SafeUMul(width, Channels));
	// The pitch is already padded, so add another 2*pad scanlines of padding to height.
	return SafeUMul(pitch, SafeUAdd(height, SafeUMul(padAmount, 2U)));
}

IMAGEPLANE()::ImagePlane(uint8_t* buffer, unsigned int capacity, bool ownsBuffer)
{
	m_Buffer = buffer;
	m_Capacity = capacity;
	m_Width = 0;
	m_Height = 0;
	m_Pitch = 0;
	m_Padding = 0;
	m_OffsetX = 0;
	m_OffsetY = 0;
	m_Alignment = 1;
	m_PadRegionDirty = kEdge_All;
	m_OwnsBuffer = ownsBuffer;
}

IMAGEPLANE(ImagePlane<Channels>*)::create(uint8_t* buffer, unsigned int capacity)
{
	return new ImagePlane<Channels>(buffer, capacity, false);
}

IMAGEPLANE(ImagePlane<Channels>*)::create(unsigned int width, unsigned int height)
{
	return ImagePlane<Channels>::create(width, height, 0, 1);
}

IMAGEPLANE(ImagePlane<Channels>*)::create(unsigned int width, unsigned int height, unsigned int padding, unsigned int alignment)
{
	unsigned int totalSize = totalImageSize(width, height, padding, alignment);
	uint8_t* imageBuffer = (uint8_t*)memalign(max(16U, alignment), totalSize);
	if( imageBuffer == NULL ) {
		return NULL;
	}
	ImagePlane<Channels>* image = new ImagePlane<Channels>(imageBuffer, totalSize, true);
	if( image == NULL ) {
		free(imageBuffer);
		return NULL;
	}
	image->setDimensions(width, height, padding, alignment);
	return image;
}

IMAGEPLANE()::~ImagePlane<Channels>()
{
	if( m_OwnsBuffer && m_Buffer != NULL ) {
		free(m_Buffer);
		m_Buffer = NULL;
	}
}

IMAGEPLANE(const uint8_t*)::getBytes()
{
	unsigned int p;
	return lockRect(m_Width, m_Height, p);
}

IMAGEPLANE(uint8_t*)::lockRect(unsigned int width, unsigned int height, unsigned int& pitch)
{
	return lockRect(0, 0, width, height, pitch);
}

IMAGEPLANE(uint8_t*)::lockRect(unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned int& pitch)
{
	SECURE_ASSERT(height != 0 && width != 0);
	pitch = m_Pitch;
	unsigned int writeX = SafeUAdd(x, SafeUAdd(m_OffsetX, m_Padding));
	unsigned int writeY = SafeUAdd(y, SafeUAdd(m_OffsetY, m_Padding));
	unsigned int headOffset = SafeUAdd(SafeUMul(writeY, m_Pitch), SafeUMul(writeX, Channels));
	unsigned int tailOffset = SafeUAdd(SafeUMul(m_Padding, m_Pitch), SafeUMul(m_Padding, Channels));
	unsigned int remainingBytes = SafeUSub(m_Capacity, SafeUAdd(headOffset, tailOffset));
	unsigned int writeBytes = SafeUSub(SafeUMul(m_Pitch, height), SafeUAdd(SafeUMul(m_Padding, Channels), SafeUMul(writeX, Channels)));
	SECURE_ASSERT(writeBytes <= remainingBytes);
	SECURE_ASSERT(SafeUMul(SafeUAdd(width, SafeUMul(2, m_Padding)), Channels) <= m_Pitch);
	SECURE_ASSERT(Image::validateSize(width, height));
	m_PadRegionDirty = kEdge_All;
	return m_Buffer + headOffset;
}

IMAGEPLANE(void)::unlockRect()
{

}

IMAGEPLANE(void)::setDimensions(unsigned int width, unsigned int height)
{
	m_Width = width;
	m_Height = height;
	m_Pitch = paddedPitch(m_Width, m_Padding, m_Alignment);
	m_PadRegionDirty = kEdge_All;
	SECURE_ASSERT(checkCapacity(m_Width, m_Height));
}

IMAGEPLANE(void)::setDimensions(unsigned int width, unsigned int height, unsigned int padding, unsigned int alignment)
{
	m_Width = width;
	m_Height = height;
	m_Padding = padding;
	m_Alignment = alignment;
	m_Pitch = paddedPitch(m_Width, m_Padding, m_Alignment);
	m_PadRegionDirty = kEdge_All;
	SECURE_ASSERT(checkCapacity(m_Width, m_Height));
}

IMAGEPLANE(void)::setPadding(unsigned int padding)
{
	m_Padding = padding;
	m_Pitch = paddedPitch(m_Width, m_Padding, m_Alignment);
	m_PadRegionDirty = kEdge_All;
	SECURE_ASSERT(checkCapacity(m_Width, m_Height));
}

IMAGEPLANE(bool)::checkCapacity(unsigned int width, unsigned int height)
{
	unsigned int requestedSize = totalImageSize(SafeUAdd(width, m_OffsetX), SafeUAdd(height, m_OffsetY), m_Padding, m_Alignment);
	unsigned int requestedPitch = paddedPitch(width, m_Padding, m_Alignment);
	SECURE_ASSERT(requestedPitch >= SafeUMul(width, Channels));
	return requestedSize <= m_Capacity;
}

IMAGEPLANE(unsigned int)::getImageSize()
{
	// The total user-available area inside the buffer (doesn't count the offset and padding).
	// When you ask for image->getBytes(), this is how much you're allowed to write.
	return SafeUMul(m_Pitch, m_Height);
}

IMAGEPLANE(void)::setOffset(unsigned int offsetX, unsigned int offsetY)
{
	m_OffsetX = offsetX;
	m_OffsetY = offsetY;
	m_PadRegionDirty = kEdge_All;
	SECURE_ASSERT(checkCapacity(m_Width, m_Height));
}

IMAGEPLANE(bool)::crop(const ImageRegion& boundingBox)
{
	if( m_Width >= boundingBox.width() && m_Height >= boundingBox.height() ) {
		SECURE_ASSERT(SafeUAdd(boundingBox.width(), boundingBox.left()) <= m_Width);
		SECURE_ASSERT(SafeUAdd(boundingBox.height(), boundingBox.top()) <= m_Height);
		m_OffsetX += boundingBox.left();
		m_OffsetY += boundingBox.top();
		m_Width = boundingBox.width();
		m_Height = boundingBox.height();
		m_PadRegionDirty = kEdge_All;
		return true;
	}
	return false;
}

IMAGEPLANE(void)::rotate(ImagePlane<Channels>* dest, EImageOrientation direction)
{
	unsigned int destPitch = 0;
	if( direction == kImageOrientation_Left ) {
		dest->setDimensions(m_Height, m_Width);
		uint8_t* destBuffer = dest->lockRect(m_Height, m_Width, destPitch);
		Filters<ComponentSIMD<Channels>>::rotateLeft(this->getBytes(), destBuffer, m_Width, m_Height, this->getPitch(), destPitch, dest->getImageSize());
	} else if( direction == kImageOrientation_Right ) {
		dest->setDimensions(m_Height, m_Width);
		uint8_t* destBuffer = dest->lockRect(m_Height, m_Width, destPitch);
		Filters<ComponentSIMD<Channels>>::rotateRight(this->getBytes(), destBuffer, m_Width, m_Height, this->getPitch(), destPitch, dest->getImageSize());
	} else if( direction == kImageOrientation_Up || direction == kImageOrientation_Down ) {
		dest->setDimensions(m_Width, m_Height);
		uint8_t* destBuffer = dest->lockRect(m_Width, m_Height, destPitch);
		Filters<ComponentSIMD<Channels>>::rotateUp(this->getBytes(), destBuffer, m_Width, m_Height, this->getPitch(), destPitch, dest->getImageSize());
	}
	dest->unlockRect();
}

IMAGEPLANE(void)::transpose(ImagePlane* dest)
{
	SECURE_ASSERT(m_Width == dest->getHeight());
	SECURE_ASSERT(m_Height == dest->getWidth());
	SECURE_ASSERT((m_Pitch & 3) == 0); // multiple of 4 only
	uint32_t destPitch;
	uint8_t* destBuffer = dest->lockRect(dest->getWidth(), dest->getHeight(), destPitch);
	SECURE_ASSERT((destPitch & 3) == 0); // multiple of 4 only
	Filters<ComponentSIMD<Channels>>::transpose(this->getBytes(), destBuffer, m_Width, m_Height, this->getPitch(), destPitch, dest->getImageSize());
}

IMAGEPLANE(void)::fillPadding(EEdgeMask edgeMask)
{
	if (m_PadRegionDirty != kEdge_None) {
		unsigned int pitch;
		// lockRect will validate that we have enough capacity to write m_Padding*2 bytes beyond m_Width and m_Height.
		typename primType::asType* sample = (typename primType::asType*)lockRect(m_Width, m_Height, pitch);
		int componentPitch = pitch / Channels;
		if( (edgeMask & kEdge_Left) != 0 && (m_PadRegionDirty & kEdge_Left) != 0 ) {
			for( int y = 0; y < m_Height; ++y ) {
				for( int x = -m_Padding; x < 0; ++x ) {
					sample[y * componentPitch + x] = sample[y * componentPitch + 0];
				}
			}
			m_PadRegionDirty &= ~kEdge_Left;
		}
		if( (edgeMask & kEdge_Right) != 0 && (m_PadRegionDirty & kEdge_Right) != 0 ) {
			unsigned int paddedWidth = m_Width + m_Padding;
			for( int y = 0; y < m_Height; ++y ) {
				for( int x = m_Width; x < paddedWidth; ++x ) {
					sample[y * componentPitch + x] = sample[y * componentPitch + (m_Width - 1)];
				}
			}
			m_PadRegionDirty &= ~kEdge_Right;
		}
		if( (edgeMask & kEdge_Top) != 0 && (m_PadRegionDirty & kEdge_Top) != 0 ) {
			unsigned int twicePaddedWidth = m_Width + m_Padding * 2;
			for( int y = -m_Padding; y < 0; ++y ) {
				memcpy(sample + y * componentPitch - m_Padding, sample + 0 * componentPitch - m_Padding, twicePaddedWidth * Channels);
			}
			m_PadRegionDirty &= ~kEdge_Top;
		}
		if( (edgeMask & kEdge_Bottom) != 0 && (m_PadRegionDirty & kEdge_Bottom) != 0 ) {
			unsigned int paddedHeight = m_Height + m_Padding;
			unsigned int twicePaddedWidth = m_Width + m_Padding * 2;
			for( int y = m_Height; y < paddedHeight; ++y ) {
				memcpy(sample + y * componentPitch - m_Padding, sample + (m_Height - 1) * componentPitch - m_Padding, twicePaddedWidth * Channels);
			}
			m_PadRegionDirty &= ~kEdge_Bottom;
		}
	}
}

IMAGEPLANE(void)::reduceHalf(ImagePlane<Channels>* dest)
{
	dest->setDimensions(m_Width / 2, m_Height / 2);
	unsigned int destPitch = 0;
	uint8_t* destBuffer = dest->lockRect(m_Width / 2, m_Height / 2, destPitch);
	Filters<ComponentSIMD<Channels>>::reduceHalf(this->getBytes(), destBuffer, m_Width, m_Height, m_Pitch, destPitch, dest->getImageSize());
	dest->unlockRect();
}

IMAGEPLANE(bool)::resize(ImagePlane<Channels>* dest, EResizeQuality quality)
{
	if( dest->getWidth() == m_Width && dest->getHeight() == m_Height ) {
		copy(dest);
		return true;
	} else if( dest->getWidth() > m_Width || dest->getHeight() > m_Height ) {
		ImagePlane<Channels>* workBuffer = NULL;
		ImagePlane<Channels>* sourceImage = this;
		if( m_Padding < 4 ) {
			workBuffer = ImagePlane<Channels>::create(m_Width, m_Height, 4, 16);
			if( workBuffer == NULL ) {
				return false;
			}
			copy(workBuffer);
			sourceImage = workBuffer;
		}
		// Upsample.
		EFilterType kernelType = Image::getUpsampleFilterKernelType(quality);
		FilterKernelFixed filterKernelX(kernelType, this->getWidth(), dest->getWidth());
		FilterKernelFixed filterKernelY(kernelType, this->getHeight(), dest->getHeight());
		bool success = sourceImage->upsampleFilter4x4(dest, &filterKernelX, &filterKernelY);
		delete workBuffer;
		return success;
	} else {
		// Downsample.
		ImagePlane<Channels>* whichImage = this;
		EFilterType kernelType = Image::getDownsampleFilterKernelType(quality);
		unsigned int kernelSize = Image::getDownsampleFilterKernelSize(quality);

		unsigned int destWidth = dest->getWidth();
		unsigned int destHeight = dest->getHeight();
		ImagePlane<Channels>* workBuffer[2] = { NULL, NULL };
		bool unpadded = false;
		bool doneDownsampling = false;
		if(Filters<ComponentSIMD<Channels>>::supportsUnpadded(kernelSize)) {
			unpadded = Filters<ComponentSIMD<Channels>>::fasterUnpadded(kernelSize);
		}
		// Do we need to do iterative 2x2 reductions first?
		if (whichImage->getWidth() / 2 >= destWidth && whichImage->getHeight() / 2 >= destHeight) {
			workBuffer[0] = ImagePlane<Channels>::create(whichImage->getWidth() / 2, whichImage->getHeight() / 2, kernelSize, 16);
			workBuffer[1] = ImagePlane<Channels>::create(whichImage->getWidth() / 2, whichImage->getHeight() / 2, kernelSize, 16);

			if (workBuffer[0] == NULL || workBuffer[1] == NULL) {
				delete workBuffer[0];
				delete workBuffer[1];
				return false;
			}

			unsigned int whichWorkBuffer = 0;
			while (!doneDownsampling && whichImage->getWidth() / 2 >= destWidth && whichImage->getHeight() / 2 >= destHeight) {
				if ((whichImage->getWidth() / 2 == destWidth) && (whichImage->getHeight() / 2 == destHeight)) {
					whichImage->reduceHalf(dest);
					doneDownsampling = true; // for the case where reducehalf gets us the correct size
				} else {
					whichImage->reduceHalf(workBuffer[whichWorkBuffer]);
					whichImage = workBuffer[whichWorkBuffer];
					whichWorkBuffer ^= 1;
				}
			}
		} else if (m_Padding < kernelSize) {
			if(Filters<ComponentSIMD<Channels>>::supportsUnpadded(kernelSize)) {
				unpadded = true; // use unpadded code path to avoid copying for all bit depths
			} else {
				// TODO: this is wasteful, find another solution
				workBuffer[0] = ImagePlane<Channels>::create(whichImage->getWidth(), whichImage->getHeight(), kernelSize, 16);
				if (workBuffer[0] == NULL) {
					return false;
				}
				copy(workBuffer[0]);
				whichImage = workBuffer[0];
			}
		}

		bool success = false;
		if (doneDownsampling) {
			success = true;
		} else {
			FilterKernelAdaptive filterKernelX(kernelType, kernelSize, whichImage->getWidth(), destWidth);
			FilterKernelAdaptive filterKernelY(kernelType, kernelSize, whichImage->getHeight(), destHeight);
			success = whichImage->downsampleFilter(dest, &filterKernelX, &filterKernelY, unpadded);
		}
		// Only allocated if iterative reduction was performed.
		delete workBuffer[0];
		delete workBuffer[1];
		return success;
	}
}

IMAGEPLANE(bool)::downsampleFilter(ImagePlane<Channels> *dest, const FilterKernelAdaptive *filterKernelX, const FilterKernelAdaptive *filterKernelY, bool unpadded)
{
	if (filterKernelX->getKernelSize() == 2) {
		// Special low quality but fast 2x2 bilinear filter, used for on device video transcoding
		return downsampleFilter2x2(dest, filterKernelX, filterKernelY);
	} else if (filterKernelX->getKernelSize() == 4) {
		// Special 4x4 non-seperable filter.
		return downsampleFilter4x4(dest, filterKernelX, filterKernelY);
	} else {
		return downsampleFilterSeperable(dest, filterKernelX, filterKernelY, unpadded);
	}
}

IMAGEPLANE(bool)::downsampleFilterSeperable(ImagePlane<Channels>* dest, const FilterKernelAdaptive* filterKernelX, const FilterKernelAdaptive* filterKernelY, bool unpadded)
{
	unsigned int padSize = max(filterKernelX->getKernelSize(), filterKernelY->getKernelSize());
	SECURE_ASSERT((m_Padding >= padSize) || (unpadded));
	ImagePlane<Channels>* temp = ImagePlane<Channels>::create(m_Height, dest->getWidth(), padSize, 16U);
	if( temp == NULL ) {
		return false;
	}
	unsigned int tempPitch = 0;
	if( !unpadded ) {
		fillPadding();
	}
	uint8_t* tempBuffer = temp->lockRect(temp->getWidth(), temp->getHeight(), tempPitch);
	Filters<ComponentSIMD<Channels>>::adaptiveSeperable(filterKernelX, this->getBytes(), m_Width, m_Height, m_Pitch,
							   tempBuffer, temp->getHeight(), temp->getWidth(), tempPitch, temp->getImageSize(), unpadded);
	temp->unlockRect();
	if( !unpadded ) {
		temp->fillPadding();
	}
	unsigned int destPitch = 0;
	uint8_t* destBuffer = dest->lockRect(dest->getWidth(), dest->getHeight(), destPitch);
	Filters<ComponentSIMD<Channels>>::adaptiveSeperable(filterKernelY, temp->getBytes(), temp->getWidth(), temp->getHeight(), temp->getPitch(),
							   destBuffer, dest->getHeight(), dest->getWidth(), destPitch, dest->getImageSize(), unpadded);
	dest->unlockRect();
	delete temp;
	return true;
}


IMAGEPLANE(bool)::downsampleFilter2x2(ImagePlane<Channels>* dest, const FilterKernelAdaptive* filterKernelX, const FilterKernelAdaptive* filterKernelY)
{
	unsigned int destPitch = 0;
	fillPadding();
	ImagePlane<Channels>* transposedDest = ImagePlane<Channels>::create(dest->getHeight(), dest->getWidth(), 0, 4);
	uint8_t* destBuffer = transposedDest->lockRect(dest->getHeight(), dest->getWidth(), destPitch);
	Filters<ComponentSIMD<Channels>>::adaptiveSeparable2x2(filterKernelX, filterKernelY, this->getBytes(), m_Width, m_Height, m_Pitch,
												  destBuffer, dest->getWidth(), dest->getHeight(), destPitch, dest->getImageSize());
	transposedDest->unlockRect();
	transposedDest->transpose(dest);
	delete transposedDest;
	return true;
}

IMAGEPLANE(bool)::downsampleFilter4x4(ImagePlane<Channels>* dest, const FilterKernelAdaptive* filterKernelX, const FilterKernelAdaptive* filterKernelY)
{
	SECURE_ASSERT(m_Padding >= 4);
	unsigned int destPitch = 0;
	fillPadding();
	uint8_t* destBuffer = dest->lockRect(dest->getWidth(), dest->getHeight(), destPitch);
	Filters<ComponentSIMD<Channels>>::adaptive4x4(filterKernelX, filterKernelY, this->getBytes(), m_Width, m_Height, m_Pitch,
						 destBuffer, dest->getWidth(), dest->getHeight(), destPitch, dest->getImageSize());
	dest->unlockRect();
	return true;
}

IMAGEPLANE(bool)::upsampleFilter4x4(ImagePlane<Channels>* dest, const FilterKernelFixed* filterKernelX, const FilterKernelFixed* filterKernelY)
{
	SECURE_ASSERT(m_Padding >= 4);
	unsigned int destPitch = 0;
	fillPadding();
	uint8_t* destBuffer = dest->lockRect(dest->getWidth(), dest->getHeight(), destPitch);
	Filters<ComponentSIMD<Channels>>::fixed4x4(filterKernelX, filterKernelY, this->getBytes(), m_Width, m_Height, m_Pitch,
					  destBuffer, dest->getWidth(), dest->getHeight(), destPitch, dest->getImageSize());
	dest->unlockRect();
	return true;
}

IMAGEPLANE(void)::clearRect(unsigned int sx, unsigned int sy, unsigned int w, unsigned int h, typename primType::asType component)
{
	unsigned int destPitch = 0;
	uint8_t* destBuffer = lockRect(sx, sy, w, h, destPitch);
	for( unsigned int y = 0; y < h; ++y ) {
		for( unsigned int x = 0; x < w; ++x ) {
			unsigned int outIndex = y * destPitch + x * Channels;
			*(typename primType::asType*)&destBuffer[outIndex + 0] = component;
		}
	}
	unlockRect();
}

IMAGEPLANE(void)::clear(typename primType::asType component)
{
	clearRect(0, 0, m_Width, m_Height, component);
}

IMAGEPLANE(void)::copyRect(ImagePlane<Channels>* dest, unsigned int sourceX, unsigned int sourceY, unsigned int destX, unsigned int destY, unsigned int width, unsigned int height)
{
	if( this == dest && sourceX == destX && sourceY == destY && width == m_Width && height == m_Height ) {
		return;
	}
	unsigned int sourcePitch = 0;
	uint8_t* sourceBuffer = lockRect(sourceX, sourceY, width, height, sourcePitch);
	unsigned int destPitch = 0;
	uint8_t* destBuffer = dest->lockRect(destX, destY, width, height, destPitch);
	for( unsigned int y = 0; y < height; ++y ) {
		memcpy(destBuffer + y * destPitch, sourceBuffer + y * sourcePitch, width * Channels);
	}
}

IMAGEPLANE(void)::copy(ImagePlane<Channels>* dest)
{
	copyRect(dest, 0, 0, 0, 0, m_Width, m_Height);
}

template class ImagePlane<1>;
template class ImagePlane<2>;
template class ImagePlane<4>;

Image* Image::create(EImageColorModel colorSpace, unsigned int width, unsigned int height)
{
	if( colorModelIsRGBA(colorSpace) ) {
		return ImageRGBA::create(width, height, colorSpace == kColorModel_RGBA);
	} else if( colorModelIsGrayscale(colorSpace) ) {
		return ImageGrayscale::create(width, height);
	} else if( colorModelIsYUV(colorSpace) ) {
		return ImageYUV::create(width, height);
	}
	return NULL;
}

Image* Image::create(EImageColorModel colorSpace, unsigned int width, unsigned int height, unsigned int padding, unsigned int alignment)
{
	if( colorModelIsRGBA(colorSpace) ) {
		return ImageRGBA::create(width, height, padding, alignment, colorSpace == kColorModel_RGBA);
	} else if( colorModelIsGrayscale(colorSpace) ) {
		return ImageGrayscale::create(width, height, padding, alignment);
	} else if( colorModelIsYUV(colorSpace) ) {
		return ImageYUV::create(width, height, padding, alignment);
	}
	return NULL;
}

unsigned int Image::getDownsampleFilterKernelSize(EResizeQuality quality)
{
	if( quality == kResizeQuality_Bilinear ) {
		return 2;
	} else if( quality == kResizeQuality_Low ) {
		return 4;
	} else if( quality == kResizeQuality_Medium ) {
		return 8;
	} else if( quality == kResizeQuality_High || quality == kResizeQuality_HighSharp ) {
		return 12;
	}
	ASSERT(0);
	return 0;
}

EFilterType Image::getDownsampleFilterKernelType(EResizeQuality quality)
{
	if (quality == kResizeQuality_Bilinear) {
		return kFilterType_Linear;
	} else if( quality == kResizeQuality_Low ) {
		return kFilterType_Kaiser;
	} else if( quality == kResizeQuality_Medium || quality == kResizeQuality_High ) {
		return kFilterType_Lanczos;
	} else if( quality == kResizeQuality_HighSharp ) {
		return kFilterType_LanczosSharper;
	}
	ASSERT(0);
	return kFilterType_Lanczos;
}

unsigned int Image::getUpsampleFilterKernelSize(EResizeQuality quality)
{
	return 4;
}

EFilterType Image::getUpsampleFilterKernelType(EResizeQuality quality)
{
	if( quality == kResizeQuality_Low ) {
		return kFilterType_MitchellNetravali;
	} else if( quality == kResizeQuality_Medium ) {
		return kFilterType_MitchellNetravali;
	} else if( quality == kResizeQuality_High ) {
		return kFilterType_Lanczos;
	} else if( quality == kResizeQuality_HighSharp ) {
		return kFilterType_LanczosSharper;
	}
	ASSERT(0);
	return kFilterType_MitchellNetravali;
}

bool Image::validateSize(unsigned int width, unsigned int height)
{
	if( width < 1 || height < 1 ) {
		return false;
	}
	if( width > 16384 || height > 16384 ) {
		return false;
	}
	if( SafeUMul(width, height) > 8192 * 8192 ) {
		return false;
	}
	return true;
}

void Image::clear(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	clearRect(0, 0, getWidth(), getHeight(), r, g, b, a);
}

void Image::copy(Image* dest)
{
	copyRect(dest, 0, 0, 0, 0, getWidth(), getHeight());
}

}
