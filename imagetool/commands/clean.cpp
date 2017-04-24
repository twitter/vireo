//
//  clean.cpp
//  ImageTool
//
//  Created by Luke Alonso on 4/17/14.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#include "clean.h"

REGISTER_COMMAND("clean", CleanCommand);

CleanCommand::CleanCommand()
:	ImageIOCommand()
{
}

CleanCommand::~CleanCommand()
{

}

int CleanCommand::run(const char** args, unsigned int numArgs)
{
	if ( numArgs < 2 ) {
		fprintf(stderr, "Usage: ImageTool clean <input> <output> [-perfect true|false] [-skiprotate true|false] [-addcolorprofile true|false] [-pad N,N,N]\n");
		fprintf(stderr, "\te.g. ImageTool clean input.jpg output.jpg\n");
		return IMAGECORE_INVALID_USAGE;
	}

	int inputError = open(args[0], args[1]);
	if (inputError != IMAGECORE_SUCCESS) {
		return inputError;
	}

	ImageReader* reader = ImageReader::create(m_Source);
	if( reader == NULL ) {
		fprintf(stderr, "error: unknown or corrupt image format for '%s'\n", m_InputFilePath);
		return IMAGECORE_INVALID_FORMAT;
	}

	EImageFormat outputFormat = ImageWriter::formatFromExtension(args[1], reader->getFormat());

	unsigned int writeOptions = ImageWriter::kWriteOption_CopyColorProfile;

	// Optional args.
	unsigned int numOptional = numArgs - 2;
	if( numOptional > 0 ) {
		unsigned int numPairs = numOptional / 2;
		for( unsigned int i = 0; i < numPairs; i++ ) {
			const char* argName = args[2 + i * 2 + 0];
			const char* argValue = args[2 + i * 2 + 1];
			if( strcmp(argName, "-pad") == 0 ) {
				int ret = populateBuckets(argValue);
				if (ret != IMAGECORE_SUCCESS) {
					return ret;
				}
			} else if( strcmp(argName, "-perfect") == 0 ) {
				if( strcmp(argValue, "true") == 0 ) {
					writeOptions |= ImageWriter::kWriteOption_LosslessPerfect;
				}
			} else if( strcmp(argName, "-skiprotate") == 0 ) {
				if( strcmp(argValue, "true") == 0 ) {
					writeOptions |= ImageWriter::kWriteOption_WriteExifOrientation;
				}
			} else if( strcmp(argName, "-addcolorprofile") == 0 ) {
				if( strcmp(argValue, "true") == 0 ) {
					writeOptions |= ImageWriter::kWriteOption_WriteDefaultColorProfile;
				}
			} else if( strcmp(argName, "-geotag") == 0 ) {
				if( strcmp(argValue, "true") == 0 ) {
					writeOptions |= ImageWriter::kWriteOption_GeoTagData;
				}
			} else if( strcmp(argName, "-progressive") == 0 ) {
				if( strcmp(argValue, "true") == 0 ) {
					writeOptions |= ImageWriter::kWriteOption_Progressive;
				}
			}
		}
	}

	int returnCode = IMAGECORE_UNKNOWN_ERROR;

	ImageWriter* imageWriter = ImageWriter::createWithFormat(outputFormat, m_Output);
	if( imageWriter != NULL ) {
		imageWriter->setWriteOptions(writeOptions);
		if( imageWriter->copyLossless(reader) ) {
			returnCode = IMAGECORE_SUCCESS;
		} else {
			fprintf(stderr, "error: unable to perform lossless copy.\n");
			returnCode = IMAGECORE_INVALID_OPERATION;
		}
		delete imageWriter;
	}

	delete reader;
	reader = NULL;

	if( returnCode != IMAGECORE_SUCCESS ) {
		return returnCode;
	}

	return close();
}
