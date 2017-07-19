//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#include "vireo/common/enum.hpp"
#include "vireo/error/error.h"
#include "vireo/internal/decode/avcc.h"
#include "vireo/internal/decode/types.h"

const static uint8_t kAnnexBStartCodeSize = 4;

namespace vireo {
namespace internal {
namespace decode {

template <>
struct _AVCC<H264NalType> {
  common::Data32 data;
  uint8_t nalu_length_size;
  vector<NalInfo<H264NalType>> nal_infos;
  bool initialized = false;
  _AVCC(const common::Data32& data, uint8_t nalu_length_size)
    : data(move(common::Data32(data.data() + data.a(), data.count(), nullptr))), nalu_length_size(nalu_length_size) {}
  auto nalu_size(common::Data32& data) -> uint32_t {
    common::Data32 _data = common::Data32(data.data() + data.a(), data.count(), nullptr);
    CHECK(_data.count() >= nalu_length_size);
    uint32_t size = 0;
    for (int32_t i = nalu_length_size - 1; i >= 0; i--) {
      size |= _data(i) << ((nalu_length_size - 1 - i) * CHAR_BIT);
    }
    return size;
  };

  auto next_nal() -> NalInfo<H264NalType> {
    uint32_t a = data.a();
    NalInfo<H264NalType> info;
    uint32_t nal_size = nalu_size(data);
    data.set_bounds(data.a() + nalu_length_size, data.b());
    uint8_t type = (data(data.a()) & 0x1F);
    info.type = H264NalType::Unknown;
    for (auto nal_type: enumeration::Enum<H264NalType>(H264NalType::FRM, H264NalType::FLLR)) {
      if (nal_type == type) {
        info.type = nal_type;
        break;
      }
    }
    THROW_IF(info.type == H264NalType::EOFL, Unsupported);  // not tested
    info.byte_offset = data.a();
    info.size = nal_size;
    info.start_code_prefix_size = nalu_length_size;
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
AVCC<H264NalType>::AVCC(const common::Data32& data, uint8_t nalu_length_size)
  : _this(make_shared<_AVCC<H264NalType>>(data, nalu_length_size)) {
  if (_this->finish_initialization()) {
    set_bounds(0, (uint32_t)_this->nal_infos.size());
    _this->initialized = true;
  } else {
    THROW_IF(true, Uninitialized);
  }
}

template <>
auto AVCC<H264NalType>::operator()(uint32_t index) const -> NalInfo<H264NalType> {
  THROW_IF(!_this->initialized, Invalid);
  THROW_IF(index < 0, OutOfRange);
  THROW_IF(index >= _this->nal_infos.size(), OutOfRange);
  NalInfo<H264NalType> info = _this->nal_infos[index];
  return info;
}

auto avcc_to_annexb(const common::Data32& data, uint8_t nalu_length_size) -> common::Data32 {
  common::Data32 _data = common::Data32(data.data() + data.a(), data.count(), nullptr);
  AVCC<H264NalType> avcc_parser(_data, nalu_length_size);
  uint32_t out_size = 0;
  for (const auto& nal_info: avcc_parser) {
    out_size += kAnnexBStartCodeSize + nal_info.size;
  }

  common::Data32 out = common::Data32(new uint8_t[out_size], out_size, [](uint8_t* p){ delete[] p; });
  for (const auto& nal_info: avcc_parser) {
    write_annexb_startcode(out);
    out.set_bounds(out.a() + kAnnexBStartCodeSize, out.capacity());
    _data.set_bounds(nal_info.byte_offset, nal_info.byte_offset + nal_info.size);
    out.copy(_data);
    out.set_bounds(out.b(), out.capacity());
  }
  out.set_bounds(0, out.capacity());
  return move(out);
}

auto contain_sps_pps(const common::Data32& data, uint8_t nalu_length_size) -> bool {
  common::Data32 _data = common::Data32(data.data() + data.a(), data.count(), nullptr);
  AVCC<H264NalType> avcc_parser(_data, nalu_length_size);
  bool has_sps = false;
  bool has_pps = false;
  for (const auto& nal_info : avcc_parser) {
    if (nal_info.type == H264NalType::SPS) {
      has_sps = true;
    }
    if (nal_info.type == H264NalType::PPS) {
      has_pps = true;
    }
  }
  THROW_IF((has_sps && !has_pps) || (!has_sps && has_pps), Unsupported);
  return has_sps && has_pps;
}

}}}