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

#include "imagecore/image/rgba.h"
#include "imagecore/image/tiledresize.h"
#include "imagecore/utils/mathutils.h"

namespace imagecore {

TiledResizeOperation::TiledResizeOperation(ImageReader* imageReader, ImageWriter* imageWriter, unsigned int outputWidth, unsigned int outputHeight)
{
	m_ImageReader = imageReader;
	m_ImageWriter = imageWriter;
	m_OutputWidth = outputWidth;
	m_OutputHeight = outputHeight;
	m_ResizeQuality = kResizeQuality_High;
}

TiledResizeOperation::~TiledResizeOperation()
{

}

FilterKernelAdaptive* createFilterKernel(EResizeQuality quality, unsigned int inSize, unsigned int outSize)
{
	return new FilterKernelAdaptive(ImageRGBA::getDownsampleFilterKernelType(quality), ImageRGBA::getDownsampleFilterKernelSize(quality), inSize, outSize);
}

ImageRGBA* resizeTile(ImageRGBA* source, ImageRGBA* dest1, ImageRGBA* dest2, int inSampleOffset, int outSampleOffset, FilterKernelAdaptive *kernelX, FilterKernelAdaptive *kernelY, bool isExact)
{
	unsigned int destWidth = dest1->getWidth();
	unsigned int destHeight = dest1->getHeight();
	ImageRGBA* inImage = source;
	unsigned int whichOutImage = 0;
	ImageRGBA* outImages[2] = { dest1, dest2 };

	while( inImage->getWidth() / 2 >= destWidth && inImage->getHeight() / 2 >= destHeight ) {
		START_CLOCK(reduce);
		inImage->reduceHalf(outImages[whichOutImage]);
		inSampleOffset /= 2;
		END_CLOCK(reduce);
		inImage = outImages[whichOutImage];
		whichOutImage ^= 1;
	}

	if (!isExact) {
		START_CLOCK(filter);
		outImages[whichOutImage]->setDimensions(destWidth, destHeight);
		kernelY->setSampleOffset(inSampleOffset, outSampleOffset);
		if( !inImage->downsampleFilter(outImages[whichOutImage], kernelX, kernelY) ) {
			return NULL;
		}
		END_CLOCK(filter);
		whichOutImage ^= 1;
	}

	return outImages[whichOutImage ^ 1];
}

int TiledResizeOperation::performResize()
{
	unsigned int targetWidth = (unsigned int)m_OutputWidth;
	unsigned int targetHeight = (unsigned int)m_OutputHeight;

	// Only downsampling, haven't tested or thought about upsampling.
	if( targetWidth > m_ImageReader->getWidth() || targetHeight > m_ImageReader->getHeight() ) {
		targetWidth = m_ImageReader->getWidth();
		targetHeight = m_ImageReader->getHeight();
	}

	// Some readers (JPEG) can get us close to our desired size for free.
	unsigned int readWidth;
	unsigned int readHeight;
	m_ImageReader->computeReadDimensions(targetWidth, targetHeight, readWidth, readHeight);

	// If we're still more than a power of two away, reduce by powers of two until we're there.
	unsigned int reducedWidth = readWidth;
	unsigned int reducedHeight = readHeight;
	while( reducedWidth / 2 >= targetWidth && reducedHeight / 2 >= targetHeight ) {
		reducedWidth /= 2;
		reducedHeight /= 2;
	}

	// Avoid unnecessary filtering if we're close.
	if( abs((int)reducedWidth - (int)targetWidth) < 4 && abs((int)reducedHeight - (int)targetHeight) < 4 ) {
		targetWidth = reducedWidth;
		targetHeight = reducedHeight;
	}

	bool skipFiltering = false;
	if( targetWidth == reducedWidth && targetHeight == reducedHeight ) {
		skipFiltering = true;
	}

	// Compute tile sizes.
	unsigned int outMaxRows = 128;
	unsigned int inMaxRows = ((readHeight * outMaxRows) / targetHeight);

	while( inMaxRows * readWidth > 1024 * 1024 && outMaxRows >= 16 ) {
		// The input is huge, so try a smaller output size.
		outMaxRows /= 2;
		inMaxRows = ((readHeight * outMaxRows) / targetHeight);
	}

	unsigned int tileOverlap = 12;
	unsigned int imagePadding = 12;
	bool skipScale = targetWidth == readWidth && targetHeight == readHeight;
	if( skipScale || skipFiltering || readHeight <= inMaxRows ) {
		// If we're not scaling/filtering, or the entire image fits into a single tile, we don't need to
		// have any redundant overlap between tiles, which is usually there for the edges of the tile
		// to filter properly.
		tileOverlap = 0;
	}

	EImageColorModel colorModel = m_ImageReader->getNativeColorModel() == kColorModel_RGBA ? kColorModel_RGBA : kColorModel_RGBX;

	bool success = false;

	// If the image is just too big (> ~8192), don't even try.
	// It might work, but it's untested.
	if( outMaxRows >= 16 ) {
		FilterKernelAdaptive* filterKernelX = createFilterKernel(m_ResizeQuality, reducedWidth, targetWidth);
		FilterKernelAdaptive* filterKernelY = createFilterKernel(m_ResizeQuality, reducedHeight, targetHeight);
		if( filterKernelX != NULL && filterKernelY != NULL ) {
			if( m_ImageReader->beginRead(readWidth, readHeight, colorModel) ) {
				if( m_ImageWriter->beginWrite(targetWidth, targetHeight, colorModel) ) {
					unsigned int maxSourceHeight = inMaxRows + tileOverlap * 4;
					ImageRGBA* sourceImage = ImageRGBA::create(readWidth, maxSourceHeight, imagePadding, 16);
					ImageRGBA* destImage1 = skipScale ? NULL : ImageRGBA::create(readWidth, outMaxRows + tileOverlap * 2, imagePadding, 16);
					ImageRGBA* destImage2 = skipScale ? NULL : ImageRGBA::create(readWidth, outMaxRows + tileOverlap * 2, imagePadding, 16);
					unsigned int prevTileUnprocessed = 0;
					unsigned int prevTileOverlap = 0;
					if( sourceImage != NULL && ((destImage1 != NULL && destImage2 != NULL) || skipScale) ) {
						unsigned int currentInRow = 0;
						unsigned int currentOutRow = 0;
						unsigned int rowsInRemaining = readHeight;
						unsigned int rowsOutRemaining = targetHeight;
						bool failed = false;
						while( rowsOutRemaining > 0 ) {
							unsigned int desiredInRows = inMaxRows;
							if( currentOutRow == 0 ) {
								// For the first tile we need to load a bit of the next one, for proper filtering.
								desiredInRows += tileOverlap;
							}

							unsigned int effectiveInRows = min(rowsInRemaining + prevTileUnprocessed, inMaxRows);
							unsigned int effectiveOutRows = min(rowsOutRemaining, outMaxRows);

							unsigned int rowsToRead = min(rowsInRemaining, desiredInRows);
							if( rowsOutRemaining == effectiveOutRows ) {
								rowsToRead = rowsInRemaining;
							}

							if( rowsToRead > 0 ) {
								sourceImage->setDimensions(readWidth, rowsToRead);
								// Start loading the next tile into the image below the part of the previous tile we kept around.
								sourceImage->setOffset(0, prevTileUnprocessed + prevTileOverlap);
								START_CLOCK(read);
								unsigned int rowsRead = m_ImageReader->readRows(sourceImage, 0, rowsToRead);
								END_CLOCK(read);
								if( rowsRead != rowsToRead ) {
									failed = true;
									// Abort, stop processing tiles.
									break;
								}
								sourceImage->setOffset(0, 0);
							}

							unsigned int rowsProcessed = effectiveInRows;
							unsigned int rowsAvailable = prevTileUnprocessed + rowsToRead;
							unsigned int rowsLeftOver = rowsAvailable - rowsProcessed;

							int outPreOverlap = ((targetHeight * prevTileOverlap) / readHeight);
							int outPostOverlap = ((targetHeight * rowsLeftOver) / readHeight);

							sourceImage->setDimensions(readWidth, rowsProcessed + prevTileOverlap + rowsLeftOver);

							// Setting these sample offsets allows us to filter the tile exactly the same way it would be
							// if the entire image was being filtered. The filter kernel was constructed for the entire image.
							int inSampleOffset = currentInRow - prevTileOverlap;
							int outSampleOffset = currentOutRow - outPreOverlap;
							filterKernelY->setSampleOffset(inSampleOffset, outSampleOffset);

							ImageRGBA* image = NULL;

							if( skipScale ) {
								image = sourceImage;
							} else {
								// Perform the resize of the tile.
								destImage1->setDimensions(targetWidth, effectiveOutRows + outPreOverlap + outPostOverlap);
								destImage2->setDimensions(targetWidth, effectiveOutRows + outPreOverlap + outPostOverlap);
								image = resizeTile(sourceImage, destImage1, destImage2, inSampleOffset, outSampleOffset, filterKernelX, filterKernelY, skipFiltering);
								// For writing we skip past the top few rows, which are from the previous tile.
								image->setOffset(0, outPreOverlap);
								image->setDimensions(targetWidth, effectiveOutRows);
							}

							START_CLOCK(write);
							unsigned int rowsWritten = m_ImageWriter->writeRows(image, 0, image->getHeight());
							END_CLOCK(write);
							if( rowsWritten != image->getHeight() ) {
								failed = true;
								// Abort, stop processing tiles.
								break;
							}
							image->setOffset(0, 0);

							if( rowsLeftOver > 0 ) {
								// Copy the bottom of this tile to the top of the image, so it serves as the filter edge padding  for the next resize.
								sourceImage->setDimensions(readWidth, maxSourceHeight);
								sourceImage->copyRect(sourceImage, 0, prevTileOverlap + rowsProcessed - tileOverlap, 0, 0, readWidth, rowsLeftOver + tileOverlap);
								sourceImage->setDimensions(readWidth, rowsLeftOver + tileOverlap);
								prevTileUnprocessed = rowsLeftOver;
								prevTileOverlap = tileOverlap;
							} else {
								prevTileUnprocessed = 0;
								prevTileOverlap = 0;
							}

							currentInRow += rowsProcessed;
							currentOutRow += effectiveOutRows;
							rowsOutRemaining -= effectiveOutRows;
							rowsInRemaining -= rowsToRead;
						}

						if( !failed ) {
							START_CLOCK(finish);
							if( m_ImageReader->endRead() ) {
								if( m_ImageWriter->endWrite() ) {
									END_CLOCK(finish);
									success = true;
								}
							}
						}
					}

					delete destImage1;
					delete destImage2;
					delete sourceImage;
				}
			}
		}

		delete filterKernelX;
		delete filterKernelY;
	}

	return success ? IMAGECORE_SUCCESS : IMAGECORE_UNKNOWN_ERROR;
}

}
