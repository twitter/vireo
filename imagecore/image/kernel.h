//
//  kernel.h
//  ImageTool
//
//  Created by Luke Alonso on 10/24/12.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "imagecore/imagecore.h"

enum EFilterType
{
	kFilterType_Lanczos				= 0,
	kFilterType_LanczosSharper		= 1,
	kFilterType_MitchellNetravali	= 2,
	kFilterType_Kaiser = 3,
	kFilterType_Linear = 4,
	kFilterType_MAX
};

class FilterKernel
{
public:
	FilterKernel();
	~FilterKernel();

	float* getTable() const
	{
		return m_Table + (m_OutSampleOffset * m_KernelSize);
	}

	uint8_t* getTableFixedPointBilinear() const
	{
		return m_TableBilinear + (m_OutSampleOffset * m_KernelSize);
	}

	int32_t* getTableFixedPoint() const
	{
		return m_TableFixedPoint + (m_OutSampleOffset * m_KernelSize);
	}

	int32_t* getTableFixedPoint4() const
	{
		return m_TableFixedPoint4 + (m_OutSampleOffset * 4 * m_KernelSize);
	}

	unsigned int getTableSize() const
	{
		return m_TableSize;
	}

	unsigned int getKernelSize() const
	{
		return m_KernelSize;
	}

	float getSampleRatio() const
	{
		return m_SampleRatio;
	}

	float getMaxSamples() const
	{
		return m_MaxSamples;
	}

	void setSampleOffset(unsigned int inSampleOffset, unsigned int outSampleOffset);

protected:
	void generateFixedPoint(EFilterType type);
	unsigned int m_InSampleOffset;
	unsigned int m_OutSampleOffset;
	unsigned int m_KernelSize;
	unsigned int m_TableSize;
	unsigned int m_MaxSamples;
	float m_WindowWidth;
	float m_SampleRatio;
	float* m_Table;
	uint8_t* m_TableBilinear;
	int32_t* m_TableFixedPoint;
	int32_t* m_TableFixedPoint4;
};

class FilterKernelAdaptive : public FilterKernel
{
public:
	FilterKernelAdaptive(EFilterType type, unsigned int kernelSize, unsigned int inSize, unsigned int outSize);

	int computeSampleStart(int outPosition) const
	{
		float sample = ((outPosition + m_OutSampleOffset) + 0.5f) * m_SampleRatio - 0.00001f;
		return (int)(sample - m_WindowWidth + 0.5f) - m_InSampleOffset;
	}
};

class FilterKernelFixed : public FilterKernel
{
public:
	FilterKernelFixed(EFilterType type, unsigned int inSize, unsigned int outSize);

	int computeSampleStart(int outPosition) const
	{
		return (int)floorf(fmaxf(0.0f, ((float)(outPosition + 0.5f) * m_SampleRatio) - 0.5f));
	}
};
