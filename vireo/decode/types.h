//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"
#include "vireo/common/data.h"
#include "vireo/domain/interval.hpp"
#include "vireo/frame/rgb.h"
#include "vireo/frame/yuv.h"
#include "vireo/functional/media.hpp"
#include "vireo/sound/pcm.h"
#include "vireo/types.h"

namespace vireo {
namespace decode {

struct ByteRange {
  ByteRange() : available(false), pos(0), size(0) {}
  ByteRange(const ByteRange& byte_range) : available(byte_range.available), pos(byte_range.pos), size(byte_range.size) {}
  ByteRange(uint32_t pos, uint32_t size) : available(true), pos(pos), size(size) {}
  bool available;
  uint32_t pos;
  uint32_t size;
};

struct Sample {
  Sample(int64_t pts, int64_t dts, bool keyframe, SampleType type, const std::function<common::Data32(void)>& nal, uint32_t pos, uint32_t size)
    : pts(pts), dts(dts), keyframe(keyframe), type(type), nal(nal), byte_range(ByteRange(pos, size)) {}
  Sample(int64_t pts, int64_t dts, bool keyframe, SampleType type, const std::function<common::Data32(void)>& nal)
    : pts(pts), dts(dts), keyframe(keyframe), type(type), nal(nal) {}
  Sample(const Sample& sample, int64_t new_pts, int64_t new_dts) : pts(new_pts), dts(new_dts), keyframe(sample.keyframe), type(sample.type), nal(sample.nal), byte_range(sample.byte_range) {}
  int64_t pts;
  int64_t dts;
  bool keyframe;
  SampleType type;
  ByteRange byte_range;
  std::function<common::Data32(void)> nal;
  auto operator==(const Sample& sample) const -> bool {
    // this serves as a lightweight comparison without checking the actual payload
    if (pts != sample.pts || dts != sample.dts || keyframe != sample.keyframe || type != sample.type) {
      return false;
    }
    return true;
  };
  auto operator!=(const Sample& sample) const -> bool {
    return !(*this == sample);
  }
  inline auto shift(const int64_t offset) const -> Sample {
    if (offset < 0) {
      THROW_IF(-offset > pts, InvalidArguments);
      THROW_IF(-offset > dts, InvalidArguments);
    } else if (offset > 0) {
      THROW_IF(std::numeric_limits<int64_t>::max() - pts < offset, Overflow);
      THROW_IF(std::numeric_limits<int64_t>::max() - dts < offset, Overflow);
    }
    return Sample(pts + offset, dts + offset, keyframe, type, nal);
  };
};

}}
