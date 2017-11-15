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

#include "imagecore/imagecore.h"
#include "vireo/base_cpp.h"
#include "vireo/common/enum.hpp"
#include "vireo/common/ref.h"
#include "vireo/error/error.h"
#include "vireo/frame/plane.h"

namespace vireo {

CONSTRUCTOR static void _Init() {
  RegisterImageCoreAssertionHandler(&error::ImageCoreHandler);
}

namespace frame {

struct _Plane {
  uint16_t row;
  uint16_t width;
  uint16_t height;
  common::Data32 bytes;
  _Plane(uint16_t row, uint16_t width, uint16_t height, common::Data32&& bytes)
    : row(row), width(width), height(height), bytes(move(bytes)) {}
};

Plane::Plane(uint16_t row, uint16_t width, uint16_t height, common::Data32&& bytes)
  : domain::Interval<Plane, common::Data16, uint16_t>(0, height) {
  _this = new _Plane(row, width, height, move(bytes));
}

Plane::Plane(Plane&& plane)
  : Plane{plane._this->row, plane.width(), plane.height(), move(plane._this->bytes)} {
}

Plane::~Plane() {
  delete _this;
}

auto Plane::operator()(const uint16_t& y) const -> common::Data16 {
  return common::Data16(_this->bytes.data() + y * _this->row, width(), NULL);
}

auto Plane::row() const -> uint16_t {
  return _this->row;
}

auto Plane::width() const -> uint16_t {
  return _this->width;
}

auto Plane::height() const -> uint16_t {
  return _this->height;
}

auto Plane::alignment() const -> uint8_t {
  for (int i = 7; i >= 0; --i) {
    if (_this->row % (1 << i) == 0) {
      return (1 << i);
    }
  }
  return 1;
}

auto Plane::bytes() const -> common::Data32& {
  return _this->bytes;
}

}}
