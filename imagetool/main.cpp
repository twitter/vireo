//
//  main.cpp
//  ImageTool
//
//  Created by Luke Alonso on 10/21/12.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#include "commands/command.h"
#include "version.h"
#include "imagecore/imagecore.h"

int main(int argc, const char* argv[])
{
	if( argc < 2 ) {
		fprintf(stderr, "Usage: ImageTool <identify | resize>\n");
		return -1;
	}
	const char* commandName = argv[1];

	Command* command = Command::createCommand(commandName);
	if( command ) {
		int result = command->run(argv + 2, argc - 2);
		delete command;
		return result;
	} else if( strcmp(commandName, "--version") == 0 ) {
		fprintf(stderr, "ImageTool version %s\n", IMAGETOOL_VERSION);
		return IMAGECORE_SUCCESS;
	} else {
		fprintf(stderr, "error: unknown command '%s'\n", argv[1]);
		return IMAGECORE_INVALID_USAGE;
	}
}

