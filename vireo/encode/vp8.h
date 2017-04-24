//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

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
