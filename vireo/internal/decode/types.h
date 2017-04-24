//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"
#include "vireo/common/data.h"

namespace vireo {
namespace internal {
namespace decode {

enum H264NalType { FRM = 1, IDR = 5, SEI = 6, SPS = 7, PPS = 8, AUD = 9, EOS = 10, EOFL = 11, FLLR = 12, Unknown = 255 };

struct RawSample {
  bool keyframe;
  common::Data32 nal;
};

template <typename NalType>
struct NalInfo {
  NalType type;
  uint32_t byte_offset;
  uint32_t size;
  uint8_t start_code_prefix_size;
};
}}}
