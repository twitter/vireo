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