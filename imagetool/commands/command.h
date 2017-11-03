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
