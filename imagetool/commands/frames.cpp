//
//  frames.cpp
//  ImageTool
//
//  Created by Luke Alonso on 11/19/12.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

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
