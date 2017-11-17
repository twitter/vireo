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
#include "imagecore/image/kernel.h"

namespace imagecore {

// translates from number of channels to primitive type
template<uint32_t channels> class toPrimType {
public:
	typedef uint8_t asType;
};

template<> class toPrimType<1> {
public:
	typedef uint8_t asType;
};

template<> class toPrimType<2> {
public:
	typedef uint16_t asType;
};

template<> class toPrimType<4> {
public:
	typedef uint32_t asType;
};

// components, with scalar and simd specializations
template<uint32_t Channels>
struct ComponentBase {
	typedef toPrimType<Channels> primType;
	uint8_t mChannels[Channels];
};

template<uint32_t Channels>
struct ComponentScalar : public ComponentBase<Channels> {
};

template<uint32_t Channels>
struct ComponentSIMD : public ComponentBase<Channels> {
};

enum EResizeQuality
{
	kResizeQuality_Bilinear = 0,
	kResizeQuality_Low,
	kResizeQuality_Medium,
	kResizeQuality_High,
	kResizeQuality_HighSharp,
	kResizeQuality_MAX
};

enum ECropGravity
{
	kGravityHeuristic = 0, // top for portraits, else center
	kGravityCenter,
	kGravityLeft,
	kGravityTop,
	kGravityRight,
	kGravityBottom
};

enum EImageColorModel
{
	kColorModel_RGBA,
	kColorModel_RGBX,
	kColorModel_Grayscale,
	kColorModel_YUV_420
};

enum EImageOrientation
{
	kImageOrientation_Up = 1,
	kImageOrientation_Down = 3,
	kImageOrientation_Left = 6,
	kImageOrientation_Right = 8
};

enum EResolutionUnit
{
	kNone = 1,
	kInches = 2,
	kCm = 3
};

enum EAltitudeRef
{
	kAboveSeaLevel = 0,
	kBelowSeaLevel = 1
};

enum EEdgeMask
{
	kEdge_None	  = 0x00,
	kEdge_Left    = 0x01,
	kEdge_Top     = 0x02,
	kEdge_Right   = 0x04,
	kEdge_Bottom  = 0x08,
	kEdge_All	  = kEdge_Left | kEdge_Top | kEdge_Right | kEdge_Bottom
};

// A bounding box class
class ImageRegion
{
public:
	ImageRegion(unsigned int width, unsigned int height, unsigned int left, unsigned int top)
	{
		m_Left = left;
		m_Top = top;
		m_Width = width;
		m_Height = height;
	}

	// these return NULL if there is an error
	static ImageRegion* fromString(const char* input);
	static ImageRegion* fromGravity(
		unsigned int width,
		unsigned int height,
		unsigned int targetWidth,
		unsigned int targetHeight,
		ECropGravity gravity);

	// accessors
	unsigned int left() const { return m_Left; }
	unsigned int right() const { return m_Left + m_Width; }
	unsigned int top() const { return m_Top; }
	unsigned int bottom() const { return m_Top + m_Height; }
	unsigned int width() const { return m_Width; }
	unsigned int height() const { return m_Height; }

	// mutators
	void left(unsigned int value) { m_Left = value; }
	void top(unsigned int value) { m_Top = value; }
	void width(unsigned int value) { m_Width = value; }
	void height(unsigned int value) { m_Height = value; }

protected:
	unsigned int m_Left;
	unsigned int m_Top;
	unsigned int m_Width;
	unsigned int m_Height;
};

class ImageGrayscale;
class ImageRGBA;
class ImageYUV;
class ImageInterleaved;

class Image
{
public:
	static Image* create(EImageColorModel colorSpace, unsigned int width, unsigned int height);
	static Image* create(EImageColorModel colorSpace, unsigned int width, unsigned int height, unsigned int padding, unsigned int alignment);

	virtual ~Image() {}

	virtual void setDimensions(unsigned int width, unsigned int height) = 0;
	virtual void setDimensions(unsigned int width, unsigned int height, unsigned int padding, unsigned int alignment) = 0;
	virtual void setPadding(unsigned int padding) = 0;

	virtual bool resize(Image* dest, EResizeQuality quality) = 0;
	virtual void reduceHalf(Image* dest) = 0;
	virtual bool crop(const ImageRegion& boundingBox) = 0;
	virtual void rotate(Image* dest, EImageOrientation direction) = 0;
	virtual void fillPadding() = 0;

	virtual void clear(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
	virtual void clearRect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a) = 0;

	virtual void copy(Image* dest);
	virtual void copyRect(Image* dest, unsigned int sourceX, unsigned int sourceY, unsigned int destX, unsigned int destY, unsigned int width, unsigned int height) = 0;

	virtual Image* move() = 0;

	virtual unsigned int getWidth() const = 0;
	virtual unsigned int getHeight() const = 0;
	virtual unsigned int getPadding() = 0;
	virtual EImageColorModel getColorModel() const = 0;

	virtual class ImageRGBA* asRGBA() = 0;
	virtual class ImageGrayscale* asGrayscale() = 0;
	virtual class ImageYUV* asYUV() = 0;
	virtual class ImageYUVSemiplanar* asYUVSemiplanar() = 0;
	virtual class ImageInterleaved* asInterleaved() = 0;

	static bool colorModelIsRGBA(EImageColorModel colorModel)
	{
		return colorModel == kColorModel_RGBA || colorModel == kColorModel_RGBX;
	}

	static bool colorModelIsGrayscale(EImageColorModel colorModel)
	{
		return colorModel == kColorModel_Grayscale;
	}

	static bool colorModelIsInterleaved(EImageColorModel colorModel)
	{
		return colorModelIsRGBA(colorModel) || colorModelIsGrayscale(colorModel);
	}

	static bool colorModelIsYUV(EImageColorModel colorModel)
	{
		return colorModel == kColorModel_YUV_420;
	}

	static unsigned int getDownsampleFilterKernelSize(EResizeQuality quality);
	static EFilterType getDownsampleFilterKernelType(EResizeQuality quality);
	static unsigned int getUpsampleFilterKernelSize(EResizeQuality quality);
	static EFilterType getUpsampleFilterKernelType(EResizeQuality quality);
	static bool validateSize(unsigned int width, unsigned int height);
};

template <uint32_t Channels>
class ImagePlane
{
	typedef toPrimType<Channels> primType;
public:
	static ImagePlane* create(uint8_t* buffer, unsigned int capacity);
	static ImagePlane* create(unsigned int width, unsigned int height);
	static ImagePlane* create(unsigned int width, unsigned int height, unsigned int padding, unsigned int alignment);

	~ImagePlane();

	const uint8_t* getBytes();
	uint8_t* lockRect(unsigned int width, unsigned int height, unsigned int& pitch);
	uint8_t* lockRect(unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned int& pitch);
	void unlockRect();

	void setDimensions(unsigned int width, unsigned int height);
	void setDimensions(unsigned int width, unsigned int height, unsigned int padding, unsigned int alignment);
	void setPadding(unsigned int padding);
	void setOffset(unsigned int offsetX, unsigned int offsetY);

	bool resize(ImagePlane* dest, EResizeQuality quality);
	void reduceHalf(ImagePlane* dest);
	bool downsampleFilter(ImagePlane* dest, const FilterKernelAdaptive* filterKernelX, const FilterKernelAdaptive* filterKernelY, bool unpadded);
	bool crop(const ImageRegion& boundingBox);
	void rotate(ImagePlane* dest, EImageOrientation direction);
	void transpose(ImagePlane* dest);
	void fillPadding(EEdgeMask edgeMask = kEdge_All);

	void clear(typename primType::asType component);
	void clearRect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, typename primType::asType component);
	void copyRect(ImagePlane* dest, unsigned int sourceX, unsigned int sourceY, unsigned int destX, unsigned int destY, unsigned int width, unsigned int height);
	void copy(ImagePlane* dest);

	unsigned int getWidth()
	{
		return m_Width;
	}

	unsigned int getHeight()
	{
		return m_Height;
	}

	unsigned int getPitch()
	{
		return m_Pitch;
	}

	unsigned int getPadding()
	{
		return m_Padding;
	}

	unsigned int getCapacity()
	{
		return m_Capacity;
	}

	unsigned int getAlignment()
	{
		return m_Alignment;
	}

	unsigned int getImageSize();

private:
	ImagePlane(uint8_t* buffer, unsigned int capacity, bool ownsBuffer);
	bool checkCapacity(unsigned int width, unsigned int height);
	bool downsampleFilterSeperable(ImagePlane* dest, const FilterKernelAdaptive* filterKernelX, const FilterKernelAdaptive* filterKernelY, bool unpadded);
	bool downsampleFilter4x4(ImagePlane* dest, const FilterKernelAdaptive* filterKernelX, const FilterKernelAdaptive* filterKernelY);
	bool downsampleFilter2x2(ImagePlane* dest, const FilterKernelAdaptive* filterKernelX, const FilterKernelAdaptive* filterKernelY);
	bool upsampleFilter4x4(ImagePlane* dest, const FilterKernelFixed* filterKernelX, const FilterKernelFixed* filterKernelY);

	static unsigned int paddingOffset(unsigned int pitch, unsigned int pad_amount);
	static unsigned int paddedPitch(unsigned int width, unsigned int pad_amount, unsigned int alignment);
	static unsigned int totalImageSize(unsigned int width, unsigned int height, unsigned int padAmount, unsigned int alignment);

	uint8_t* m_Buffer;
	unsigned int m_Capacity;
	unsigned int m_Width;
	unsigned int m_Height;
	unsigned int m_Pitch;
	unsigned int m_Padding;
	unsigned int m_OffsetX;
	unsigned int m_OffsetY;
	unsigned int m_Alignment;
	unsigned int m_PadRegionDirty;
	bool m_OwnsBuffer;
};

extern template class ImagePlane<1>;
extern template class ImagePlane<2>;
extern template class ImagePlane<4>;

typedef ImagePlane<1> ImagePlaneGrayscale;
typedef ImagePlane<1> ImagePlane8;
typedef ImagePlane<2> ImagePlane16;
typedef ImagePlane<4> ImagePlaneRGBA;

}
