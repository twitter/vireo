//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"

namespace vireo {
namespace security {

const uint32_t kMaxDimension = 0x2000;
const uint32_t kMaxGOPSize = 0x200;
const uint32_t kMaxHeaderSize = 0x1000;
const uint32_t kMaxSampleCount = 0x40000;
const uint32_t kMaxSampleSize = 0x400000;
const uint32_t kMaxWriteSize = 0xA000000;

static inline bool valid_dimensions(uint16_t width, uint16_t height) {
  if(width < 1 || height < 1) {
    return false;
  }
  if(width > kMaxDimension || height > kMaxDimension) {
    return false;
  }
  return true;
}

}}
