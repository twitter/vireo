//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"
#include "vireo/common/data.h"
#include "vireo/internal/decode/types.h"
#include "vireo/internal/decode/util.h"

namespace vireo {
namespace internal {
namespace decode {

template <typename NalType> struct _ANNEXB;

template <typename NalType>
class PUBLIC ANNEXB final : public domain::Interval<ANNEXB<NalType>, NalInfo<NalType>, uint32_t> {
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
