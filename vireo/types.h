//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

namespace vireo {

enum SampleType { Unknown = 0, Video = 1, Audio = 2, Data = 3, Caption = 4 };

enum FileType { UnknownFileType = 0, MP4 = 1, MP2TS = 2, WebM = 3, Image = 4 };
const static char* kFileTypeToString[] = { "Unknown file type", "MP4", "MP2TS", "WebM", "Image" };

enum FileFormat { Regular = 0, DashInitializer = 1, DashData = 2, HeaderOnly = 3, SamplesOnly = 4 };

}
