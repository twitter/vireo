//
//  force_rgb.h
//  ImageTool
//
//  Created by Robert Sayre  on 4/9/13.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "command.h"
#include "imagecore/formats/format.h"
#include "jpeglib.h"

class ForceRGBCommand : public ImageIOCommand
{
public:
	ForceRGBCommand();
	virtual ~ForceRGBCommand();
	int run(const char** args, unsigned int numArgs);

private:
	int forceRGB(unsigned int compressionQuality);
	uint8_t* stdinBuffer;
	unsigned int stdinSize;
};
