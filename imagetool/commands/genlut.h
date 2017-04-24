//
//  genlut.h
//  ImageTool
//
//  Created by Luke Alonso on 12/14/12.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "command.h"

class GenLUTCommand : public Command
{
public:
	int run(const char** args, unsigned int numArgs);
};
