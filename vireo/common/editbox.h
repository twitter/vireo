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

#include <vector>

#include "vireo/base_h.h"
#include "vireo/types.h"

#define EMPTY_EDIT_BOX -1

namespace vireo {
namespace common {

using namespace std;

struct EditBox {
  int64_t start_pts;
  uint64_t duration_pts;
  float rate;
  SampleType type;
  EditBox(int64_t start_pts, uint64_t duration_pts, float rate, SampleType type) : start_pts(start_pts), duration_pts(duration_pts), rate(rate), type(type) {};
  auto shift(const int64_t offset) const -> EditBox;
  static auto Valid(const vector<common::EditBox> &edit_boxes) -> bool;
  static auto RealPts(const vector<common::EditBox> &edit_boxes, uint64_t pts) -> int64_t;
  static auto Plays(const vector<common::EditBox> &edit_boxes, uint64_t pts) -> bool;
};

}}
