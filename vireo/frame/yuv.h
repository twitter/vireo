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
#include "vireo/dependency.hpp"
#include "vireo/domain/interval.hpp"
#include "vireo/frame/plane.h"

namespace vireo {
namespace frame {

class RGB;
enum PlaneIndex { Y = 0, U = 1, V = 2 };

class PUBLIC YUV : public domain::Interval<YUV, std::function<common::Data16(PlaneIndex)>, uint16_t> {
  struct _YUV* _this;
public:
  YUV(uint16_t width, uint16_t height, uint8_t uv_x_ratio, uint8_t uv_y_ratio, bool full_range = true);
  YUV(Plane&& y, Plane&& u, Plane&& v, bool full_range = true);
  YUV(YUV&& yuv);
  YUV(const YUV& yuv);
  virtual ~YUV();
  DISALLOW_ASSIGN(YUV);
  auto operator()(uint16_t y) const -> std::function<common::Data16(PlaneIndex)>;
  auto width() const -> uint16_t;
  auto height() const -> uint16_t;
  auto uv_ratio() const -> std::pair<uint8_t, uint8_t>;
  auto full_range() const -> bool;
  auto plane(PlaneIndex index) const -> const Plane&;

  // Transforms
  template <typename Available = has_swscale>
  auto rgb(uint8_t component_count) -> RGB;
  auto full_range(bool full_range) -> YUV;
  auto crop(uint16_t x_offset, uint16_t y_offset, uint16_t width, uint16_t height) const -> YUV;
  auto rotate(Rotation direction) -> YUV;
  auto scale(int num, int denum) -> YUV { return stretch(num, denum, num, denum); }
  auto stretch(int num_x, int denum_x, int num_y, int denum_y, bool high_quality = true) -> YUV;
};

}}
