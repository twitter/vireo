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
#include "vireo/common/util.h"
#include "vireo/internal/decode/types.h"

namespace vireo {
namespace internal {
namespace decode {

template <typename NalType> struct _AVCC;

template <typename NalType>
class AVCC final : public domain::Interval<AVCC<NalType>, NalInfo<NalType>, uint32_t> {
  std::shared_ptr<_AVCC<NalType>> _this = nullptr;
public:
  AVCC(const common::Data32& data, uint8_t nalu_length_size);
  DISALLOW_COPY_AND_ASSIGN(AVCC);
  auto operator()(uint32_t index) const -> NalInfo<NalType>;
};

auto avcc_to_annexb(const common::Data32& data, uint8_t nalu_length_size) -> common::Data32;
auto contain_sps_pps(const common::Data32& data, uint8_t nalu_length_size) -> bool;

}}}
