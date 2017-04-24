//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

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
