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