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
  auto rgb(uint8_t component_count) -> RGB;
  auto full_range(bool full_range) -> YUV;
  auto crop(uint16_t x_offset, uint16_t y_offset, uint16_t width, uint16_t height) const -> YUV;
  auto rotate(Rotation direction) -> YUV;
  auto scale(int num, int denum) -> YUV { return stretch(num, denum, num, denum); }
  auto stretch(int num_x, int denum_x, int num_y, int denum_y, bool high_quality = true) -> YUV;
};

}}
