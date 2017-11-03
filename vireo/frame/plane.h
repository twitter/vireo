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

#include "vireo/base_h.h"
#include "vireo/common/data.h"
#include "vireo/domain/interval.hpp"

namespace vireo {
namespace frame {

enum Rotation { None = 0, Right = 1, Down = 2, Left = 3 };

class PUBLIC Plane : public domain::Interval<Plane, common::Data16, uint16_t> {
  struct _Plane* _this;
public:
  Plane(uint16_t row, uint16_t width, uint16_t height, common::Data32&& bytes);
  Plane(Plane&& plane);
  virtual ~Plane();
  DISALLOW_COPY_AND_ASSIGN(Plane);
  auto operator()(const uint16_t& y) const -> common::Data16;
  auto row() const -> uint16_t;
  auto width() const -> uint16_t;
  auto height() const -> uint16_t;
  auto alignment() const -> uint8_t;
  auto bytes() const -> common::Data32&;
};

}}
