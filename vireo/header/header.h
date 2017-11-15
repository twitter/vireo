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

namespace vireo {
namespace header {

struct PUBLIC SPS_PPS {
  common::Data16 sps;
  common::Data16 pps;
  uint8_t nalu_length_size;
  SPS_PPS(const common::Data16& sps, const common::Data16& pps, const uint8_t nalu_length_size) noexcept;
  SPS_PPS(const SPS_PPS& sps_pps) noexcept;
  SPS_PPS(SPS_PPS&& sps_pps) noexcept;
  auto operator=(const SPS_PPS& sps_pps) -> void;
  auto operator=(SPS_PPS&& sps_pps) -> void;
  auto operator==(const SPS_PPS& x) const -> bool;
  auto operator!=(const SPS_PPS& x) const -> bool;
  enum ExtraDataType { iso, annex_b, avcc };
  auto as_extradata(ExtraDataType type) const -> common::Data16;
};

}}
