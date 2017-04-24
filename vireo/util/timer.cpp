//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#include <chrono>
#include "vireo/base_h.h"
#include "vireo/util/timer.h"

namespace vireo {
namespace util {

using namespace std;

auto Timer::mark() -> uint64_t {
  uint64_t diff = 0;
  if (initialized) {
    auto now = std::chrono::high_resolution_clock::now();
    diff = std::chrono::duration_cast<std::chrono::nanoseconds>(now - start).count();
    _total += diff;
  }
  reset();
  return diff;
}

auto Timer::now() -> void {
  uint64_t diff = mark();
  cout << "Elapsed time = " << diff << " nsecs" << endl;
}

auto Timer::reset() -> void {
  start = std::chrono::high_resolution_clock::now();
  initialized = true;
}

auto Timer::total() -> void {
  cout << "Total time = " << _total / (1000.f * 1000.0f) << " msecs" << endl;
}

}}