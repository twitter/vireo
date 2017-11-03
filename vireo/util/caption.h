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

#include "vireo/base_h.h"
#include "vireo/common/data.h"
#include "vireo/decode/types.h"

namespace vireo {
namespace util {

struct PtsIndexPair {
  uint64_t pts;
  uint32_t index;
  PtsIndexPair(uint64_t pts, uint32_t index) : pts(pts), index(index) {};
  bool operator<(const PtsIndexPair& pts_index) const {
    return pts < pts_index.pts;
  }
};

struct CaptionPayloadInfo {
  vector<decode::ByteRange> byte_ranges;
  bool valid;
};

class CaptionHandler {
public:
  static CaptionPayloadInfo ParsePayloadInfo(const common::Data32& sei_data);
  static uint32_t CopyPayloadsIntoData(const common::Data32& sei_data,
                                       const CaptionPayloadInfo& info,
                                       const uint8_t& nalu_length_size,
                                       common::Data32& out_data);
};

}}
