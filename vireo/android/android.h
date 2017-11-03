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
