//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"
#include "vireo/decode/types.h"
#include "vireo/frame/frame.h"
#include "vireo/functional/media.hpp"

namespace vireo {
namespace decode {

class PUBLIC Video final : public functional::DirectVideo<Video, frame::Frame> {
  std::shared_ptr<struct _Video> _this;
public:
  Video(const functional::Video<Sample>& track, uint32_t thread_count = 0);
  Video(const Video& video);
  DISALLOW_ASSIGN(Video);
  auto operator()(uint32_t index) const -> frame::Frame;
};

}}
