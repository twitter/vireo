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
namespace functional {

template <typename ReturnType, typename... ArgType>
class Function : public std::function<ReturnType(ArgType...)> {
public:
  Function() : std::function<ReturnType(ArgType...)>(nullptr) {}
  Function(std::function<ReturnType(ArgType...)> f) : std::function<ReturnType(ArgType...)>(f) {}
  template <typename OldReturnType>
  Function(Function<OldReturnType(ArgType...)> g, std::function<ReturnType(OldReturnType)> f)
    : std::function<ReturnType(ArgType...)>([f, g](ArgType... args) -> ReturnType {
      return f(g(args...));
    }) {}
};

}}
