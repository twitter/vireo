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

#include "vireo/common/enum.hpp"
#include "vireo/error/error.h"
#include "vireo/internal/decode/annexb.h"
#include "vireo/internal/decode/types.h"

namespace vireo {
namespace internal {
namespace decode {

struct AnnexB {
  uint8_t start_code_prefix_size;
  uint32_t nal_size;
};

template <>
auto ANNEXB<H264NalType>::GetNalType(const common::Data32 &data) -> H264NalType {
  if (data.count()) {
    uint8_t type = (data(data.a()) & 0x1F);
    for (auto nal_type: enumeration::Enum<H264NalType>(H264NalType::FRM, H264NalType::FLLR)) {
      if (nal_type == type) {
        return nal_type;
      }
    }
  }
  return H264NalType::Unknown;
}

template <>
struct _ANNEXB<H264NalType> {
  common::Data32 data;
  vector<NalInfo<H264NalType>> nal_infos;
  _ANNEXB(const common::Data32& data) : data(move(common::Data32(data.data() + data.a(), data.count(), nullptr))) {}
  bool initialized = false;

  static auto NalSize(const common::Data32& data) -> uint32_t {
    common::Data32 _data = common::Data32(data.data() + data.a(), data.count(), nullptr);
    uint32_t size = 0;
    while (_data.count()) {
      if (ANNEXB<H264NalType>::StartCodePrefixSize(_data)) {
        break;
      } else {
        ++size;
        _data.set_bounds(_data.a() + 1, _data.b());
      }
    }
    return size;
  };

  static auto NextAnnexB(const common::Data32& data) -> AnnexB {
    common::Data32 _data = common::Data32(data.data() + data.a(), data.count(), nullptr);
    AnnexB annexb;
    annexb.start_code_prefix_size = ANNEXB<H264NalType>::StartCodePrefixSize(_data);
    CHECK(annexb.start_code_prefix_size != 0);
    _data.set_bounds(_data.a() + annexb.start_code_prefix_size, _data.b());
    annexb.nal_size = NalSize(_data);
    return annexb;
  }

  auto next_nal() -> NalInfo<H264NalType> {
    uint32_t a = data.a();
    NalInfo<H264NalType> info;
    AnnexB annexb = NextAnnexB(data);
    data.set_bounds(data.a() + annexb.start_code_prefix_size, data.b());
    info.type = ANNEXB<H264NalType>::GetNalType(data);
    THROW_IF(info.type == H264NalType::EOFL, Unsupported);  // not tested
    info.byte_offset = data.a();
    info.size = annexb.nal_size;
    info.start_code_prefix_size = annexb.start_code_prefix_size;
    data.set_bounds(a, data.b());
    return info;
  };

  bool finish_initialization() {
    while (data.count()) {
      NalInfo<H264NalType> info = next_nal();
      nal_infos.push_back(info);
      data.set_bounds(data.a() + info.start_code_prefix_size + info.size, data.b());
    }
    return true;
  }
};

template <>
ANNEXB<H264NalType>::ANNEXB(const common::Data32& data)
  : _this(make_shared<_ANNEXB<H264NalType>>(data)) {
  if (_this->finish_initialization()) {
    set_bounds(0, (uint32_t)_this->nal_infos.size());
    _this->initialized = true;
  } else {
    THROW_IF(true, Uninitialized);
  }
}

template <>
auto ANNEXB<H264NalType>::operator()(uint32_t index) const -> NalInfo<H264NalType> {
  THROW_IF(!_this->initialized, Invalid);
  THROW_IF(index <  0, OutOfRange);
  THROW_IF(index >= _this->nal_infos.size(), OutOfRange);
  NalInfo<H264NalType> info = _this->nal_infos[index];
  return info;
}

auto annexb_to_avcc(common::Data32& data, uint8_t nalu_length_size) -> void {
  THROW_IF(nalu_length_size != 4, Unsupported);

  common::Data32 _data = common::Data32(data.data() + data.a(), data.count(), nullptr);
  ANNEXB<H264NalType> annexb_parser(_data);
  uint32_t out_size = 0;
  for (const auto& nal_info: annexb_parser) {
    out_size += nalu_length_size + nal_info.size;
  }

  if (out_size == data.count()) {  // inline conversion possible
    for (const auto& nal_info: annexb_parser) {
      _data.set_bounds(nal_info.byte_offset - nalu_length_size, _data.b());
      auto start_code_prefix_size = ANNEXB<H264NalType>::StartCodePrefixSize(_data);
      THROW_IF(start_code_prefix_size != nalu_length_size, Invalid);
      common::util::WriteNalSize(_data, nal_info.size, nalu_length_size);
    }
  } else {
    common::Data32 out = common::Data32(new uint8_t[out_size], out_size, [](uint8_t* p){ delete[] p; }); 
    for (const auto& nal_info: annexb_parser) {
      common::util::WriteNalSize(out, nal_info.size, nalu_length_size);
      out.set_bounds(out.a() + nalu_length_size, out.capacity());
      _data.set_bounds(nal_info.byte_offset, nal_info.byte_offset + nal_info.size);
      out.copy(_data);
      out.set_bounds(out.b(), out.capacity());
    }
    out.set_bounds(0, out.capacity());
    data = move(out);
  }
}

}}}
