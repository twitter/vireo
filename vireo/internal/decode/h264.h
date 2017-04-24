//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"
#include "vireo/decode/types.h"
#include "vireo/frame/frame.h"
#include "vireo/functional/media.hpp"

namespace vireo {
namespace internal {
namespace decode {

using namespace vireo::decode;

class PUBLIC H264 final : public functional::DirectVideo<H264, frame::Frame> {
  std::shared_ptr<struct _H264> _this;
public:
  H264(const functional::Video<Sample>& track, uint32_t thread_count = 0);
  H264(const H264& h264);
  DISALLOW_ASSIGN(H264);
  auto operator()(uint32_t index) const -> frame::Frame;
};

}}}
