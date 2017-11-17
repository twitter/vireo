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

template <typename NalType> struct _ANNEXB;

template <typename NalType>
class ANNEXB final : public domain::Interval<ANNEXB<NalType>, NalInfo<NalType>, uint32_t> {
  std::shared_ptr<_ANNEXB<NalType>> _this = nullptr;
public:
  ANNEXB(const common::Data32& data);
  DISALLOW_COPY_AND_ASSIGN(ANNEXB);
  auto operator()(uint32_t index) const -> NalInfo<NalType>;
  static auto GetNalType(const common::Data32& data) -> NalType;
  static auto StartCodePrefixSize(const common::Data32& data) -> uint8_t {
    if (data.count() >= 4 && data(data.a()) == 0x00 && data(data.a() + 1) == 0x00 && data(data.a() + 2) == 0x00 && data(data.a() + 3) == 0x01) {  // 4-byte start code prefix
      return 4;
    } else if (data.count() >= 3 && data(data.a()) == 0x00 && data(data.a() + 1) == 0x00 && data(data.a() + 2) == 0x01) {  // 3-byte start code prefix
      return 3;
    } else {
      return 0;
    }
  };
};

auto annexb_to_avcc(common::Data32& data, uint8_t nalu_length_size) -> void;

}}}
