//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

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
