//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

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
