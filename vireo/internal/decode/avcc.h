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

template <typename NalType> struct _AVCC;

template <typename NalType>
class PUBLIC AVCC final : public domain::Interval<AVCC<NalType>, NalInfo<NalType>, uint32_t> {
  std::shared_ptr<_AVCC<NalType>> _this = nullptr;
public:
  AVCC(const common::Data32& data, uint8_t nalu_length_size);
  DISALLOW_COPY_AND_ASSIGN(AVCC);
  auto operator()(uint32_t index) const -> NalInfo<NalType>;
};

auto avcc_to_annexb(const common::Data32& data, uint8_t nalu_length_size) -> common::Data32;

}}}
