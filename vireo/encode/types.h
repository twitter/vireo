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
