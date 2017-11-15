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

