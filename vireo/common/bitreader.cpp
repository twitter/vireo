//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#include "vireo/base_h.h"
#include "vireo/common/bitreader.h"

namespace vireo {
namespace common {

using namespace std;

auto BitReader::read_bits(uint8_t n) -> uint32_t {
  THROW_IF(!n || n > CHAR_BIT * sizeof(uint32_t), InvalidArguments);  // can only parse 32-bits at a time
  uint32_t value = 0;
  while (n) {
    THROW_IF(!data.count(), Unsafe);
    CHECK(bit_offset < CHAR_BIT);
    uint8_t bits_to_read = min((uint32_t)(CHAR_BIT - bit_offset), (uint32_t)n);
    value = value << bits_to_read;  // make space for the newly read bits

    uint8_t unread_bits = data(data.a()) << bit_offset;  // flush out already read bits (now bits of interest are the most-significant bits)
    unread_bits = unread_bits >> (CHAR_BIT - bits_to_read);  // shift such that bits of interest are the least-significant bits
    value += unread_bits;

    bit_offset += bits_to_read;
    data.set_bounds(data.a() + bit_offset / CHAR_BIT, data.b());
    bit_offset %= CHAR_BIT;
    n -= bits_to_read;
  }
  return value;
}

auto BitReader::remaining() -> uint64_t {
  return (uint64_t)data.count() * CHAR_BIT + (CHAR_BIT - bit_offset);
}

}}