//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"
#include "vireo/common/data.h"
#include "vireo/decode/types.h"
#include "vireo/header/header.h"
#include "vireo/types.h"

namespace vireo {
namespace encode {

struct Sample {
  Sample(const decode::Sample& sample) : Sample(sample.pts, sample.dts, sample.keyframe, sample.type, sample.nal()) {}
  Sample(int64_t pts, int64_t dts, uint64_t keyframe, SampleType type, const common::Data32& nal)
    : pts(pts), dts(dts), keyframe(keyframe), type(type), nal(nal) {}
  int64_t pts;
  int64_t dts;
  bool keyframe;
  SampleType type;
  common::Data32 nal;
  static inline auto Convert(const decode::Sample& sample) -> Sample {
    return Sample(sample);
  };
};

}}
