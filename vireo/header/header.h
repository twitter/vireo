//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"
#include "vireo/common/data.h"

namespace vireo {
namespace header {

struct SPS_PPS {
  common::Data16 sps;
  common::Data16 pps;
  uint8_t nalu_length_size;
  SPS_PPS(const common::Data16& sps, const common::Data16& pps, const uint8_t nalu_length_size) noexcept;
  SPS_PPS(const SPS_PPS& sps_pps) noexcept;
  SPS_PPS(SPS_PPS&& sps_pps) noexcept;
  auto operator=(const SPS_PPS& sps_pps) -> void;
  auto operator=(SPS_PPS&& sps_pps) -> void;
  enum ExtraDataType { avcc, annex_b };
  auto as_extradata(ExtraDataType type) const -> common::Data16;
};

}}
