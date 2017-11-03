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

#include "vireo/base_cpp.h"
#include "vireo/error/error.h"
#include "vireo/util/ftyp.h"

namespace vireo {
namespace util {

auto FtypUtil::Matches(const vector<uint8_t>& ftyp, const common::Data32& data) -> bool {
  if (data.count() < ftyp.size()) {
    return false;
  }
  uint32_t index = 0;
  for (const auto byte: ftyp) {
    if (byte != data(index++)) {
      return false;
    }
  }
  return true;
};

auto FtypUtil::Matches(const vector<vector<uint8_t>>& ftyps, const common::Data32& data) -> bool {
  for (auto ftyp: ftyps) {
    if (Matches(ftyp, data)) {
      return true;
    }
  }
  return false;
};

}}