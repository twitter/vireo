/*
 * MIT License
 *
 * Copyright (c) 2017 Twitter
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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

