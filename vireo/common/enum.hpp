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

#include "vireo/base_h.h"

namespace vireo {
namespace enumeration {

template <typename E> class Enum;

template <typename E>
class Iterator {
  int x;
  Iterator(const int x) : x(x) {}
  friend class Enum<E>;
public:
  auto operator*() const -> E {
    return static_cast<E>(x);
  }
  auto operator++() -> const Iterator& {
    ++x;
    return *this;
  }
  auto operator!=(const Iterator& other) const -> bool {
    return x != other.x;
  }
};

template <typename E>
class Enum {
  Iterator<E> _a;
  Iterator<E> _b;
public:
  Enum(E a, E b) : _a(static_cast<int>(a)), _b(static_cast<int>(b) + 1) {}
  auto begin() const -> Iterator<E> { return _a; }
  auto end() const -> Iterator<E> { return _b; }
};

}}
