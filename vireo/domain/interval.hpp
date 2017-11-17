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
#include "vireo/base_cpp.h"
#include "vireo/domain/util.h"
#include "vireo/error/error.h"
#include "vireo/functional/function.hpp"

namespace vireo {
namespace domain {

template <typename ObjectType, typename ReturnType, typename ArgType> class Interval;

template <typename ObjectType, typename ReturnType, typename ArgType>
class Iterator {
  const ObjectType& c;
  const vector<ArgType>& mapping;
  ArgType x;
  Iterator(const ObjectType& c, const ArgType& x, const vector<ArgType>& mapping = vector<ArgType>()) : c(c), x(x), mapping(mapping) {}
  friend class Interval<ObjectType, ReturnType, ArgType>;
public:
  auto operator*() const -> ReturnType {
    return c(x);
  }
  auto operator++() -> const Iterator& {
    ++x;
    return *this;
  }
  auto operator++(int k) -> Iterator {
    Iterator copy(*this);
    x += k;
    return copy;
  }
  auto operator==(const Iterator& other) const -> bool {
    return x == other.x;
  }
  auto operator!=(const Iterator& other) const -> bool {
    return x != other.x;
  }
  auto operator-(const Iterator& other) const -> ReturnType {
    return x - other.x;
  }
};

template <typename ObjectType, typename ReturnType, typename ArgType>
struct MaybeDerivesFromFunction {
  virtual ~MaybeDerivesFromFunction() {}
  enum { derives_from_function = 0 };
};

template <typename ReturnType, typename ArgType>
struct MaybeDerivesFromFunction<functional::Function<ReturnType, ArgType>, ReturnType, ArgType> : public functional::Function<ReturnType, ArgType> {
  enum { derives_from_function = 1 };
  MaybeDerivesFromFunction<functional::Function<ReturnType, ArgType>, ReturnType, ArgType>(const std::function<ReturnType(ArgType)>& f)
    : functional::Function<ReturnType, ArgType>(f) {}
  MaybeDerivesFromFunction<functional::Function<ReturnType, ArgType>, ReturnType, ArgType>()
    : functional::Function<ReturnType, ArgType>(std::function<ReturnType(ArgType)>(nullptr)) {}
};

template <typename ObjectType, typename ReturnType, typename ArgType>
class Interval : public MaybeDerivesFromFunction<ObjectType, ReturnType, ArgType> {
  ArgType _a;
  ArgType _b;

  typedef functional::Function<ReturnType, ArgType> FunctionType;
  typedef Interval<FunctionType, ReturnType, ArgType> IntervalFunctionType;

public:
  Interval() : _a(0), _b(0) {}
  Interval(const ArgType& a, const ArgType& b) : _a(a), _b(b) {}

  Interval(const std::function<ReturnType(ArgType)>& f, const ArgType& a, const ArgType& b)
    : MaybeDerivesFromFunction<FunctionType, ReturnType, ArgType>(f), _a(a), _b(b) {}

  // Convert vector into function interval
  Interval(const vector<ReturnType>& v)
    : MaybeDerivesFromFunction<FunctionType, ReturnType, ArgType>([v](ArgType arg) -> ReturnType {
        return v[arg];
      }), _a(0), _b((uint32_t)v.size()) {
    THROW_IF(v.size() >= std::numeric_limits<ArgType>::max(), Unsafe);
  }

  // Convert non function intervals into function intervals
  template <typename T,
            typename std::enable_if<!is_vector<T>::value && !std::is_base_of<FunctionType, T>::value>::type* = nullptr>  // Interval in argument is not a vector or function
  Interval(const T& t)
    : MaybeDerivesFromFunction<FunctionType, ReturnType, ArgType>([t](ArgType arg) -> ReturnType {
        return t(arg);
      }), _a(t.a()), _b(t.b()) {
  }

  // Converts one interval into another
  template <typename T, typename LambdaTransform,
            typename std::enable_if<std::is_class<T>::value && !is_vector<T>::value>::type* = nullptr>  // Interval in argument is not a vector
  Interval(const T& t, LambdaTransform transform)
    : MaybeDerivesFromFunction<FunctionType, ReturnType, ArgType>(
        [t, transform](ArgType arg) -> ReturnType {
          return transform(t(arg));
        }
      ),
      _a(t.a()), _b(t.b()) {
  }
  template <typename InputReturnType, typename LambdaTransform,
            typename std::enable_if<!std::is_same<InputReturnType, ReturnType>::value>::type* = nullptr>
  Interval(const vector<InputReturnType>& v, LambdaTransform transform)
    : MaybeDerivesFromFunction<FunctionType, ReturnType, ArgType>(
        [v, transform](ArgType arg) -> ReturnType {
          return transform(v[arg]);
        }
      ),
      _a(0), _b((uint32_t)v.size()) {
  }

  // Converts 2 intervals into another
  template <typename T1, typename T2, typename LambdaTransform>  // Current interval is a function
  Interval(const T1& t1, const T2& t2, LambdaTransform transform)
    : MaybeDerivesFromFunction<FunctionType, ReturnType, ArgType>(
        [t1, t2, transform](ArgType arg) -> ReturnType {
          return transform(t1(arg), t2(arg));
        }
      ),
     _a(std::min(t1.a(), t2.a())), _b(std::min(t1.b(), t2.b())) {
  }
  template <typename T1, typename T2, typename OtherReturnType1, typename OtherReturnType2>  // Current interval is a function
  Interval(const T1& t1, const T2& t2, const std::function<ReturnType(OtherReturnType1, OtherReturnType2)>& g)
    : MaybeDerivesFromFunction<FunctionType, ReturnType, ArgType>(
        [t1, t2, g](ArgType arg) -> ReturnType {
          return g(t1(arg), t2(arg));
        }
      ),
     _a(std::min(t1.a(), t2.a())), _b(std::min(t1.b(), t2.b())) {
  }

  auto begin() const -> Iterator<ObjectType, ReturnType, ArgType> { return Iterator<ObjectType, ReturnType, ArgType>(*static_cast<const ObjectType*>(this), _a); }
  auto end() const -> Iterator<ObjectType, ReturnType, ArgType> { return Iterator<ObjectType, ReturnType, ArgType>(*static_cast<const ObjectType*>(this), _b); }
  auto a() const -> ArgType { return _a; }
  auto b() const -> ArgType { return _b; }
  auto set_bounds(const ArgType& a, const ArgType& b) -> void { _a = std::min(a, b); _b = b; }
  auto count() const -> ArgType { return _b - _a; }
};

}}
