//
//  pad.h
//  ImageTool
//
//  Created by Robert Sayre  on 6/26/13.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "command.h"

class PadCommand : public ImageIOCommand
{
public:
	PadCommand();
	virtual ~PadCommand();
	int run(const char** args, unsigned int numArgs);

};
