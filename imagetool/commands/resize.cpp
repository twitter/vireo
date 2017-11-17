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

#include "resize.h"
#include "imagecore/utils/securemath.h"
#include "imagecore/utils/mathutils.h"
#include "imagecore/image/resizecrop.h"

REGISTER_COMMAND("resize", ResizeCommand);

bool parseOutputSize(const char* outputSize, unsigned int inputWidth, unsigned int inputHeight, unsigned int& targetWidth, unsigned int& targetHeight)
{
	unsigned int w = 0;
	unsigned int h = 0;
	if( sscanf(outputSize, "%d%%x%d%%", &w, &h) == 2 ) {
		targetWidth = (inputWidth * w) / 100;
		targetHeight = (inputHeight * h) / 100;
	} else if( sscanf(outputSize, "%dx%d", &w, &h) == 2 ) {
		targetWidth = w;
		targetHeight = h;
	} else if( sscanf(outputSize, "%d%%", &w) == 1 ) {
		targetWidth = (inputWidth * w) / 100;
		targetHeight = (inputHeight * w) / 100;
	} else if( sscanf(outputSize, "%d", &w) == 1 ) {
		targetWidth = w;
		targetHeight = w;
	}
	return Image::validateSize(targetWidth, targetHeight);
}

bool parseBackgroundfillSize(const char* outputSize, unsigned int outputWidth, unsigned int outputHeight, unsigned int& targetWidth, unsigned int& targetHeight)
{
	unsigned int w = 0;
	unsigned int h = 0;

	if( sscanf(outputSize, "%dx%d", &w, &h) == 2 ) {
		targetWidth = w;
		targetHeight = h;
		if( ( targetWidth < outputWidth) || ( targetHeight < outputHeight ) ) {
			return false; // can't have background smaller than the resized image
		}
	}
	return Image::validateSize(targetWidth, targetHeight);
}

bool parseBackfillColor(const char* backfillColor, unsigned char& backfillR, unsigned char& backfillG, unsigned char& backfillB )
{
	int rgb = 0;
	if( sscanf(backfillColor, "#%x", &rgb) == 1 ) {
		if( (rgb >= 0) && (rgb <= 0xFFFFFF) ) {
			backfillR = (unsigned char)((rgb >> 16) & 0xFF);
			backfillG = (unsigned char)((rgb >> 8) & 0xFF);
			backfillB = (unsigned char)(rgb & 0xFF);
			return true;
		}
	}
	return false;
}

// helper code to convert resize quality from string to enum
struct ResizeQualityMapEntry
{
	const char* m_String;
	EResizeQuality m_Enum;
};

static ResizeQualityMapEntry resizeQualityMap[kResizeQuality_MAX] = {
	{ "bilinear", kResizeQuality_Bilinear },
	{ "low", kResizeQuality_Low },
	{ "medium", kResizeQuality_Medium },
	{ "high", kResizeQuality_High },
	{ "highSharp", kResizeQuality_HighSharp }
};

static EResizeQuality getResizeQuality(const char* resizeQuality)
{
	for (uint32_t i = 0; i < kResizeQuality_MAX; i++) {
		if(strcmp(resizeQualityMap[i].m_String, resizeQuality) == 0) {
			return resizeQualityMap[i].m_Enum;
		}
	}
	// if can not find specified resize quality default to High
	return kResizeQuality_High;
}

Image* backfillImage(Image* srcImage, unsigned int width, unsigned int height, unsigned char r, unsigned char g, unsigned char b)
{
	Image* image = Image::create(srcImage->getColorModel(), width, height, 0, 0);
	if( image != NULL ) {
		const unsigned int srcWidth = srcImage->getWidth();
		const unsigned int widthDiff = width - srcWidth;
		const unsigned int srcHeight = srcImage->getHeight();
		const unsigned int heightDiff = height - srcHeight;
		const unsigned int padLeft = widthDiff / 2;
		const unsigned int padTop = heightDiff / 2;
		const unsigned int padRight = width - srcWidth - padLeft;
		const unsigned int padBottom = height - srcHeight - padTop;
		if( image != NULL ) {
			srcImage->copyRect(image, 0, 0, padLeft, padTop, srcWidth, srcHeight);
			if( padTop > 0 ) {
				image->clearRect(0, 0, width, padTop, r, g, b, 255);
			}
			if( padBottom > 0 ) {
				image->clearRect(0, padTop + srcHeight, width, padBottom, r, g, b, 255);
			}
			if( padLeft > 0 ) {
				image->clearRect(0, padTop, padLeft, srcHeight, r, g, b, 255);
			}
			if( padRight > 0 ) {
				image->clearRect(padLeft + srcWidth, padTop, padRight, srcHeight, r, g, b, 255);
			}
			return image;
		}
	}
	return NULL;
}


ResizeCommand::ResizeCommand()
{
}

ResizeCommand::~ResizeCommand()
{
}

int ResizeCommand::run(const char** args, unsigned int numArgs)
{
	if( numArgs < 3 ) {
		fprintf(stderr, "Usage: ImageTool resize <input> <output> <size> [-mode crop|fit|fill] [-gravity center|left|top|right|bottom] [-region <width>x<height>L<left_offset>T<top_offset>] [-mod N] [-filequality 0-100] [-resizequality 0-2] [-forcergb true|false] [-pad N,N,N]\n");
		fprintf(stderr, "\te.g. ImageTool resize input.jpg output.jpg 1000x1000 -filequality 75\n");
		return IMAGECORE_INVALID_USAGE;
	}

	int ret = open(args[0], args[1]);
	if (ret != IMAGECORE_SUCCESS) {
		return ret;
	}

	ret = performResize(args, numArgs);
	if (ret != IMAGECORE_SUCCESS) {
		return ret;
	}

	return close();
}

int ResizeCommand::performResize(const char** args, unsigned int numArgs)
{
	ImageReader* reader = ImageReader::create(m_Source);
	if( reader == NULL ) {
		fprintf(stderr, "error: unknown or corrupt image format for '%s'\n", m_InputFilePath);
		return IMAGECORE_INVALID_FORMAT;
	}

	const char* outputSize = args[2];
	unsigned int parseWidth = reader->getOrientedWidth();
	unsigned int parseHeight = reader->getOrientedHeight();
	unsigned int outputWidth = 0;
	unsigned int outputHeight = 0;

	if( !parseOutputSize(outputSize, parseWidth, parseHeight, outputWidth, outputHeight) ) {
		fprintf(stderr, "error: bad size parameter\n");
		delete reader;
		return IMAGECORE_INVALID_OUTPUT_SIZE;
	}

	bool shouldCrop = true;
	ECropGravity cropGravity = kGravityHeuristic;
	bool minAxis = false;
	EResizeQuality resizeQuality = kResizeQuality_High;
	unsigned int compressionQuality = 75;
	ImageRegion* cropRegion = NULL;
	bool allowYUV = true;
	bool allowUpsample = true;
	bool allowDownsample = true;
	EResizeMode resizeMode = kResizeMode_ExactCrop;
	const char* format = NULL;
	bool didSetMode = false;
	bool forceRGB = false;
	bool forceRLE = false;
	bool progressive = false;
	bool backfill = false;
	unsigned int backfillWidth = 0;
	unsigned int backfillHeight = 0;
	unsigned char backfillR = 0;
	unsigned char backfillG = 0;
	unsigned char backfillB = 0;
	unsigned int mod = 1;
	const char* writerArgNames[32];
	const char* writerArgValues[32];
	unsigned int numWriterArgs = 0;

	// Optional args.
	unsigned int numOptional = numArgs - 3;
	if( numOptional > 0 ) {
		unsigned int numPairs = numOptional / 2;
		for( unsigned int i = 0; i < numPairs; i++ ) {
			const char* argName = args[3 + i * 2 + 0];
			const char* argValue = args[3 + i * 2 + 1];
			if( strcmp(argName, "-crop") == 0 ) {
				shouldCrop = strcmp(argValue, "true") == 0;
			} else if( strcmp(argName, "-gravity") == 0 ) {
				if( strcmp(argValue, "center") == 0 ) {
					cropGravity = kGravityCenter;
				} else if( strcmp(argValue, "left") == 0 ) {
					cropGravity = kGravityLeft;
				} else if( strcmp(argValue, "top") == 0 ) {
					cropGravity = kGravityTop;
				} else if( strcmp(argValue, "right") == 0 ) {
					cropGravity = kGravityRight;
				} else if( strcmp(argValue, "bottom") == 0 ) {
					cropGravity = kGravityBottom;
				}
			} else if( strcmp(argName, "-region") == 0 ) {
				// because this flag causes a new ImageRegion to be allocated,
				// make sure to free the previous allocation in the case that
				// this is not the first -region flag.
				if( cropRegion != NULL ) {
					delete cropRegion;
					cropRegion = NULL;
				}

				if( (cropRegion = ImageRegion::fromString(argValue)) == NULL ) {
					fprintf(stderr, "error: invalid crop region given as '%s'\n", argValue);
					delete reader;
					return IMAGECORE_INVALID_USAGE;
				}

				if( cropRegion != NULL && (		!Image::validateSize(cropRegion->width(), cropRegion->height())
										   ||	SafeUAdd(cropRegion->left(), cropRegion->width()) > parseWidth
										   ||	SafeUAdd(cropRegion->top(), cropRegion->height()) > parseHeight) ) {
					fprintf(stderr, "error: crop region not within image dimensions\n");
					delete reader;
					return IMAGECORE_INVALID_OUTPUT_SIZE;
				}
			} else if( strcmp(argName, "-minaxis") == 0 ) {
				minAxis = strcmp(argValue, "true") == 0;
			} else if( strcmp(argName, "-resizequality") == 0 ) {
				resizeQuality = getResizeQuality(argValue);
			} else if( strcmp(argName, "-filequality") == 0 || strcmp(argName, "-quality") == 0 ) {
				compressionQuality = clamp(0, 100, atoi(argValue));
			} else if( strcmp(argName, "-pad") == 0 ) {
				int ret = populateBuckets(argValue);
				if (ret != IMAGECORE_SUCCESS) {
					delete reader;
					return ret;
				}
			} else if( strcmp(argName, "-forcergb") == 0 ) {
				forceRGB = strcmp(argValue, "true") == 0;
				if( forceRGB ) {
					reader->setReadOptions(ImageReader::kReadOption_ApplyColorProfile);
				}
			} else if( strcmp(argName, "-yuvpath") == 0 ) {
				allowYUV = strcmp(argValue, "true") == 0;
			} else if( strcmp(argName, "-upsample") == 0 ) {
				allowUpsample = strcmp(argValue, "true") == 0;
			} else if( strcmp(argName, "-downsample") == 0 ) {
				allowDownsample = strcmp(argValue, "true") == 0;
			} else if( strcmp(argName, "-format") == 0 ) {
				format = argValue;
			} else if( strcmp(argName, "-progressive") == 0 ) {
				progressive = strcmp(argValue, "true") == 0;
			} else if( strcmp(argName, "-mode") == 0 ) {
				if( strcmp(argValue, "fit") == 0 ) {
					resizeMode = kResizeMode_AspectFit;
				} else if( strcmp(argValue, "fill") == 0 ) {
					resizeMode = kResizeMode_AspectFill;
				} else if( strcmp(argValue, "crop") == 0 ) {
					resizeMode = kResizeMode_ExactCrop;
				} else if( strcmp(argValue, "stretch") == 0 ) {
					resizeMode = kResizeMode_Stretch;
				} else {
					fprintf(stderr, "error: bad resize mode\n");
					delete reader;
					return IMAGECORE_INVALID_USAGE;
				}
				didSetMode = true;
			} else if( strcmp(argName, "-mod") == 0 ) {
				mod = atoi(argValue);
			} else if( strcmp(argName, "-png:forceRLE") == 0 ) {
				forceRLE = strcmp(argValue, "true") == 0;
			} else if( strcmp(argName, "-backfillsize") == 0 ) {
				if ( !parseBackgroundfillSize(argValue, outputWidth, outputHeight, backfillWidth, backfillHeight) ) {
					fprintf(stderr, "error: bad backfill size\n");
					delete reader;
					return IMAGECORE_INVALID_OUTPUT_SIZE;
				}
				backfill = true;
			} else if( strcmp(argName, "-backfillcolor") == 0 ) {
				if ( !parseBackfillColor(argValue, backfillR, backfillG, backfillB) ) {
					fprintf(stderr, "error: bad backfill color\n");
					delete reader;
					return IMAGECORE_INVALID_COLOR;
				}
			} else if( strncmp(argName, "-encoder:", 9) == 0 ) {
				size_t argLen = strlen(argName) - 9;
				if( argLen > 0 ) {
					writerArgNames[numWriterArgs] = argName + 9;
					writerArgValues[numWriterArgs] = argValue;
					numWriterArgs++;
				} else {
					fprintf(stderr, "error: bad encoder argument\n");
					delete reader;
					return IMAGECORE_INVALID_USAGE;
				}
			}
		}
	}

	if (!didSetMode) {
		// Legacy params.
		if (shouldCrop) {
			resizeMode = kResizeMode_ExactCrop;
		} else if (minAxis) {
			resizeMode = kResizeMode_AspectFit;
		} else {
			resizeMode = kResizeMode_AspectFill;
		}
	}

	EImageFormat outputFormat = ImageWriter::formatFromExtension(format != NULL ? format : args[1], reader->getFormat());

	ResizeCropOperation resizeCrop;
	resizeCrop.setImageReader(reader);
	resizeCrop.setCropGravity(cropGravity);
	resizeCrop.setResizeQuality(resizeQuality);
	resizeCrop.setCropRegion(cropRegion);
	resizeCrop.setOutputSize(outputWidth, outputHeight);
	resizeCrop.setResizeMode(resizeMode);
	resizeCrop.setAllowUpsample(allowUpsample);
	resizeCrop.setAllowDownsample(allowDownsample);
	resizeCrop.setOutputMod(mod);
	if( backfill ) {
		resizeCrop.setBackgroundFillColor(backfillR, backfillG, backfillB);
	}

	// Reader and writer agree on a more optimal mutual color model and no backfill color is specified use native,
	// otherwise use default (rgb).
	EImageColorModel nativeColorModel = reader->getNativeColorModel();
	if( ImageWriter::outputFormatSupportsColorModel(outputFormat, nativeColorModel) && !backfill ) {
		// Some further restrictions, the YUV path isn't optimized for anything but high quality.
		// Also, color profiles cannot be applied.
		if( Image::colorModelIsYUV(nativeColorModel) ) {
			if( allowYUV && resizeQuality >= kResizeQuality_High && !forceRGB ) {
				resizeCrop.setOutputColorModel(nativeColorModel);
			}
		} else {
			resizeCrop.setOutputColorModel(nativeColorModel);
		}
	}

	Image* resizedImage = NULL;
	int ret = resizeCrop.performResizeCrop(resizedImage);
	if( ret != IMAGECORE_SUCCESS ) {
		delete reader;
		return ret;
	}

	START_CLOCK(compress);

	ImageWriter* writer = ImageWriter::createWithFormat(outputFormat, m_Output);
	if (writer == NULL) {
		fprintf(stderr, "error: unable to create ImageWriter\n");
		delete reader;
		return IMAGECORE_OUT_OF_MEMORY;
	}

	unsigned int writeOptions = 0;
	if( forceRGB ) {
		writeOptions |= ImageWriter::kWriteOption_WriteDefaultColorProfile;
	} else {
		writeOptions |= ImageWriter::kWriteOption_CopyColorProfile;;
	}
	if( progressive ) {
		writeOptions |= ImageWriter::kWriteOption_Progressive;
	}
	if( forceRLE ) {
		writeOptions |= ImageWriter::kWriteOption_ForcePNGRunLengthEncoding;
	}
	writer->setWriteOptions(writeOptions);

	// Allows certain formats to re-use information from the input image, like color profiles.
	writer->setSourceReader(reader);

	writer->setQuality(compressionQuality);

	if( !writer->applyExtraOptions(writerArgNames, writerArgValues, numWriterArgs) ) {
		fprintf(stderr, "error: unable to apply writer-specific options\n");
		delete reader;
		delete writer;
		return IMAGECORE_INVALID_USAGE;
	}

	// handle backfill requests
	Image* backfilledImage = NULL;
	Image* finalImage;
	if( backfill ) {
		backfilledImage = backfillImage(resizedImage, backfillWidth, backfillHeight, backfillR, backfillG, backfillB);
		finalImage = backfilledImage;
	} else {
		finalImage = resizedImage;
	}

	if( !writer->writeImage(finalImage) ) {
		fprintf(stderr, "error: failed to compress image\n");
		delete reader;
		delete writer;
		return IMAGECORE_WRITE_ERROR;
	}
	END_CLOCK(compress);

	delete backfilledImage;
	delete writer;
	delete reader;

	return IMAGECORE_SUCCESS;
}
