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

#include "gif.h"
#include "imagecore/utils/securemath.h"
#include "imagecore/utils/mathutils.h"
#include "imagecore/image/rgba.h"

namespace imagecore {

REGISTER_IMAGE_READER(ImageReaderGIF);

int gifRead(GifFileType* gif, GifByteType* dest, int numBytes)
{
	ImageReader::Storage* source = (ImageReader::Storage*)gif->UserData;
	return (int)source->read(dest, numBytes);
}

ImageReader* ImageReaderGIF::Factory::create()
{
	return new ImageReaderGIF();
}

bool ImageReaderGIF::Factory::matchesSignature(const uint8_t* sig, unsigned int sigLen)
{
	if( sigLen >= 3 && sig[0] == 0x47 && sig[1] == 0x49 && sig[2] == 0x46 ) {
		return true;
	}
	return false;
}

ImageReaderGIF::ImageReaderGIF()
:	m_Source(NULL)
,	m_Width(0)
,	m_Height(0)
,	m_CurrentFrame(0)
,	m_Gif(NULL)
,	m_PrevFrameCopy(NULL)
,	m_HasAlpha(true)
{
}

ImageReaderGIF::~ImageReaderGIF()
{
	if( m_Gif ) {
		DGifCloseFile(m_Gif);
		m_Gif = NULL;
	}
	delete m_PrevFrameCopy;
	m_PrevFrameCopy = NULL;
}

bool ImageReaderGIF::initWithStorage(ImageReader::Storage* source)
{
	m_Source = source;
	return true;
}

bool ImageReaderGIF::readHeader()
{
	int err = GIF_OK;
	m_Gif = DGifOpen(m_Source, gifRead, &err);
	if( err != GIF_OK ) {
		return false;
	}
	if (DGifParseFrames(m_Gif) == GIF_ERROR) {
		return false;
	}
	if( m_Gif->ImageCount < 1 ) {
		return false;
	}
	m_Width = m_Gif->SWidth;
	m_Height = m_Gif->SHeight;
	return true;
}

bool getGraphicsControlBlock(SavedImage* image, GraphicsControlBlock& gcb)
{
	bool found = false;
	for( int i = 0; i < image->ExtensionBlockCount; i++ ) {
		ExtensionBlock* eb = image->ExtensionBlocks + i;
		if( eb->Function == GRAPHICS_EXT_FUNC_CODE ) {
            if( DGifExtensionToGCB(eb->ByteCount, eb->Bytes, &gcb) == GIF_OK ) {
                found = true;
            }
		}
	}
	return found;
}

void getValidRegion(int& left, int& top, int& width, int& height, const GifImageDesc& imageDesc, unsigned int maxWidth, unsigned int maxHeight)
{
	left = clamp(0, maxWidth - 1, imageDesc.Left);
	top = clamp(0, maxHeight - 1, imageDesc.Top);
	width = min((int)maxWidth - left, imageDesc.Width);
	height = min((int)maxHeight - top, imageDesc.Height);
}

// We trust libgif not to be stupid when setting up extension blocks.
int getTransparentIndex(SavedImage* image)
{
	GraphicsControlBlock gcb;
	if( getGraphicsControlBlock(image, gcb) ) {
		return gcb.TransparentColor;
	}
	return -1;
}

int getFrameDelay(SavedImage* image)
{
	GraphicsControlBlock gcb;
	if( getGraphicsControlBlock(image, gcb) ) {
		return gcb.DelayTime;
	}
	return 0;
}

int getDisposalMode(SavedImage* image)
{
	GraphicsControlBlock gcb;
	if( getGraphicsControlBlock(image, gcb) ) {
		return gcb.DisposalMode;
	}
	return 0;
}

void ImageReaderGIF::preDecodeFrame(unsigned int frameIndex)
{
	if( m_Gif == NULL || (unsigned int)m_Gif->ImageCount <= frameIndex ) {
		return;
	}
	SavedImage* savedImage = &m_Gif->SavedImages[frameIndex];
	if( savedImage->RasterBits == NULL ) {
		DGifDecodeFrame(m_Gif, frameIndex);
	}
}

bool ImageReaderGIF::copyFrameRegion(unsigned int frameIndex, ImageRGBA* destImage, bool writeBackground)
{
	if( m_Gif == NULL || (unsigned int)m_Gif->ImageCount <= frameIndex ) {
		return false;
	}

	SavedImage* savedImage = &m_Gif->SavedImages[frameIndex];
	if( savedImage->RasterBits == NULL ) {
		if( DGifDecodeFrame(m_Gif, frameIndex) != GIF_OK || savedImage->RasterBits == NULL ) {
			return false;
		}
	}
	GifImageDesc& imageDesc = savedImage->ImageDesc;
	int transparentIndex = getTransparentIndex(savedImage);
	uint8_t* regionImage = savedImage->RasterBits;
	if( regionImage == NULL ) {
		return false;
	}

	ColorMapObject* colorMap = imageDesc.ColorMap ? imageDesc.ColorMap : m_Gif->SColorMap;
	if( colorMap == NULL ) {
		return false;
	}

	int regionX;
	int regionY;
	int regionWidth;
	int regionHeight;
	getValidRegion(regionX, regionY, regionWidth, regionHeight, imageDesc, m_Width, m_Height);

	unsigned int destPitch = 0;
	uint8_t* destBuffer = destImage->lockRect(regionX, regionY, regionWidth, regionHeight, destPitch);
	bool hadAlpha = false;

	for( unsigned int y = 0; y < regionHeight; y++ ) {
		for( unsigned int x = 0; x < regionWidth; x++ ) {
			unsigned char paletteIndex = regionImage[y * imageDesc.Width + x];
			unsigned int outIndex = y * destPitch + x * 4U;
			bool isAlpha = transparentIndex == paletteIndex;
			if( !isAlpha ) {
				GifColorType* color = &colorMap->Colors[paletteIndex];
				destBuffer[outIndex + 0] = color->Red;
				destBuffer[outIndex + 1] = color->Green;
				destBuffer[outIndex + 2] = color->Blue;
				destBuffer[outIndex + 3] = 255;
			} else {
				if( writeBackground ) {
					destBuffer[outIndex + 0] = 255;
					destBuffer[outIndex + 1] = 255;
					destBuffer[outIndex + 2] = 255;
					destBuffer[outIndex + 3] = 0;
				}
				if( !hadAlpha ) {
					hadAlpha = true;
				}
			}
		}
	}

	// If we previously had a transparent frame, but just wrote a full frame over it, then it no longer has alpha.
	if( m_HasAlpha && !hadAlpha && regionWidth == m_Width && regionHeight == m_Height) {
		m_HasAlpha = false;
	}

	destImage->unlockRect();

	return true;
}

bool ImageReaderGIF::readImage(Image* dest)
{
	if( !supportsOutputColorModel(dest->getColorModel()) ) {
		return false;
	}

	ImageRGBA* destImage = dest->asRGBA();

	if( m_Gif == NULL || (unsigned int)m_Gif->ImageCount <= m_CurrentFrame) {
		return false;
	}

	SavedImage* prevFrame = m_CurrentFrame > 0 ? &m_Gif->SavedImages[m_CurrentFrame - 1] : NULL;
	SavedImage* currentFrame = &m_Gif->SavedImages[m_CurrentFrame];
	int prevDisposalMode = prevFrame != NULL ? getDisposalMode(prevFrame) : DISPOSAL_UNSPECIFIED;
	int currDisposalMode = getDisposalMode(currentFrame);

	// if this is the 1st frame and it is not equal to the full image size, perform a clear
	if((m_CurrentFrame == 0) && ((currentFrame->ImageDesc.Left != 0) || (currentFrame->ImageDesc.Top != 0) || (currentFrame->ImageDesc.Width != m_Width) || (currentFrame->ImageDesc.Height != m_Height))) {
		destImage->clear(0, 0, 0, 0);
	}

	// If consecutive frames use DISPOSE_PREVIOUS, it means keep using the original frame.
	// Otherwise, copy the current frame to copy back later.
	if( currDisposalMode == DISPOSE_PREVIOUS && prevDisposalMode != DISPOSE_PREVIOUS && prevFrame != NULL ) {
		if( m_PrevFrameCopy == NULL ) {
			m_PrevFrameCopy = ImageRGBA::create(destImage->getWidth(), destImage->getHeight(), destImage->getColorModel() == kColorModel_RGBA);
			if( m_PrevFrameCopy == NULL ) {
				return false;
			}
		}
		destImage->copy(m_PrevFrameCopy);
	}

	// Previous frame asked for it to be cleared by copying back the original frame.
	if( prevDisposalMode == DISPOSE_PREVIOUS && prevFrame != NULL && m_PrevFrameCopy != NULL ) {
		m_PrevFrameCopy->copy(destImage);
		const GifImageDesc& desc = prevFrame->ImageDesc;
		int left, top, width, height;
		getValidRegion(left, top, width, height, desc, m_Width, m_Height);
		destImage->copyRect(m_PrevFrameCopy, left, top, left, top, width, height);
	}

	// Previous frame specified a background clear, so clear the region covered by that frame.
	if( prevDisposalMode == DISPOSE_BACKGROUND  && prevFrame != NULL ) {
		const GifImageDesc& desc = prevFrame->ImageDesc;
		int left, top, width, height;
		getValidRegion(left, top, width, height, desc, m_Width, m_Height);
		destImage->clearRect(left, top, width, height, 255, 255, 255, 0);
		m_HasAlpha = true;
	}

	return copyFrameRegion(m_CurrentFrame, destImage, m_CurrentFrame == 0);
}

unsigned int ImageReaderGIF::getNumFrames()
{
	return m_Gif->ImageCount;
}

bool ImageReaderGIF::advanceFrame()
{
	if( m_CurrentFrame < (unsigned int)m_Gif->ImageCount ) {
		m_CurrentFrame++;
		return true;
	}
	return false;
}

bool ImageReaderGIF::seekToFirstFrame() {
	m_CurrentFrame = 0;
	return true;
}

unsigned int ImageReaderGIF::getFrameDelayMs()
{
	if( m_Gif == NULL || (unsigned int)m_Gif->ImageCount <= m_CurrentFrame) {
		return false;
	}
	return getFrameDelay(&m_Gif->SavedImages[m_CurrentFrame]) * 10;
}

EImageFormat ImageReaderGIF::getFormat()
{
	return kImageFormat_GIF;
}

const char* ImageReaderGIF::getFormatName()
{
	return "GIF";
}

unsigned int ImageReaderGIF::getWidth()
{
	return m_Width;
}

unsigned int ImageReaderGIF::getHeight()
{
	return m_Height;
}

EImageColorModel ImageReaderGIF::getNativeColorModel()
{
	return m_HasAlpha ? kColorModel_RGBA : kColorModel_RGBX;
}

}
