//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"
#include "vireo/common/data.h"

namespace vireo {
namespace internal {
namespace decode {

static inline void write_nal_size(common::Data32& data, uint32_t nal_size, uint8_t nalu_length_size) {
  THROW_IF(data.count() < nalu_length_size, Invalid);
  uint8_t* bytes = (uint8_t*)data.data() + data.a();
  for (auto i = 0; i < nalu_length_size; ++i) {
    bytes[i] = (nal_size >> (CHAR_BIT * sizeof(uint8_t) * (nalu_length_size - 1 - i))) & 0xFF;
  }
};

static inline void write_annexb_startcode(common::Data32& data) {
  THROW_IF(data.count() <= 4, Invalid);
  uint8_t* bytes = (uint8_t*)data.data() + data.a();
  bytes[0] = 0x00;
  bytes[1] = 0x00;
  bytes[2] = 0x00;
  bytes[3] = 0x01;
};

}}}
