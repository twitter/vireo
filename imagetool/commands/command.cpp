//
//  command.cpp
//  ImageTool
//
//  Created by Luke Alonso on 10/26/12.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#include "command.h"
#include "imagecore/utils/securemath.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define MAX_COMMANDS 128

static const char* s_CommandNames[MAX_COMMANDS];
static commandCreateFunc s_CommandCreateFuncs[MAX_COMMANDS];
static unsigned int s_CommandCount = 0;

int Command::registerCommand(const char* name, commandCreateFunc createFunc)
{
	if( s_CommandCount < MAX_COMMANDS ) {
		s_CommandNames[s_CommandCount] = name;
		s_CommandCreateFuncs[s_CommandCount] = createFunc;
		s_CommandCount++;
	}
	return s_CommandCount;
}

Command* Command::createCommand(const char* name)
{
	for( unsigned int i = 0; i < s_CommandCount; i++ ) {
		if( strcmp(name, s_CommandNames[i]) == 0 ) {
			return (Command*)(s_CommandCreateFuncs[i]());
		}
	}
	return NULL;
}

ImageIOCommand::ImageIOCommand()
:	Command()
,	m_InputFilePath(NULL)
,	m_OutputFilePath(NULL)
,	m_PadBucketCount(0)
,	m_Source(NULL)
,	m_Output(NULL)
{
}

ImageIOCommand::~ImageIOCommand()
{
	reset();
}

void ImageIOCommand::reset()
{
	m_PadBucketCount = 0;
	free(m_InputFilePath);
	m_InputFilePath = NULL;
	free(m_OutputFilePath);
	m_OutputFilePath = NULL;
	delete m_Source;
	m_Source = NULL;
	delete m_Output;
	m_Output = NULL;
}

int ImageIOCommand::open(const char* inputFilePath, const char* outputFilePath)
{
	reset();
	m_InputFilePath = strdup(inputFilePath);
	m_Source = ImageReader::FileStorage::open(m_InputFilePath);
	if( m_Source == NULL ) {
		fprintf(stderr, "error: unable to open input file for '%s'\n", m_InputFilePath);
		return IMAGECORE_READ_ERROR;
	}

	m_OutputFilePath = strdup(outputFilePath);
	m_Output = ImageWriter::FileStorage::open(m_OutputFilePath);
	if( m_Output == NULL ) {
		fprintf(stderr, "error: unable to open output stream\n");
		return IMAGECORE_WRITE_ERROR;
	}

	return IMAGECORE_SUCCESS;
}

uint64_t ImageIOCommand::getPadAmount(uint64_t inputSize)
{
	if( m_PadBucketCount == 0 ) {
		return 0;
	}

	if( inputSize < m_PadBuckets[0] ) {
		return m_PadBuckets[0] - inputSize;
	}

	for(int i = 0; i < m_PadBucketCount - 1; i++) {
		if( inputSize > m_PadBuckets[i] && inputSize < m_PadBuckets[i + 1] ) {
			return m_PadBuckets[i + 1] - inputSize;
		}
	}

	if( m_PadBucketCount > 1 && inputSize > m_PadBuckets[m_PadBucketCount - 1] ) {
		uint64_t delta = m_PadBuckets[m_PadBucketCount - 1] - m_PadBuckets[m_PadBucketCount - 2];
		uint64_t deltaCount = (inputSize - m_PadBuckets[m_PadBucketCount - 1]) / delta;
		if( inputSize > m_PadBuckets[m_PadBucketCount - 1] + delta * deltaCount ) {
			return m_PadBuckets[m_PadBucketCount - 1] + delta * (deltaCount + 1) - inputSize;
		}
	}

	return 0;
}

int ImageIOCommand::padFile(ImageWriter::Storage* outFile)
{
	FILE* outputFile;
	if( outFile->asFile(outputFile) ) {
		uint64_t bytesWritten = outFile->totalBytesWritten();
		uint64_t padAmount = getPadAmount(bytesWritten);
		if( padAmount > 0 ) {
			uint8_t* trailer = (uint8_t*)malloc(padAmount);
			if( !trailer ) {
				fprintf(stderr, "error: couldn't allocate pad buffer.\n");
				return IMAGECORE_OUT_OF_MEMORY;
			} else {
				memset(trailer, ' ', padAmount);
				fwrite(trailer, padAmount, 1, outputFile);
			}
			free(trailer);
			if( ferror(outputFile) != 0 ) {
				fprintf(stderr, "error: file had error %i\n", ferror(outputFile));
				return IMAGECORE_WRITE_ERROR;
			}
		}
	}

	return IMAGECORE_SUCCESS;
}

int compare(const void* a, const void* b)
{
	return ( *(int*)a - *(int*)b );
}

int ImageIOCommand::populateBuckets(const char* padArgValue)
{
	int count = 0;
	const char delims[] = ",";
	char* bucketList = strdup(padArgValue);
	char* bucket = strtok(bucketList, delims);

	while ( bucket != NULL && count < MAX_BUCKETS ) {
		unsigned int size = atoi(bucket);
		if ( size == 0 ) {
			fprintf(stderr, "error: invalid pad size '%s'", bucket);
			m_PadBucketCount = 0;
			free(bucketList);
			return IMAGECORE_INVALID_USAGE;
		}
		m_PadBuckets[m_PadBucketCount] = size;
		m_PadBucketCount++;
		bucket = strtok( NULL, delims );
	}

	qsort(m_PadBuckets, m_PadBucketCount, sizeof(unsigned int), compare);

	free(bucketList);

	return IMAGECORE_SUCCESS;
}

int ImageIOCommand::close()
{
	int ret = IMAGECORE_SUCCESS;
	if( m_PadBucketCount > 0 ) {
		ret = padFile(m_Output);
	}

	if( ret != IMAGECORE_SUCCESS ) {
		fprintf(stderr, "error: failed writing trailing padding data.\n");
		ret = IMAGECORE_WRITE_ERROR;
	}

	reset();

	return ret;
}

