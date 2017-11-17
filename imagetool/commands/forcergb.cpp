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

#include "forcergb.h"
#include "imagecore/utils/mathutils.h"
#include "imagecore/utils/securemath.h"
#include "imagecore/formats/format.h"
#include "imagecore/image/rgba.h"
#include "lcms2.h"

REGISTER_COMMAND("forcergb", ForceRGBCommand);

ForceRGBCommand::ForceRGBCommand()
:	ImageIOCommand()
{
}

ForceRGBCommand::~ForceRGBCommand()
{
}

int ForceRGBCommand::run(const char** args, unsigned int numArgs)
{
	if ( numArgs < 2 ) {
		fprintf(stderr, "Usage: ImageTool forcergb <input> <output> [-filequality 0-100] [-pad N,N,N]\n");
		fprintf(stderr, "\te.g. ImageTool forcergb input.jpg output.jpg\n");
		return IMAGECORE_INVALID_USAGE;
	}

	int ret = open(args[0], args[1]);
	if (ret != IMAGECORE_SUCCESS) {
		return ret;
	}

	// Defaults
	unsigned int compressionQuality = 75;

	// Optional args
	unsigned int numOptional = numArgs - 2;
	if ( numOptional > 0 ) {
		unsigned int numPairs = numOptional / 2;
		for ( unsigned int i = 0; i < numPairs; i++ ) {
			const char* argName = args[2 + i * 2 + 0];
			const char* argValue = args[2 + i * 2 + 1];
			if( strcmp(argName, "-filequality") == 0 ) {
				compressionQuality = clamp(0, 100, atoi(argValue));
			} else if( strcmp(argName, "-pad") == 0 ) {
				int ret = populateBuckets(argValue);
				if (ret != IMAGECORE_SUCCESS) {
					return ret;
				}
			}
		}
	}


	ImageReader* reader = ImageReader::create(m_Source);
	if( reader == NULL ) {
		fprintf(stderr, "error: unknown or corrupt image format for '%s'\n", m_InputFilePath);
		return IMAGECORE_INVALID_FORMAT;
	}

	EImageFormat outputFormat = ImageWriter::formatFromExtension(args[1], reader->getFormat());

	unsigned int colorProfileSize = 0;
	reader->getColorProfile(colorProfileSize);
	if( colorProfileSize != 0 && reader->getFormat() == kImageFormat_JPEG ) {
		reader->setReadOptions(ImageReader::kReadOption_ApplyColorProfile);
		ImageRGBA* image = ImageRGBA::create(reader->getWidth(), reader->getHeight());
		if( reader->readImage(image) ) {
			ImageWriter* writer = ImageWriter::createWithFormat(kImageFormat_JPEG, m_Output);
			if (writer == NULL) {
				fprintf(stderr, "error: unable to create ImageWriter\n");
				return IMAGECORE_OUT_OF_MEMORY;
			}
			writer->setWriteOptions(ImageWriter::kWriteOption_WriteDefaultColorProfile);
			writer->setSourceReader(reader);
			writer->setQuality(compressionQuality);
			if( !writer->writeImage(image) ) {
				ret = IMAGECORE_WRITE_ERROR;
			}
			delete writer;
		} else {
			fprintf(stderr, "error unable to read input image");
			ret = IMAGECORE_READ_ERROR;
		}
		delete image;
	} else {
		ImageWriter* imageWriter = ImageWriter::createWithFormat(outputFormat, m_Output);
		unsigned int writeOptions = 0;
		writeOptions |= ImageWriter::kWriteOption_WriteExifOrientation;
		writeOptions |= ImageWriter::kWriteOption_WriteDefaultColorProfile;
		if( imageWriter != NULL ) {
			imageWriter->setWriteOptions(writeOptions);
			if( imageWriter->copyLossless(reader) ) {
				ret = IMAGECORE_SUCCESS;
			} else {
				fprintf(stderr, "error: unable to perform lossless copy.\n");
				ret = IMAGECORE_INVALID_OPERATION;
			}
			delete imageWriter;
		}
	}

	delete reader;
	reader = NULL;

	close();

	return ret;
}
