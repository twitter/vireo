//
//  identify.cpp
//  ImageTool
//
//  Created by Luke Alonso on 10/28/12.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#include "identify.h"
#include "imagecore/utils/mathutils.h"
#include "imagecore/formats/format.h"
#include "imagecore/image/image.h"
#include "imagecore/image/rgba.h"

REGISTER_COMMAND("identify", IdentifyCommand);

IdentifyCommand::~IdentifyCommand()
{

}

int IdentifyCommand::run(const char** args, unsigned int numArgs)
{
	if( numArgs < 1 ) {
		fprintf(stderr, "Usage: ImageTool identify <input> [scan alpha for transparency]\n");
		fprintf(stderr, "\te.g. ImageTool identify input.jpg\n");
		fprintf(stderr, "\te.g. ImageTool identify input.jpg false\n");
		fprintf(stderr, "\te.g. ImageTool identify input.jpg true\n");
		return IMAGECORE_INVALID_USAGE;
	}

	bool scanAlpha = false;

	const char* inputFilePath = args[0];
	if( numArgs >= 2 )
	{
		if( strcmp(args[1], "true") == 0 )
		{
			scanAlpha = true;
		}
	}

	ImageReader::FileStorage* source = ImageReader::FileStorage::open(inputFilePath);
	if( source == NULL ) {
		fprintf(stderr, "error: unable to open input file for '%s'\n", inputFilePath);
		return IMAGECORE_READ_ERROR;
	}

	ImageReader* imageReader = ImageReader::create(source);
	if( imageReader == NULL ) {
		fprintf(stderr, "error: unknown or corrupt image format for '%s'\n", inputFilePath);
		delete source;
		return IMAGECORE_INVALID_FORMAT;
	}

	unsigned int width = imageReader->getOrientedWidth();
	unsigned int height = imageReader->getOrientedHeight();

	bool isRGBA = imageReader->getNativeColorModel() == kColorModel_RGBA;
	int hasTransparency = isRGBA ? -1 : 0;  // -1: unknown, 0: no, 1: yes
	if( hasTransparency < 0 && scanAlpha ) {
		hasTransparency = 0;
		ImageRGBA* frameImage = ImageRGBA::create(imageReader->getWidth(), imageReader->getHeight(), true);
		unsigned int numFrames = imageReader->getNumFrames();
		for( unsigned int i = 0; i < numFrames; i++ ) {
			if( !imageReader->readImage(frameImage) ) {
				fprintf(stderr, "error: unable to decode image '%s'\n", inputFilePath);
				return IMAGECORE_INVALID_FORMAT;
			}
			if( frameImage->scanAlpha() )
			{
				hasTransparency = 1;
				break;
			}
			imageReader->advanceFrame();
		}
		delete frameImage;
	}
	const char* transparencyInfo = hasTransparency < 0 ? "unknown" : (hasTransparency ? "yes" : "no");

	fprintf(stdout, "%s format:%s dimensions:%ix%i num_frames:%d transparency:%s\n", inputFilePath, imageReader->getFormatName(), width, height, imageReader->getNumFrames(), transparencyInfo);

	delete imageReader;
	delete source;

	return IMAGECORE_SUCCESS;
}

