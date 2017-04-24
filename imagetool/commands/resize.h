//
//  resize.h
//  ImageTool
//
//  Created by Luke Alonso on 10/26/12.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "command.h"
#include "imagecore/formats/format.h"
#include "imagecore/image/image.h"

class ResizeCommand : public ImageIOCommand
{
public:
	ResizeCommand();
	virtual ~ResizeCommand();
	int run(const char** args, unsigned int numArgs);

private:
	int calcOutputSize();
	int load();
	void resize();
	void rotateCrop();
	int performResize(const char** args, unsigned int numArgs);
};
