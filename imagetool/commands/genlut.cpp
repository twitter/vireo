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

#include "genlut.h"
#include "imagecore/utils/mathutils.h"
#include "imagecore/utils/securemath.h"
#include "imagecore/formats/format.h"
#include "imagecore/image/image.h"
#include "imagecore/image/rgba.h"

REGISTER_COMMAND("genlut", GenLUTCommand);

int GenLUTCommand::run(const char** args, unsigned int numArgs)
{
	if( numArgs < 3 ) {
		fprintf(stderr, "Usage: ImageTool genlut [1d|3d] <input> <output>\n");
		fprintf(stderr, "\te.g. ImageTool genlut 3d input.png output.cube\n");
		return IMAGECORE_INVALID_USAGE;
	}

	FILE* outputFile = fopen(args[2], "wb");

	if( strcmp(args[0], "3d") == 0 || strcmp(args[0], "1d") == 0  ) {
		const char* inputFilePath = args[1];

		ImageReader::Storage* source = ImageReader::FileStorage::open(inputFilePath);
		if( source == NULL ) {
			fprintf(stderr, "error: unable to open input file for '%s'\n", inputFilePath);
			return IMAGECORE_READ_ERROR;
		}

		ImageReader* imageReader = ImageReader::create(source);
		if( imageReader == NULL ) {
			fprintf(stderr, "error: unknown or corrupt image format for '%s'\n", inputFilePath);
			return IMAGECORE_INVALID_FORMAT;
		}

		ImageRGBA* inputImage = ImageRGBA::create(imageReader->getWidth(), imageReader->getHeight());
		SECURE_ASSERT(inputImage != NULL);
		imageReader->readImage(inputImage);

		const uint8_t* buffer = inputImage->getBytes();
		unsigned int inHeight = inputImage->getHeight();
		unsigned int inPitch = inputImage->getPitch();

		if( strcmp(args[0], "3d") == 0 ) {
			unsigned int lutSize = inputImage->getWidth();
			SECURE_ASSERT(inputImage->getHeight() == lutSize * lutSize);
			fprintf(outputFile, "#Created by: Twitter ImageTool\n");
			fprintf(outputFile, "LUT_3D_SIZE %i\n", lutSize);
			fprintf(outputFile, "DOMAIN_MIN 0.0 0.0 0.0\n");
			fprintf(outputFile, "DOMAIN_MAX 1.0 1.0 1.0\n");
			for( unsigned int b = 0; b < lutSize; b++ ) {
				unsigned int baseY = b * lutSize;
				for( unsigned int g = 0; g < lutSize; g++ ) {
					for( unsigned int r = 0; r < lutSize; r++ ) {
						int inX = r;
						int inY = g + baseY;
						int inOffset = (inY * inPitch) + inX * 4;
						unsigned int sampr = buffer[inOffset + 0];
						unsigned int sampg = buffer[inOffset + 1];
						unsigned int sampb = buffer[inOffset + 2];
						float rf = (float)sampr / 255.0f;
						float gf = (float)sampg / 255.0f;
						float bf = (float)sampb / 255.0f;
						fprintf(outputFile, "%f %f %f\n", rf, gf, bf);
					}
				}
			}
		} else if( strcmp(args[0], "1d") == 0 ) {
			fprintf(outputFile, "#Created by: Twitter ImageTool\n");
			fprintf(outputFile, "LUT_3D_SIZE 17\n");
			fprintf(outputFile, "DOMAIN_MIN 0.0 0.0 0.0\n");
			fprintf(outputFile, "DOMAIN_MAX 1.0 1.0 1.0\n");
			unsigned int lutSize = 17;
			for( unsigned int b = 0; b < lutSize; b++ ) {
				for( unsigned int g = 0; g < lutSize; g++ ) {
					for( unsigned int r = 0; r < lutSize; r++ ) {
						float rl = (float)r / ((float)lutSize - 1.0f);
						float gl = (float)g / ((float)lutSize - 1.0f);
						float bl = (float)b / ((float)lutSize - 1.0f);
						unsigned int rs = clamp(0, inHeight - 1, rl * (float)inHeight);
						unsigned int gs = clamp(0, inHeight - 1, gl * (float)inHeight);
						unsigned int bs = clamp(0, inHeight - 1, bl * (float)inHeight);
						unsigned int sampr = buffer[rs * inPitch + 0];
						unsigned int sampg = buffer[gs * inPitch + 1];
						unsigned int sampb = buffer[bs * inPitch + 2];
						float rf = (float)sampr / 255.0f;
						float gf = (float)sampg / 255.0f;
						float bf = (float)sampb / 255.0f;
						fprintf(outputFile, "%f %f %f\n", rf, gf, bf);
					}
				}
			}
		}
	} else if( strcmp(args[0], "identity") == 0 ) {
		int lutSize = atoi(args[1]);
		int padding = 0;
		if( numArgs > 3 ) {
			padding = atoi(args[3]);
		}
		ImageRGBA* outputImage = ImageRGBA::create((lutSize + (padding * 2)), (lutSize + (padding * 2)) * lutSize);
		SECURE_ASSERT(outputImage != NULL);
		unsigned int outPitch = 0;
		uint8_t* outputBuffer = outputImage->lockRect(outputImage->getWidth(), outputImage->getHeight(), outPitch);
		for( int b = 0; b < lutSize; b++ ) {
			unsigned int baseY = b * (lutSize + (padding * 2));
			for( int g = -padding; g < lutSize + padding; g++ ) {
				for( int r = -padding; r < lutSize + padding; r++ ) {
					int clampedG = clamp(0, lutSize - 1, g);
					int clampedR = clamp(0, lutSize - 1, r);
					float rValue = (float)clampedR / ((float)lutSize - 1.0f);
					float gValue = (float)clampedG / ((float)lutSize - 1.0f);
					float bValue = (float)b / ((float)lutSize - 1.0f);
					int outY = g + padding + baseY;
					int outX = r + padding;
					int outOffset = (outY * outPitch) + outX * 4;
					outputBuffer[outOffset + 0] = clamp(0, 255, rValue * 255.0f);
					outputBuffer[outOffset + 1] = clamp(0, 255, gValue * 255.0f);
					outputBuffer[outOffset + 2] = clamp(0, 255, bValue * 255.0f);
					outputBuffer[outOffset + 3] = 255;
				}
			}
		}
		ImageWriter::FileStorage outputStorage(outputFile);
		ImageWriter* writer = ImageWriter::createWithFormat(kImageFormat_PNG, &outputStorage);
		writer->writeImage(outputImage);
	} else if( strcmp(args[0], "depad") == 0 ) {
		const char* inputFilePath = args[1];

		ImageReader::Storage* source = ImageReader::FileStorage::open(inputFilePath);
		if( source == NULL ) {
			fprintf(stderr, "error: unable to open input file for '%s'\n", inputFilePath);
			return IMAGECORE_READ_ERROR;
		}

		ImageReader* imageReader = ImageReader::create(source);
		if( imageReader == NULL ) {
			fprintf(stderr, "error: unknown or corrupt image format for '%s'\n", inputFilePath);
			return IMAGECORE_INVALID_FORMAT;
		}

		ImageRGBA* inputImage = ImageRGBA::create(imageReader->getWidth(), imageReader->getHeight());
		SECURE_ASSERT(inputImage != NULL);
		imageReader->readImage(inputImage);

		const uint8_t* buffer = inputImage->getBytes();

		int padding = atoi(args[3]);
		int lutSize = inputImage->getWidth() - padding * 2;
		SECURE_ASSERT(lutSize * (lutSize + padding * 2) == (int)inputImage->getHeight());
		unsigned int inPitch = inputImage->getWidth();

		ImageRGBA* outputImage = ImageRGBA::create(lutSize, lutSize*lutSize);
		SECURE_ASSERT(inputImage != NULL);
		unsigned int outPitch = 0;
		uint8_t* outputBuffer = outputImage->lockRect(outputImage->getWidth(), outputImage->getHeight(), outPitch);
		for( int b = 0; b < lutSize; b++ ) {
			unsigned int inBaseY = b * (lutSize + padding * 2);
			unsigned int outBaseY = b * lutSize;
			for( int g = 0; g < lutSize; g++ ) {
				for( int r = 0; r < lutSize; r++ ) {
					int inY = g + padding + inBaseY;
					int inX = r + padding;
					int inputOffset = (inY * inPitch) + inX * 4;
					int outY = g + outBaseY;
					int outX = r;
					int outputOffset = (outY * outPitch) + outX * 4;
					outputBuffer[outputOffset + 0] = buffer[inputOffset + 0];
					outputBuffer[outputOffset + 1] = buffer[inputOffset + 1];
					outputBuffer[outputOffset + 2] = buffer[inputOffset + 2];
					outputBuffer[outputOffset + 3] = 255;
				}
			}
		}
		ImageWriter::FileStorage outputStorage(outputFile);
		ImageWriter* writer = ImageWriter::createWithFormat(kImageFormat_PNG, &outputStorage);
		writer->writeImage(outputImage);
	}

	fclose(outputFile);

	return IMAGECORE_SUCCESS;
}
