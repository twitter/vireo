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

#pragma once

#define IMAGE_ROW_DEFAULT_ALIGNMENT_SHIFT 5
#define IMAGE_ROW_DEFAULT_ALIGNMENT (1 << IMAGE_ROW_DEFAULT_ALIGNMENT_SHIFT)

#define AUDIO_FRAME_SIZE 1024
#define SBR_FACTOR 2

#define MP2TS_SYNC_BYTE 0x47
#define MP2TS_PACKET_LENGTH 188
// Max allowed difference between predicted and actual pts, measured on 90 kHz clock.
#define MP2TS_PTS_ALLOWED_DRIFT 10

#include <vector>
static const std::vector<uint32_t> kSampleRate = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350 };

const static uint64_t kMilliSecondScale = 1000;
const static uint64_t kMicroSecondScale = 1000000;
const static uint64_t kNanoSecondScale = 1000000000;

const static uint32_t kMP2TSTimescale = 90000;  // default timescale for MPEG-TS
