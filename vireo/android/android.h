//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// Error values chosen to avoid collision with errno.h
#define ERR_EXCEPTION         128
#define ERR_NO_AUDIO_OR_VIDEO 129
#define ERR_SETTING_MISMATCH  130
#define ERR_INVALID_ARGUMENTS 131
#define ERR_FILE_NOT_FOUND    132
#define ERR_ASSERTION_FAIL    133

extern bool CanTrim(int in_fd);
extern int Trim(int in_fd, int out_fd, long start_ms, long duration_ms);
extern int GetFrameInterval(int in_fd, int* interval);
extern int Stitch(int* in_fds, int count, int out_fd);
extern int Mux(int in_mp4_fd, int in_h264_bytestream_fd, int out_fd, int fps_factor = 1, int width = 0, int height = 0);

#ifdef __cplusplus
}
#endif /* __cplusplus */
