//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include <vector>

#include "vireo/base_h.h"
#include "vireo/common/data.h"
#include "vireo/decode/types.h"
#include "vireo/internal/decode/h264_bytestream.h"
#include "vireo/internal/demux/mp4.h"
#include "vireo/mux/mp4.h"

namespace vireo {
namespace android {

struct FrameInterval {
  uint32_t start_index;
  uint32_t num_frames;
};

PUBLIC auto frame_intervals(internal::demux::MP4& mp4_decoder) -> const vector<FrameInterval>;
PUBLIC auto mux(internal::demux::MP4& mp4_decoder, internal::decode::H264_BYTESTREAM& h264_bytestream_decoder, int fps_factor = 1, int width = 0, int height = 0) -> mux::MP4;
PUBLIC auto stitch(std::vector<std::function<common::Data32(void)>> movies, bool disable_audio = false) -> mux::MP4;
PUBLIC auto trim(internal::demux::MP4& mp4_decoder, uint64_t start_ms, uint64_t duration_ms, bool respect_input_edit_boxes = true) -> mux::MP4;

}}