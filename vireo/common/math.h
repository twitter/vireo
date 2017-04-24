//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include <limits>
#include <math.h>
#include <vector>

#include "vireo/base_h.h"
#include "vireo/error/error.h"

namespace vireo {
namespace common {

using namespace std;

template <class T>
T mean(vector<T> values) {
  T acc = 0;
  for (auto v: values) {
    acc += v;
  }
  return acc / values.size();
}

template <class T>
double variance(vector<T> values) {
  T avg = mean(values);
  double acc = 0;
  for (auto v: values) {
    acc += v * v;
  }
  return (acc / values.size()) - avg * avg;
}

template <class T>
double std_dev(vector<T> values) {
  return sqrt(variance(values));
}

template <class T>
static inline typename std::enable_if<std::is_unsigned<T>::value & std::is_integral<T>::value, T>::type safe_umul(T x, T y) {
  THROW_IF(y && x > numeric_limits<T>::max() / y, Overflow);
  return x * y;
}

template <class T>
static inline T round_divide(T x, T num, T denom) {
  return (safe_umul(x, num) + (denom / 2)) / denom;
};

template <class T>
static inline T ceil_divide(T x, T num, T denom) {
  return (safe_umul(x, num) + (denom - 1)) / denom;
};

template <class T>
static inline T align_shift(T x, int shift) {
  return ((x + (1 << shift) - 1) >> shift) << shift;
};

template <class T>
static inline T align_divide(T x, T denom) {
  return ((x + (denom - 1)) / denom) * denom;
};

template <class T>
static T median(vector<T> values) {
  auto size = values.size();
  sort(values.begin(), values.end());
  auto mid = size / 2;
  return size ? ((size % 2 == 0) ? (values[mid - 1] + values[mid]) / 2 : values[mid]) : 0;
};

template <class T>
static inline T even(T x) {
  return x + (x & 0x01);
}

template <class T>
static inline T even_floor(T x) {
  return x - (x & 0x01);
}

}}
