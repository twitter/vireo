//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

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
