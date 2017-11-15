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

class YUV;

class PUBLIC RGB {
  struct _RGB* _this;
public:
  RGB(uint16_t width, uint16_t height, uint8_t component_count);
  RGB(uint8_t component_count, Plane&& plane);
  RGB(RGB&& rgb);
  RGB(const RGB& rgb);
  virtual ~RGB();
  DISALLOW_ASSIGN(RGB);
  auto width() const -> uint16_t;
  auto height() const -> uint16_t;
  auto component_count() const -> uint8_t;
  auto alignment() const -> uint8_t;
  auto plane() const -> const Plane&;

  // Transforms
  template <typename Available = has_swscale>
  auto rgb(uint8_t component_count) const -> RGB;
  template <typename Available = has_swscale>
  auto yuv(uint8_t uv_x_ratio, uint8_t uv_y_ratio) const -> YUV;
  auto crop(uint16_t x_offset, uint16_t y_offset, uint16_t width, uint16_t height) const -> RGB;
  auto rotate(Rotation direction) -> RGB;
  auto scale(int num, int denum) const -> RGB { return stretch(num, denum, num, denum); }
  auto stretch(int num_x, int denum_x, int num_y, int denum_y) const -> RGB;
};

}}
