//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#include "vireo/sound/sound.h"

namespace vireo {
namespace sound {

auto Sound::shift_pts(const int64_t offset) const -> Sound {
  if (offset < 0) {
    THROW_IF(-offset > pts, InvalidArguments);
  } else if (offset > 0) {
    THROW_IF(std::numeric_limits<uint64_t>::max() - pts < offset, Overflow);
  }
  return (Sound){ pts + offset, pcm };
};

auto Sound::adjust_pts(const vector<common::EditBox> &edit_boxes) const -> Sound {
  int64_t new_pts = common::EditBox::RealPts(edit_boxes, pts);
  CHECK(new_pts >= 0);
  return (sound::Sound){ new_pts, pcm };
};

}}