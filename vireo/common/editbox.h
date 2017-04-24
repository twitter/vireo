//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

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
