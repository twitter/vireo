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
};

auto annexb_to_avcc(common::Data32& data, uint8_t nalu_length_size) -> void;

}}}
