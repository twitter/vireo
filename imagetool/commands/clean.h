//
//  clean.h
//  ImageTool
//
//  Created by Luke Alonso on 4/17/14.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "command.h"

class CleanCommand : public ImageIOCommand
{
public:
	CleanCommand();
	virtual ~CleanCommand();
	int run(const char** args, unsigned int numArgs);
};
