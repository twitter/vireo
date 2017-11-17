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

#include "vireo/base_cpp.h"
#include "vireo/common/editbox.h"
#include "vireo/common/math.h"
#include "vireo/error/error.h"

namespace vireo {
namespace common {

auto EditBox::shift(const int64_t offset) const -> EditBox {
  if (offset != 0) {
    THROW_IF(start_pts == EMPTY_EDIT_BOX, InvalidArguments);
  }
  if (offset < 0) {
    THROW_IF(-offset > start_pts, InvalidArguments);
  } else if (offset > 0) {
    THROW_IF(std::numeric_limits<int64_t>::max() - start_pts < offset, Overflow);
  }
  return EditBox(start_pts + offset, duration_pts, rate, type);
};

// Edit boxes are valid if non-overlapping and if exists the only empty edit box is at the beginning
auto EditBox::Valid(const vector<common::EditBox> &edit_boxes) -> bool {
  int first_edit_box = true;
  uint64_t last_end_pts = 0;
  for (const auto& edit_box: edit_boxes) {
    const int64_t start_pts = edit_box.start_pts;
    const uint64_t end_pts = start_pts + edit_box.duration_pts;
    if (start_pts == EMPTY_EDIT_BOX) {
      if (!first_edit_box) {  // empty edit box should be the at the beginning
        return false;
      } else if (edit_boxes.size() == 1) {  // a single empty edit box does not make sense
        return false;
      }
    } else if (start_pts < last_end_pts) {  // edit boxes have to be non-overlapping
      return false;
    } else {
      last_end_pts = end_pts;
    }
    first_edit_box = false;
  }
  return true;
}

// Converts original pts into real pts respecting edit_boxes
auto EditBox::RealPts(const vector<common::EditBox> &edit_boxes, uint64_t pts) -> int64_t {
  bool inside_edit_box = true;
  int64_t new_pts = pts;
  if (!edit_boxes.empty()) {
    inside_edit_box = false;
    new_pts = 0;
    uint64_t last_end_pts = 0;
    for (auto edit_box: edit_boxes) {
      const int64_t start_pts = edit_box.start_pts;
      if (start_pts == EMPTY_EDIT_BOX) {
        THROW_IF(new_pts, Invalid);  // This type of edit box can only be at the very beginning
        new_pts = edit_box.duration_pts;
        continue;
      }
      const uint64_t end_pts = start_pts + edit_box.duration_pts;
      THROW_IF(start_pts < last_end_pts, Invalid);
      last_end_pts = end_pts;
      if (pts >= start_pts && pts < end_pts) {
        new_pts += (pts - edit_box.start_pts);
        inside_edit_box = true;
        break;
      } else if (pts > end_pts) {
        new_pts += edit_box.duration_pts;
      } else {
        break;
      }
    }
  }
  return inside_edit_box ? new_pts : -1;
};

auto EditBox::Plays(const vector<common::EditBox> &edit_boxes, uint64_t pts) -> bool {
  return RealPts(edit_boxes, pts) >= 0;
};

}}