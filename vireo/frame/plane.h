//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

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
