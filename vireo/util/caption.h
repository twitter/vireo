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

vector<decode::ByteRange> get_caption_ranges(const common::Data32& data);

uint32_t copy_caption_payloads_to_caption_data(const common::Data32& data, common::Data32& caption_data, const vector<decode::ByteRange>& caption_ranges, const uint8_t& nalu_length_size);

}}
