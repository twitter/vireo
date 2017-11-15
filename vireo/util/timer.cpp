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