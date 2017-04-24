//
//  identify.h
//  ImageTool
//
//  Created by Luke Alonso on 10/28/12.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "command.h"

class IdentifyCommand : public Command
{
public:
	virtual ~IdentifyCommand();
	int run(const char** args, unsigned int numArgs);
};
