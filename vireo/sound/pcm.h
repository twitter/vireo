//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"
#include "vireo/common/data.h"
#include "vireo/domain/interval.hpp"

namespace vireo {
namespace sound {

class PCM {
  struct _PCM* _this;
public:
  PCM(uint16_t size, uint8_t channels, common::Sample16&& samples);
  PCM(PCM&& sound);
  PCM(const PCM& sound);
  virtual ~PCM();
  DISALLOW_ASSIGN(PCM);
  auto size() const -> uint16_t;
  auto channels() const -> uint8_t;
  auto samples() const -> common::Sample16&;

  // Transform
  auto mix(uint8_t channels) const -> PCM;  // Mix to 1 channel supported (from 2 channels)
  auto downsample(uint8_t factor) const -> PCM;  // Downsample from 2048 to 1024 sample size
};

}}
