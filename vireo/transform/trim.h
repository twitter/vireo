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
#include "vireo/common/editbox.h"
#include "vireo/decode/types.h"
#include "vireo/functional/media.hpp"

namespace vireo {
namespace transform {

template <int Type>
class PUBLIC Trim final {
  std::shared_ptr<struct _Trim> _this = nullptr;
public:
  Trim(const functional::Media<functional::Function<decode::Sample, uint32_t>, decode::Sample, uint32_t, Type>& track, vector<common::EditBox> edit_boxes, const uint64_t start_ms, uint64_t duration_ms);
  Trim(const functional::Media<functional::Function<decode::Sample, uint32_t>, decode::Sample, uint32_t, Type>& track, const uint64_t start_ms, uint64_t duration_ms) : Trim(track, vector<common::EditBox>(), start_ms, duration_ms) {}
  Trim(const Trim& trim);
  DISALLOW_ASSIGN(Trim<Type>);

  class Track final : public functional::Media<Track, decode::Sample, uint32_t, Type> {
    std::shared_ptr<_Trim> _this;
    Track(const std::shared_ptr<_Trim>& _this);
    friend class Trim;
  public:
    Track(const Track& track);
    DISALLOW_ASSIGN(Track);
    auto duration() const -> uint64_t;
    auto edit_boxes() const -> const vector<common::EditBox>&;
    auto operator()(uint32_t index) const -> decode::Sample;
  } track;
};

}}
