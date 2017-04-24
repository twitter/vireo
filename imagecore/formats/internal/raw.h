//
//  raw.h
//  ImageTool
//
//  Created by Luke Alonso on 01/15/14.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "imagecore/formats/writer.h"
#include "register.h"

namespace imagecore {

class ImageWriterRAW : public ImageWriter
{
public:
	DECLARE_IMAGE_WRITER(ImageWriterRAW);

	virtual bool writeImage(Image* sourceImage);

	virtual bool beginWrite(unsigned int width, unsigned int height, EImageColorModel colorModel);
	virtual unsigned int writeRows(Image* sourceImage, unsigned int sourceRow, unsigned int numRows);

	virtual bool endWrite();

private:
	virtual bool initWithStorage(Storage* output);
	Storage* m_Storage;
};

}
