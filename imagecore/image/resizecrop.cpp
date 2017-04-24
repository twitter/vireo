//
//  resizecrop.cpp
//  ImageTool
//
//  Created by Luke Alonso on 3/12/13.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#include "imagecore/imagecore.h"
#include "imagecore/image/colorspace.h"
#include "imagecore/image/rgba.h"
#include "imagecore/utils/mathutils.h"
#include "imagecore/utils/securemath.h"
#include "imagecore/image/resizecrop.h"
#include "imagecore/image/yuv.h"
#include "imagecore/image/internal/filters.h"

namespace imagecore {

ResizeCropOperation::ResizeCropOperation()
{
	m_FilteredImage[0] = NULL;
	m_FilteredImage[1] = NULL;
	m_CropRegion = NULL;
	m_OutputWidth = 0;
	m_OutputHeight = 0;
	m_OutputMod = 1;
	m_TargetWidth = 0;
	m_TargetHeight = 0;
	m_ResizeMode = kResizeMode_ExactCrop;
	m_CropGravity = kGravityHeuristic;
	m_ResizeQuality = kResizeQuality_High;
	m_OutputColorModel = kColorModel_RGBX;
	m_AllowUpsample = true;
	m_AllowDownsample = true;
	m_BackgroundFillColor = RGBA(255, 255, 255, 0);
}

ResizeCropOperation::~ResizeCropOperation()
{
	for( unsigned int i = 0; i < 2; i++ ) {
		if( m_FilteredImage[i] != NULL ) {
			delete m_FilteredImage[i];
			m_FilteredImage[i] = NULL;
		}
	}
	if( m_CropRegion ) {
		delete m_CropRegion;
		m_CropRegion = NULL;
	}
}

int ResizeCropOperation::performResizeCrop(Image*& resizedImage)
{
	resizedImage = NULL;
	if( m_ImageReader == NULL || m_OutputWidth == 0 || m_OutputHeight == 0 ) {
		return IMAGECORE_INVALID_IMAGE_SIZE;
	}
	int ret = IMAGECORE_SUCCESS;
	if( (ret = readHeader()) != IMAGECORE_SUCCESS ) {
		return ret;
	}
	if( (ret = load()) != IMAGECORE_SUCCESS ) {
		return ret;
	}
	if( (ret = fillBackground()) != IMAGECORE_SUCCESS ) {
		return ret;
	}
	if( (ret = resize()) != IMAGECORE_SUCCESS ) {
		return ret;
	}
	if( (ret = rotateCrop()) != IMAGECORE_SUCCESS ) {
		return ret;
	}
	resizedImage = m_FilteredImage[m_WhichImage];
	return IMAGECORE_SUCCESS;
}

int ResizeCropOperation::performResizeCrop(ImageRGBA*& resizedImage)
{
	Image* image = NULL;
	if( performResizeCrop(image) == IMAGECORE_SUCCESS && image != NULL ) {
		resizedImage = image->asRGBA();
	}
	return IMAGECORE_UNKNOWN_ERROR;
}

static float calcScale(unsigned int orientedWidth, unsigned int orientedHeight, unsigned int desiredWidth, unsigned int desiredHeight, bool fit, bool allowUpsample, bool allowDownsample)
{
	float widthScale = (float)desiredWidth / (float)orientedWidth;
	float heightScale = (float)desiredHeight / (float)orientedHeight;
	float scale = fit ? fminf(widthScale, heightScale) : fmaxf(widthScale, heightScale);
	if( !allowUpsample ) {
		scale = fminf(scale, 1.0f);
	}
	if( !allowDownsample ) {
		scale = fmaxf(scale, 1.0f);
	}
	return scale;
}

static void calcOutputSize(unsigned int orientedWidth, unsigned int orientedHeight, unsigned int desiredWidth, unsigned int desiredHeight, unsigned int& targetWidth, unsigned int& targetHeight, unsigned int& outputWidth, unsigned int& outputHeight, EResizeMode resizeMode, bool allowUpsample, bool allowDownsample, ImageRegion* cropRegion, unsigned int outputMod)
{

	// If we have a crop region, we're attempting to scale the specified region to the output size.
	unsigned sourceWidth = cropRegion != NULL ? cropRegion->width() : orientedWidth;
	unsigned sourceHeight = cropRegion != NULL ? cropRegion->height() : orientedHeight;

	float scale = calcScale(sourceWidth, sourceHeight, desiredWidth, desiredHeight, resizeMode == kResizeMode_AspectFit, allowUpsample, allowDownsample);

	if( cropRegion != NULL ) {
		// Adjust the crop region, since the actual crop occurs after scaling.
		cropRegion->left(scale * (float)cropRegion->left());
		cropRegion->top(scale * (float)cropRegion->top());
		cropRegion->width(scale * (float)cropRegion->width());
		cropRegion->height(scale * (float)cropRegion->height());
	}

	targetWidth = roundf(scale * orientedWidth);
	targetHeight = roundf(scale * orientedHeight);

	if( resizeMode == kResizeMode_ExactCrop ) {
		// The final cropped output should match the desired aspect ratio.
		float croppedScale = calcScale(desiredWidth, desiredHeight, targetWidth, targetHeight, true, false, true);
		outputWidth = roundf(croppedScale * desiredWidth);
		outputHeight = roundf(croppedScale * desiredHeight);
	} else if( resizeMode == kResizeMode_Stretch ) {
		outputWidth = desiredWidth;
		outputHeight = desiredHeight;
		targetWidth = desiredWidth;
		targetHeight = desiredHeight;
	} else {
		outputWidth = targetWidth;
		outputHeight = targetHeight;
	}

	if( outputMod != 1 ) {
		outputWidth -= outputWidth % outputMod;
		outputHeight -= outputHeight % outputMod;
	}
}

void ResizeCropOperation::estimateOutputSize(unsigned int imageWidth, unsigned int imageHeight, unsigned int& outputWidth, unsigned int& outputHeight)
{
	calcOutputSize(imageWidth, imageHeight, m_OutputWidth, m_OutputHeight, m_TargetWidth, m_TargetHeight, outputWidth, outputHeight, m_ResizeMode, m_AllowUpsample, m_AllowDownsample, m_CropRegion, m_OutputMod);
}

int ResizeCropOperation::readHeader()
{
	// Read the image header.
	m_InputWidth = m_ImageReader->getWidth();
	m_InputHeight = m_ImageReader->getHeight();
	m_TargetWidth = 0;
	m_TargetHeight = 0;

	if( !Image::validateSize(m_InputWidth, m_InputHeight) ) {
		fprintf(stderr, "error: bad image dimensions\n");
		return IMAGECORE_INVALID_IMAGE_SIZE;
	}

	m_Orientation = m_ImageReader->getOrientation();

	unsigned int orientedWidth = m_ImageReader->getOrientedWidth();
	unsigned int orientedHeight = m_ImageReader->getOrientedHeight();

	calcOutputSize(orientedWidth, orientedHeight, m_OutputWidth, m_OutputHeight, m_TargetWidth, m_TargetHeight, m_OutputWidth, m_OutputHeight, m_ResizeMode, m_AllowUpsample, m_AllowDownsample, m_CropRegion, m_OutputMod);

	if( m_Orientation == kImageOrientation_Left || m_Orientation == kImageOrientation_Right ) {
		// Flip it back, we work on the image in the file orientation.
		swap(m_TargetWidth, m_TargetHeight);
	}

	// Allow conversion between RGBA <-> RGBX, otherwise respect the output color model specified.
	if( Image::colorModelIsRGBA(m_OutputColorModel) && Image::colorModelIsRGBA(m_ImageReader->getNativeColorModel()) ) {
		m_OutputColorModel = m_ImageReader->getNativeColorModel();
	}

	return IMAGECORE_SUCCESS;
}

int ResizeCropOperation::load()
{
	unsigned int reducedWidth;
	unsigned int reducedHeight;
	m_ImageReader->computeReadDimensions(m_TargetWidth, m_TargetHeight, reducedWidth, reducedHeight);

	// Prepare the work buffers.
	unsigned int padAmount = max(ImageRGBA::getDownsampleFilterKernelSize(m_ResizeQuality), ImageRGBA::getUpsampleFilterKernelSize(m_ResizeQuality));
	unsigned int bufferWidth = max(reducedWidth, m_TargetWidth);
	unsigned int bufferHeight = max(reducedHeight, m_TargetHeight);
    unsigned int alignment = 16;
	unsigned int padSize = max(padAmount, Image::colorModelIsYUV(m_OutputColorModel) ? 16U : 4U);
    // Add an extra (alignment) rows to height, because we might rotate the image later, and need to have enough room on that axis for the row alignment.
    unsigned int extraRows = m_ImageReader->getOrientedHeight() != m_ImageReader->getHeight() ? alignment * 2U : 0;
	m_FilteredImage[0] = Image::create(m_OutputColorModel, bufferWidth, bufferHeight + extraRows, padSize, alignment);
	if( m_FilteredImage[0] == NULL ) {
		return IMAGECORE_OUT_OF_MEMORY;
	}
    // Since the second work buffer will be used to hold the resampled output, it doesn't need to be as large as the first.
	m_FilteredImage[1] = Image::create(m_OutputColorModel, max(m_TargetWidth, (bufferWidth + 1) / 2), max(m_TargetHeight, (bufferHeight + 1) / 2) + extraRows, padSize, alignment);
	if( m_FilteredImage[1] == NULL ) {
		return IMAGECORE_OUT_OF_MEMORY;
	}
	m_WhichImage = 0;

	START_CLOCK(decompress);
	m_FilteredImage[m_WhichImage]->setDimensions(reducedWidth, reducedHeight);
	bool result = m_ImageReader->readImage(m_FilteredImage[m_WhichImage]);
	if (!result) {
		fprintf(stderr, "error: unable to read source image\n");
	}
	END_CLOCK(decompress);
	return (result ? IMAGECORE_SUCCESS : IMAGECORE_READ_ERROR);
}

int ResizeCropOperation::fillBackground()
{
	if( m_FilteredImage[m_WhichImage]->getColorModel() == kColorModel_RGBA && m_BackgroundFillColor.a == 255 ) {
		ImageRGBA* image = (ImageRGBA*)m_FilteredImage[m_WhichImage];
		const float3 fillColor = ColorSpace::byteToFloat(RGBA(m_BackgroundFillColor.r,
															  m_BackgroundFillColor.g,
															  m_BackgroundFillColor.b));
		const float3 linearFillColor = ColorSpace::srgbToLinear(fillColor);
		unsigned int framePitch;
		unsigned int width = image->getWidth();
		unsigned int height = image->getHeight();
		uint8_t* buffer = image->lockRect(width, height, framePitch);
		for( unsigned int y = 0; y < height; y++ ) {
			for( unsigned int x = 0; x < width; x++ ) {
				RGBA* rgba = (RGBA*)(&buffer[y * framePitch + x * 4]);
				if( rgba->a == 0 ) {
					*rgba = ColorSpace::floatToByte(fillColor);
				} else if( rgba->a < 255 ) {
					float a = rgba->a / 255.0f;
					const float3 currentColor = ColorSpace::srgbToLinear(ColorSpace::byteToFloat(*rgba));
					*rgba = ColorSpace::floatToByte(ColorSpace::linearToSrgb(float3(a) * currentColor + float3(1-a) * linearFillColor));
				}
			}
		}
		image->unlockRect();
	}
	return IMAGECORE_SUCCESS;
}

int ResizeCropOperation::resize()
{
	Image* inImage = m_FilteredImage[m_WhichImage];
	Image* outImage = m_FilteredImage[m_WhichImage ^ 1];

	// Do a fast iterative 2x2 reduce, equivalent in quality to the 'free' DCT downsampling done by the JPEG decoder,
	// until the image is in the right range for the filter step. This done for input formats other than JPEG.
	while( inImage->getWidth() / 2 >= m_TargetWidth && inImage->getHeight() / 2 >= m_TargetHeight ) {
		START_CLOCK(reduce);
		inImage->reduceHalf(outImage);
		END_CLOCK(reduce);
		m_WhichImage ^= 1;
		inImage = m_FilteredImage[m_WhichImage];
		outImage = m_FilteredImage[m_WhichImage ^ 1];
	}

	// If the reduce didn't happen to get us to the right size, do a final high-quality filter step.
	if( inImage->getWidth() != m_TargetWidth || inImage->getHeight() != m_TargetHeight ) {
		START_CLOCK(filter);
		outImage->setDimensions(m_TargetWidth, m_TargetHeight);
		if( !inImage->resize(outImage, m_ResizeQuality) ) {
			return IMAGECORE_OUT_OF_MEMORY;
		}
		END_CLOCK(filter);
		m_WhichImage ^= 1;
	}

	return IMAGECORE_SUCCESS;
}

int ResizeCropOperation::rotateCrop()
{
	// Apply the EXIF rotation.
	if( m_Orientation == kImageOrientation_Down || m_Orientation == kImageOrientation_Left || m_Orientation == kImageOrientation_Right ) {
		START_CLOCK(orient);
		switch( m_Orientation ) {
			case kImageOrientation_Down:
				m_FilteredImage[m_WhichImage]->rotate(m_FilteredImage[m_WhichImage ^ 1], kImageOrientation_Up);
				break;
			case kImageOrientation_Left:
				m_FilteredImage[m_WhichImage]->rotate(m_FilteredImage[m_WhichImage ^ 1], kImageOrientation_Right);
				break;
			case kImageOrientation_Right:
				m_FilteredImage[m_WhichImage]->rotate(m_FilteredImage[m_WhichImage ^ 1], kImageOrientation_Left);
				break;
		}
		m_WhichImage ^= 1;
		END_CLOCK(orient);
	}
	if( m_CropRegion != NULL ) {
		m_FilteredImage[m_WhichImage]->crop(*m_CropRegion);
	}
	if( m_ResizeMode == kResizeMode_ExactCrop ) {
		ImageRegion* bound = ImageRegion::fromGravity(
													  m_FilteredImage[m_WhichImage]->getWidth(),
													  m_FilteredImage[m_WhichImage]->getHeight(),
													  m_OutputWidth,
													  m_OutputHeight,
													  m_CropGravity);
		ASSERT(bound != NULL);

		m_FilteredImage[m_WhichImage]->crop(*bound);

		delete bound;
	}
	return IMAGECORE_SUCCESS;
}

}
