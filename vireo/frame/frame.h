//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/common/editbox.h"
#include "vireo/frame/rgb.h"
#include "vireo/frame/yuv.h"

namespace vireo {
namespace frame {

struct Frame {
  int64_t pts;
  std::function<YUV(void)> yuv;
  std::function<RGB(void)> rgb;
  auto shift_pts(const int64_t offset) const -> Frame;
  auto adjust_pts(const vector<common::EditBox> &edit_boxes) const -> Frame;
};

}}
