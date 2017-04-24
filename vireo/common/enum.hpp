//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

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
