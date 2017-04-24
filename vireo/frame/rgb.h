//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"
#include "vireo/common/data.h"
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
  auto rgb(uint8_t component_count) const -> RGB;
  auto yuv(uint8_t uv_x_ratio, uint8_t uv_y_ratio) const -> YUV;
  auto crop(uint16_t x_offset, uint16_t y_offset, uint16_t width, uint16_t height) const -> RGB;
  auto rotate(Rotation direction) -> RGB;
  auto scale(int num, int denum) const -> RGB { return stretch(num, denum, num, denum); }
  auto stretch(int num_x, int denum_x, int num_y, int denum_y) const -> RGB;
};

}}
