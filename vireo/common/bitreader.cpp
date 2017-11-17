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