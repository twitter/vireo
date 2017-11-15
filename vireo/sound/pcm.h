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
