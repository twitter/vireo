//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/common/data.h"

namespace vireo {
namespace common {

class PUBLIC BitReader final {
  common::Data32 data;
  uint32_t bit_offset = 0;
public:
  BitReader(common::Data32&& data) : data(move(data)), bit_offset(0) {};
  auto read_bits(uint8_t n) -> uint32_t;
  auto remaining() -> uint64_t;
  DISALLOW_COPY_AND_ASSIGN(BitReader);
};

}}
