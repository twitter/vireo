//
//  tiledresize.h
//  ImageCore
//
//  Created by Luke Alonso on 4/3/14.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#include "imagecore/formats/reader.h"
#include "imagecore/formats/writer.h"

namespace imagecore {

class TiledResizeOperation
{
public:
	TiledResizeOperation(ImageReader* imageReader, ImageWriter* imageWriter, unsigned int outputWidth, unsigned int outputHeight);
	~TiledResizeOperation();

	void setResizeQuality(EResizeQuality quality)
	{
		m_ResizeQuality = quality;
	}

	int performResize();

private:
	ImageReader* m_ImageReader;
	ImageWriter* m_ImageWriter;
	unsigned int m_OutputWidth;
	unsigned int m_OutputHeight;
	EResizeQuality m_ResizeQuality;
};

}
