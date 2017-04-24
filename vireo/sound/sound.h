//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"
#include "vireo/common/editbox.h"
#include "vireo/sound/pcm.h"

namespace vireo {
namespace sound {

struct Sound {
  int64_t pts;
  std::function<PCM(void)> pcm;
  auto shift_pts(const int64_t offset) const -> Sound;
  auto adjust_pts(const vector<common::EditBox> &edit_boxes) const -> Sound;
};

}}
