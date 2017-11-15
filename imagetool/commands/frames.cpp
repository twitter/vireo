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

#include "frames.h"
#include "imagecore/utils/mathutils.h"
#include "imagecore/utils/securemath.h"
#include "imagecore/formats/format.h"
#include "imagecore/image/image.h"
#include "imagecore/image/rgba.h"

REGISTER_COMMAND("frames", FramesCommand);

int FramesCommand::run(const char** args, unsigned int numArgs)
{
	if( numArgs < 2 ) {
		fprintf(stderr, "Usage: ImageTool frames <input> <output_pattern>\n");
		fprintf(stderr, "\te.g. ImageTool frames test.gif output/frame%%04d.png\n");
		return IMAGECORE_INVALID_USAGE;
	}

	const char* inputFilePath = args[0];

	ImageReader::Storage* source = ImageReader::FileStorage::open(inputFilePath);
	if( source == NULL ) {
		fprintf(stderr, "error: unable to open input file for '%s'\n", inputFilePath);
		return IMAGECORE_READ_ERROR;
	}

	ImageReader* imageReader = ImageReader::create(source);
	if( imageReader == NULL ) {
		fprintf(stderr, "error: unable to read '%s'\n", inputFilePath);
		delete source;
		return IMAGECORE_INVALID_FORMAT;
	}

	ImageRGBA* frameImage = ImageRGBA::create(imageReader->getWidth(), imageReader->getHeight());

	unsigned int numFrames = imageReader->getNumFrames();
	for( unsigned int i = 0; i < numFrames; i++ ) {
		imageReader->readImage(frameImage);

		// This is clearly bad. Suggestions for how to limit the types of format strings the user can pass in, without
		// having to write my own version of snprintf, are welcome.
		char filename[1024];
		memset(filename, 0, sizeof(filename));
		if( snprintf(filename, 1024, args[1], i) > 512 ) {
			fprintf(stderr, "error: bad output format string\n");
			return IMAGECORE_INVALID_USAGE;
		}

		FILE* outputFile = fopen(filename, "wb");
		if( outputFile == NULL ) {
			fprintf(stderr, "error: unable to open '%s' for writing\n", filename);
			return IMAGECORE_WRITE_ERROR;
		}

		ImageWriter::FileStorage* outputStorage = new ImageWriter::FileStorage(outputFile);
		ImageWriter* writer = ImageWriter::createWithFormat(ImageWriter::formatFromExtension(filename, kImageFormat_PNG), outputStorage);
		writer->setSourceReader(imageReader);

		if( writer == NULL || !writer->writeImage(frameImage) ) {
			fprintf(stderr, "error: unable to write image for '%s'\n", filename);
			return IMAGECORE_WRITE_ERROR;
		}

		fclose(outputFile);
		delete outputStorage;
		delete writer;

		imageReader->advanceFrame();
	}

	delete source;
	delete imageReader;
	delete frameImage;

	return IMAGECORE_SUCCESS;
}
