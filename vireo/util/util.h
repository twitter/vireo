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

#pragma once

#include <iomanip>
#include <fstream>

#include "vireo/common/data.h"

namespace vireo {
namespace util {

template <class V>
static inline V** get_addr(const V& ptr) {
  return (V**)&(const V*&)ptr;
}

template <typename Y, typename X>
static inline void print(const common::Data<Y, X>& data, uint32_t line_width = 64) {
  std::ios::fmtflags f(cout.flags());
  cout << std::uppercase << std::hex;
  for (X i = data.a(), cnt = 1; i < data.b(); ++i, ++cnt) {
    cout << std::right << std::setfill('0') << std::setw(2 * sizeof(Y)) << (uint64_t)data.data()[i] << " ";
    if ((cnt % line_width) == 0) {
      cout << endl;
    }
  }
  cout << endl << endl;
  cout.flags(f);
}

static inline void save(const string filename, const common::Data32& data) {
  std::ofstream ostream(filename.c_str(), std::ofstream::out | std::ofstream::binary);
  ostream << data;
  ostream.close();
}

}}
