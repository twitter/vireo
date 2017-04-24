//
//  command.h
//  ImageTool
//
//  Created by Luke Alonso on 10/26/12.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "imagecore/formats/reader.h"
#include "imagecore/formats/writer.h"

#define REGISTER_COMMAND(n, c) \
void* __create##c() { \
	return new c(); \
} \
static int __register##c = Command::registerCommand(n, __create##c);

using namespace imagecore;

typedef void*(*commandCreateFunc)();

class Command
{
public:
	virtual ~Command() { }
	virtual int run(const char** args, unsigned int numArgs) = 0;

	static int registerCommand(const char* name, commandCreateFunc createFunc);
	static Command* createCommand(const char* name);
};

#define MAX_BUCKETS 128

class ImageIOCommand : public Command
{
public:
	ImageIOCommand();
	virtual ~ImageIOCommand();

	static int defineBucket(const char* name, unsigned int buckets[]);
protected:
	int open(const char* inputFilePath, const char* ouputFilePath);
	int close();
	void reset();

	uint64_t getPadAmount(uint64_t inputSize);
	int padFile(ImageWriter::Storage* output);
	int populateBuckets(const char* padArgValue);

	char* m_InputFilePath;
	char* m_OutputFilePath;
	unsigned int m_PadBuckets[MAX_BUCKETS];
	int m_PadBucketCount;

	ImageReader::Storage* m_Source;
	ImageWriter::Storage* m_Output;
};
