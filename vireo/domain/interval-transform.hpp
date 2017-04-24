//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"
#include "vireo/functional/function.hpp"
#include "vireo/domain/interval.hpp"
#include "vireo/error/error.h"

namespace vireo {
namespace domain {

template <typename InputObjectType, typename InputReturnType, typename ArgType,
          typename OutputReturnType, typename TransformFunction, IS_DIFFERENT(InputReturnType, OutputReturnType)>
auto transform(const interval<InputObjectType, ArgType, InputReturnType>& interval, const TransformFunction& transform) -> interval<Function<OutputReturnType, ArgType>, ArgType, OutputReturnType> {
  return interval<Function<OutputReturnType, ArgType>, ArgType, OutputReturnType>([interval, transform](ArgType arg) -> ReturnType {
    return transform((*static_cast<const InputObjectType*>(&interval))(arg));
  }, interval.a(), interval.b());
}

}}
