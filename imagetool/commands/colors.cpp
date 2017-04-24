//
//  colors.cpp
//  ImageTool
//
//  Created by Luke Alonso on 11/19/12.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#include "colors.h"
#include "imagecore/utils/mathutils.h"
#include "imagecore/utils/securemath.h"
#include "imagecore/formats/format.h"
#include "imagecore/image/image.h"
#include "imagecore/image/rgba.h"
#include "imagecore/image/resizecrop.h"
#include "imagecore/image/colorpalette.h"
#include "imagecore/image/colorspace.h"
#include <vector>
#include <algorithm>

REGISTER_COMMAND("colors", ColorsCommand);

const int maxNumColors = 32;

int ColorsCommand::run(const char** args, unsigned int numArgs)
{
	if( numArgs < 2 ) {
		fprintf(stderr, "Usage: ImageTool colors <input> [subDivisions] [numColors] [algorithm] [output]\n");
		fprintf(stderr, "\te.g. ImageTool colors <input> 2 5 histogram\n");
		return IMAGECORE_INVALID_USAGE;
	}

	const char* inputFilePath = args[0];

	EColorsAlgorithm algorithm = kColorAlgorithm_Histogram;
	if( numArgs >= 4 ) {
		if( strcmp(args[3], "kmeans") == 0 ) {
			algorithm = kColorAlgorithm_KMeans;
		} else if( strcmp(args[3], "histogram") == 0 ) {
			algorithm = kColorAlgorithm_Histogram;
		} else {
			fprintf(stderr, "error: invalid algorithm\n");
			return IMAGECORE_INVALID_USAGE;
		}
	}

	int numColors = 1;
	if( numArgs >= 3 ) {
		numColors = atoi(args[2]);
		if( numColors < 1 || (algorithm == kColorAlgorithm_Histogram && numColors > maxNumColors) || (algorithm == kColorAlgorithm_KMeans && numColors > 10) ) {
			fprintf(stderr, "error: invalid color count\n");
			return IMAGECORE_INVALID_USAGE;
		}
	}

	int subDivisions = 1;
	if( numArgs >= 2 ) {
		subDivisions = atoi(args[1]);
		if( subDivisions <= 0 || subDivisions > 16 || (subDivisions > 1 && numColors > 1) ) {
			fprintf(stderr, "error: invalid subdivision count\n");
			return IMAGECORE_INVALID_USAGE;
		}
	}

	START_CLOCK(ReadImage);

	ImageReader::Storage* source = ImageReader::FileStorage::open(inputFilePath);
	if( source == NULL ) {
		fprintf(stderr, "error: unable to open input file for '%s'\n", inputFilePath);
		return IMAGECORE_READ_ERROR;
	}

	ImageReader* imageReader = ImageReader::create(source);
	ResizeCropOperation resizeCrop;
	resizeCrop.setImageReader(imageReader);
	resizeCrop.setResizeMode(kResizeMode_AspectFit);
	resizeCrop.setResizeQuality(kResizeQuality_Low);
	resizeCrop.setOutputSize(128, 128);

	ImageRGBA* frameImage;
	resizeCrop.performResizeCrop(frameImage);

	END_CLOCK(ReadImage);

	int chunkW = frameImage->getWidth() / subDivisions;
	int chunkH = frameImage->getHeight() / subDivisions;
	bool hasAlpha = frameImage->getColorModel() == kColorModel_RGBA;
 	RGBA* subdivisionColors = (RGBA*)malloc(subDivisions * subDivisions * sizeof(RGBA));
	if( subDivisions > 1 ) {
		ImageRGBA* chunkFrameImage = ImageRGBA::create(chunkW, chunkH, hasAlpha);
		for( int cy = 0; cy < subDivisions; cy++ ) {
			for( int cx = 0; cx < subDivisions; cx++ ) {
				int ox = cx * chunkW;
				int oy = cy * chunkH;
				frameImage->copyRect(chunkFrameImage, ox, oy, 0, 0, chunkW, chunkH);
				RGBA rgba[16];
				double pct[16];
				ColorPalette::compute(chunkFrameImage, rgba, pct, 1, kColorAlgorithm_Histogram); // only use histogram algorithm for subDivisions for now
				subdivisionColors[cy * subDivisions + cx] = rgba[0];
			}
		}
		delete chunkFrameImage;
	}

	RGBA mainColors[maxNumColors];
	double colorPct[maxNumColors];
	int numOutColors = ColorPalette::compute(frameImage, mainColors, colorPct, numColors, algorithm);

	if( numArgs >= 5 ) {
		FILE* outputFile = fopen(args[4], "wb");
		if( outputFile == NULL ) {
			return IMAGECORE_WRITE_ERROR;
		}

		ImageWriter::FileStorage* outputStorage = new ImageWriter::FileStorage(outputFile);
		ImageWriter* writer = ImageWriter::createWithFormat(ImageWriter::formatFromExtension(args[1], kImageFormat_PNG), outputStorage);

		if( subDivisions > 1 ) {
			ImageRGBA* chunkImage = ImageRGBA::create(subDivisions, subDivisions, 4, 4, hasAlpha);
			for( int cy = 0; cy < subDivisions; cy++ ) {
				for( int cx = 0; cx < subDivisions; cx++ ) {
					const RGBA& color = subdivisionColors[cy * subDivisions + cx];
					chunkImage->clearRect(cx, cy, 1, 1, color.r, color.g, color.b, color.a);
				}
			}
			// Output an upsampled preview image.
			ImageRGBA* outImage = ImageRGBA::create(128, 128, hasAlpha);
			chunkImage->resize(outImage, kResizeQuality_Medium); // Prefer smoother, not sharper interpolations.
			if( writer == NULL || !writer->writeImage(outImage) ) {
				return IMAGECORE_WRITE_ERROR;
			}
			delete outImage;
		} else {
			int outWidthPerColor = 50;
			int outHeight = 30;
			unsigned int outPitch;
			ImageRGBA* outImage = ImageRGBA::create(outWidthPerColor * numOutColors, outHeight, hasAlpha);
			uint8_t* outImageBuffer = outImage->lockRect(outWidthPerColor * numOutColors, outHeight, outPitch);
			for( int k = 0; k < numOutColors; k++ ) {
				for( int i = k * outWidthPerColor; i < (k + 1) * outWidthPerColor; i++ ) {
					*(RGBA*)(&outImageBuffer[i * 4]) = mainColors[k];
				}
			}
			for( int j = 1; j < outHeight; j++ ) {
				memcpy(&outImageBuffer[outWidthPerColor * numOutColors * 4 * j], outImageBuffer, outWidthPerColor * numOutColors * 4);
			}

			if( writer == NULL || !writer->writeImage(outImage) ) {
				return IMAGECORE_WRITE_ERROR;
			}

			delete outImage;
		}
		fclose(outputFile);
		delete outputStorage;
		delete writer;
	} else {
		// Just print out the dominant colors.
		if( numColors == 1 ) {
			printf("Dominant: ");
			printf("#%02X%02X%02X", mainColors[0].r, mainColors[0].g, mainColors[0].b);
			printf("\n");
		} else {
			SECURE_ASSERT(numColors > 1);
			printf("Dominant:\n");
			for( int i = 0; i < numOutColors; i++) {
				printf("#%02X%02X%02X: %0.2f%%\n", mainColors[i].r, mainColors[i].g, mainColors[i].b, colorPct[i] * 100);
			}
		}
		if( subDivisions > 1 ) {
			printf("Subdivisions: ");
			for( int cy = 0; cy < subDivisions; cy++ ) {
				for( int cx = 0; cx < subDivisions; cx++ ) {
					const RGBA& chunkColor = subdivisionColors[cy * subDivisions + cx];
					printf("#%02X%02X%02X ", chunkColor.r, chunkColor.g, chunkColor.b);
				}
			}
			printf("\n");
		}
	}

	free(subdivisionColors);

	delete source;
	delete imageReader;

	return IMAGECORE_SUCCESS;
}
