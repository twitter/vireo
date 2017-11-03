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
#include "vireo/encode/types.h"
#include "vireo/frame/frame.h"
#include "vireo/functional/media.hpp"

namespace vireo {
namespace encode {

static const int kVP8MinQuantizer = 5;
static const int kVP8MaxQuantizer = 68;
static const int kVP8MinOptimization = 0;
static const int kVP8MaxOptimization = 2;

class PUBLIC VP8 final : public functional::DirectVideo<VP8, Sample> {
  std::shared_ptr<struct _VP8> _this;
public:
  VP8(const functional::Video<frame::Frame>& frames, int quantizer, int optimization, float fps, int max_bitrate = 0);
  VP8(const VP8& vp8);
  DISALLOW_ASSIGN(VP8);
  auto operator()(uint32_t sample) const -> Sample;
};

}}
