//
//  pad.cpp
//  ImageTool
//
//  Created by Robert Sayre  on 6/26/13.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#include "pad.h"
#include "imagecore/formats/reader.h"
#include "imagecore/formats/writer.h"

REGISTER_COMMAND("pad", PadCommand);

PadCommand::PadCommand()
:	ImageIOCommand()
{
}

PadCommand::~PadCommand()
{
}

int PadCommand::run(const char** args, unsigned int numArgs)
{
	if ( numArgs < 3 ) {
		fprintf(stderr, "Usage: ImageTool pad <input> <output> <buckets>\n");
		fprintf(stderr, "\te.g. ImageTool pad input.jpg output.jpg 1234,4567,5678\n");
		return IMAGECORE_INVALID_USAGE;
	}

	m_InputFilePath = strdup(args[0]);
	m_OutputFilePath = strdup(args[1]);

	int ret = populateBuckets(args[2]);
	if (ret != IMAGECORE_SUCCESS) {
		return ret;
	}

	ImageReader::Storage* input = ImageReader::FileStorage::open(m_InputFilePath);
	ImageWriter::Storage* output = ImageWriter::FileStorage::open(m_OutputFilePath);

	output->writeStream(input);

	padFile(output);

	delete input;
	delete output;

	return IMAGECORE_SUCCESS;
}

